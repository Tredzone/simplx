/**
 * @file platform_gcc.cpp
 * @brief gcc-specific OS wrapper
 * @copyright 2013-2019 Tredzone (www.tredzone.com). All rights reserved.
 * Please see accompanying LICENSE file for licensing terms.
 */

#include <iostream>
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <iomanip>
#include <cstdio>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <array>


#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

/*

for clang must:

apt install libc++abi-dev

*/

#include <cxxabi.h>

#include "trz/engine/internal/thread.h"

using namespace std;

#if (!defined(NDEBUG) && !defined(__clang__))
/**
 * cf: http://linux.die.net/man/3/backtrace_symbols
 */
vector<string> tredzone::debugBacktrace(const uint8_t stackTraceSize) noexcept
{
    // void *buffer[stackTraceSize];						// variable length array is VERBOTEN!
    vector<void *> buffer(stackTraceSize, nullptr);
    char **trace;
    vector<string> ret;

    try
    {
        int32_t returnedStackTraceSize = backtrace(&buffer[0], stackTraceSize);
        assert(returnedStackTraceSize != 0);
        
        trace = backtrace_symbols(&buffer[0], returnedStackTraceSize);
        assert(trace != 0);

        ret.reserve(stackTraceSize);
        for (int32_t j = 0; j < returnedStackTraceSize; ++j)
        {
            ret.push_back(demangleFromSymbolName(trace[j]));
        }
        
        free(trace);
    }
    catch (...)
    {
    }
    
    return ret;
}

/**
 * cf: https://panthema.net/2008/0901-stacktrace-demangled/
 */
string tredzone::demangleFromSymbolName(char *symbolName) noexcept
{
    // allocate string which will be filled with the demangled function name
    size_t funcnamesize = 256;
    char *funcname = (char *)alloca(funcnamesize);
    stringstream ss;

    try
    {
        // iterate over the returned symbol lines. skip the first, it is the address of this function.
        char *begin_name = 0, *begin_offset = 0, *end_offset = 0;

        // find parentheses and +address offset surrounding the mangled name: ./module(function+0x15c) [0x8048a6d]
        for (char *p = symbolName; *p; ++p)
        {
            if (*p == '(')
            {
                begin_name = p;
            }
            else if (*p == '+')
            {
                begin_offset = p;
            }
            else if (*p == ')' && begin_offset)
            {
                end_offset = p;
                break;
            }
        }

        if (begin_name && begin_offset && end_offset && begin_name < begin_offset)
        {
            *begin_name++ = '\0';
            *begin_offset++ = '\0';
            *end_offset = '\0';

            // mangled name is now in [begin_name, begin_offset) and caller
            // offset in [begin_offset, end_offset). now apply __cxa_demangle():

            int status;
            char *ret = abi::__cxa_demangle(begin_name, funcname, &funcnamesize, &status);
            if (status == 0)
            {
                funcname = ret; // use possibly realloc()-ed string
                ss << symbolName << ": " << funcname << "+" << begin_offset;
            }
            else
            {
                // demangling failed. Output function name as a C function with no arguments.
                ss << symbolName << ": " << begin_name << "()+" << begin_offset;
            }
        }
        else
        {
            // couldn't parse the line? print the whole line.
            ss << symbolName;
        }
    }
    catch (...)
    {
    }

    return ss.str();
}

#else

// RELEASE BUILD and/or clang dummies

vector<string> tredzone::debugBacktrace(const uint8_t) noexcept
{
    return {""};
}

#endif


string tredzone::cppDemangledTypeInfoName(const type_info &typeInfo)
{
    int status = 0;
    char *demangledName = abi::__cxa_demangle(typeInfo.name(), 0, 0, &status);
    if (!demangledName)
    {   // couldn't resolve
        return typeInfo.name();
    }
    try
    {
        string ret(demangledName);
        
        free(demangledName);
        
        const string  stripped = "tredzone::";
        
        size_t ind;
        
        while (ind = ret.find(stripped), ind != string::npos)
        {
            ret.replace(ind, stripped.length(), "");
        }
        
        return ret;
    }
    catch (...)
    {
        throw;
    }
}

size_t tredzone::cpuGetCount()
{
    long ret;
    if ((ret = sysconf(_SC_NPROCESSORS_ONLN)) == -1)
    {
        throw RunTimeException(__FILE__, __LINE__, systemErrorToString(errno));
    }
    return (size_t)ret;
}

tredzone::thread_t tredzone::threadCreate(void (*fn)(void *), void *param, size_t stackSizeBytes)
{
    struct Callback
    {
        void (*fn)(void *);
        void *param;
        Callback(void (*f)(void *), void *p) : fn(f), param(p) {}
        static void *threadProc(void *callback)
        {
            Callback local = *((Callback *)callback);
            delete (Callback *)callback;
            local.fn(local.param);
            return 0;
        }
    };

    pthread_attr_t tattr;
    if (pthread_attr_init(&tattr))
    {
        throw RunTimeException(__FILE__, __LINE__);
    }
    if (pthread_attr_setdetachstate(&tattr, PTHREAD_CREATE_DETACHED))
    {
        throw RunTimeException(__FILE__, __LINE__);
    }
    if (stackSizeBytes > 0)
    {
        if (pthread_attr_setstacksize(&tattr, stackSizeBytes))
        {
            throw RunTimeException(__FILE__, __LINE__);
        }
    }

    Callback *callback = new Callback(fn, param);
    try
    {
        pthread_t pthread;
        if (pthread_create(&pthread, &tattr, Callback::threadProc, callback))
        {
            throw RunTimeException(__FILE__, __LINE__);
        }
        return pthread;
    }
    catch (...)
    {
        delete callback;
        throw;
    }
}

void tredzone::threadSetAffinity(unsigned cpuIndex)
{
    cpuset_type cpuset;
    if (cpuIndex >= cpuset.size())
    {
        throw RunTimeException(__FILE__, __LINE__);
    }
    cpuset[cpuIndex] = true;
    threadSetAffinity(cpuset);
}

void tredzone::threadSetAffinity(const cpuset_type &pcpuset)
{
    int cc;
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    for (size_t i = 0; i < pcpuset.size(); ++i)
    {
        if (i >= CPU_SETSIZE)
        {
            throw RunTimeException(__FILE__, __LINE__);
        }
        if (pcpuset[i])
        {
            CPU_SET((int)i, &cpuset);
        }
    }
    if ((cc = pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset)) != 0)
    {
        throw RunTimeException(__FILE__, __LINE__, systemErrorToString(cc));
    }
}

tredzone::cpuset_type tredzone::threadGetAffinity(void)
{
    int cc;
    cpu_set_t cpuset;
    if ((cc = pthread_getaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset)) != 0)
    {
        throw RunTimeException(__FILE__, __LINE__, systemErrorToString(cc));
    }
    cpuset_type ret;
    for (size_t i = 0; i < CPU_SETSIZE; ++i)
    {
        if (CPU_ISSET((int)i, &cpuset))
        {
            if (i >= ret.size())
            {
                throw RunTimeException(__FILE__, __LINE__);
            }
            ret[i] = true;
        }
    }
    return ret;
}

void tredzone::threadSetRealTime(bool rt, const ThreadRealTimeParam &param)
{
    int cc;
    struct sched_param schedParam;
    memset(&schedParam, 0, sizeof(schedParam));
    if (rt)
    {
        if (param.sched_priority == -1)
        {
            if ((schedParam.sched_priority = sched_get_priority_max(SCHED_FIFO)) == -1)
            {
                throw RunTimeException(__FILE__, __LINE__, systemErrorToString(errno));
            }
        }
        else
        {
            schedParam.sched_priority = param.sched_priority;
        }
        if ((cc = pthread_setschedparam(pthread_self(), SCHED_FIFO, &schedParam)) != 0)
        {
            throw RunTimeException(__FILE__, __LINE__, systemErrorToString(cc));
        }
    }
    else
    {
        schedParam.sched_priority = 0;
        if ((cc = pthread_setschedparam(pthread_self(), SCHED_OTHER, &schedParam)) != 0)
        {
            throw RunTimeException(__FILE__, __LINE__, systemErrorToString(cc));
        }
    }
}

void tredzone::threadSleep(const Time &delay)
{
    const Time t = (delay != Time() ? delay : Time::Millisecond(1));
    struct timespec req = {t.extractSeconds(), (long)t.extractNanoseconds()};
    int cc;
    if ((cc = clock_nanosleep(CLOCK_MONOTONIC, 0, &req, 0)) != 0 && cc != EINTR)
    {
        throw RunTimeException(__FILE__, __LINE__, systemErrorToString(cc));
    }
}

tredzone::tls_t tredzone::tlsCreate()
{
    tls_t key;
    if (pthread_key_create(&key, 0))
    {
        throw RunTimeException(__FILE__, __LINE__);
    }
    return key;
}

void tredzone::tlsDestroy(tls_t key)
{
    if (pthread_key_delete(key))
    {
        throw RunTimeException(__FILE__, __LINE__);
    }
}

//---- Mutex CLASS (not primitive) DTOR ----------------------------------------

    tredzone::Mutex::~Mutex() noexcept
{
#ifndef NDEBUG
    TREDZONE_TRY
    assert(!debugIsLocked());
    mutexDestroy(handle); // throw(tredzone::RunTimeException)
    
    TREDZONE_CATCH_AND_EXIT_FAILURE_WITH_CERR_MESSAGE
#endif
}

// nada mas
