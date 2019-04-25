/**
 * @file platform_gcc.h
 * @brief gcc-specific OS wrapper
 * @copyright 2013-2019 Tredzone (www.tredzone.com). All rights reserved.
 * Please see accompanying LICENSE file for licensing terms.
 */

#pragma once

#include <bitset>
#include <cassert>
#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <vector>
#include <string>
#include <pthread.h>
#include <cstdint>
#include <sys/time.h>
#include <unistd.h>

#ifndef NDEBUG
    #include <execinfo.h>
#endif

#include "trz/engine/internal/linux/platform_linux.h"

#include "trz/engine/internal/dlldecoration.h"      // Windows-only
#include "trz/engine/internal/macro.h"
#include "trz/engine/internal/rtexception.h"
#include "trz/engine/internal/time.h"

#include "trz/engine/internal/parallel/parallel_xplat.h"

// get host endianness from compiler
#ifndef __BYTE_ORDER__
    #error gcc __BYTE_ORDER__ not defined!
#endif

#if (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__)
    // little-endian host
    #define TREDZONE_LITTLE_ENDIAN
#elif (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
    // big-endian host
    #define TREDZONE_BIG_ENDIAN
#else
    #error unsupported endian order!
#endif

namespace tredzone
{
// import into namespace
using std::string;
using std::vector;

typedef pthread_cond_t signal_t;
typedef pthread_key_t tls_t;

vector<string> debugBacktrace(const uint8_t stackTraceSize = 32) noexcept;

string demangleFromSymbolName(char *) noexcept;

// symbols
string cppDemangledTypeInfoName(const std::type_info &);

// system
inline pid_t getPID();
inline size_t systemPageSize();
inline void *alignMalloc(size_t alignement, size_t size);
inline void alignFree(size_t, void *);
template <typename _Type> inline bool atomicCompareAndSwap(_Type *ptr, _Type oldval, _Type newval);
template <typename _Type> inline _Type atomicAddAndFetch(_Type *ptr, unsigned delta);
template <typename _Type> inline _Type atomicSubAndFetch(_Type *ptr, unsigned delta);

// time
inline DateTime timeGetEpoch();

/**
 * @struct HighResolutionTime
 */
struct HighResolutionTime
{
    /**
     * @brief Basic parenthesis operator
     * @note Uses clock_gettime()
     * @return Initialized Time object
     * @throws RunTimeException if clock_gettime() fails
     */
    inline Time operator()()
    {
        struct timespec t;
        if (clock_gettime(CLOCK_MONOTONIC, &t))
        {
            throw RunTimeException(__FILE__, __LINE__);
        }
        return Time((int64_t)t.tv_sec * (int64_t)1000000000 + (int64_t)t.tv_nsec);
    }
};

inline signal_t mutexSignalCreate();
inline void mutexSignalDestroy(signal_t &);
inline void mutexSignalWait(signal_t &, mutex_t &lockedMutex);
inline void mutexSignalWait(signal_t &, mutex_t &lockedMutex, const Time &timeOut, const Time & = timeGetEpoch());
inline void mutexSignalNotify(signal_t &);

// CPUs
typedef std::bitset<1024> cpuset_type;
size_t cpuGetCount();
// thread
struct ThreadRealTimeParam
{
    int sched_priority;
    inline ThreadRealTimeParam() noexcept : sched_priority(-1) {}
};

thread_t threadCreate(void (*)(void *), void *, size_t stackSizeBytes = 0);
cpuset_type threadGetAffinity();
void threadSetAffinity(const cpuset_type &);
void threadSetAffinity(unsigned);
void threadSetRealTime(bool, const ThreadRealTimeParam &);
inline void threadYield() noexcept;
void threadSleep(const Time &delay = Time());
inline thread_t threadCurrent();
inline bool threadEqual(const thread_t &, const thread_t &);

// tls (thread local storage)
tls_t tlsCreate();
void tlsDestroy(tls_t);
inline void *tlsGet(tls_t);
inline void tlsSet(tls_t, void *);

// inlines
pid_t getPID() { return getpid(); }

size_t systemPageSize() { return (size_t)getpagesize(); }

void *alignMalloc(size_t alignement, size_t size)
{
    void *ret;
    int cc;
    if ((cc = posix_memalign(&ret, alignement, size)) != 0)
    {
        if (cc != ENOMEM)
        {
            throw RunTimeException(__FILE__, __LINE__);
        }
        throw std::bad_alloc();
    }
    return ret;
}

void alignFree(size_t, void *p) { ::free(p); }

template <typename _Type> bool atomicCompareAndSwap(_Type *ptr, _Type oldval, _Type newval)
{
    return __sync_bool_compare_and_swap(ptr, oldval, newval);
}

template <typename _Type> _Type atomicAddAndFetch(_Type *ptr, unsigned delta)
{
    return __sync_add_and_fetch(ptr, (_Type)delta);
}

template <typename _Type> _Type atomicSubAndFetch(_Type *ptr, unsigned delta)
{
    return __sync_sub_and_fetch(ptr, (_Type)delta);
}

inline
uint64_t getTSC()
{
#ifdef __i386__
    uint64_t x;
    __asm__ volatile("rdtsc" : "=A"(x));
    return x;
#elif defined(__amd64__) || defined(__x86_64__)
    uint64_t a, d;
    __asm__ volatile("rdtsc" : "=a"(a), "=d"(d));
    return (d << 32) | a;
#else
    uint32_t cc = 0;
    __asm__ volatile ("mrc p15, 0, %0, c9, c13, 0":"=r" (cc));
    return (uint64_t)cc; 
#endif
}

signal_t mutexSignalCreate()
{
    signal_t ret;
    if (pthread_cond_init(&ret, 0))
    {
        throw RunTimeException(__FILE__, __LINE__);
    }
    return ret;
}

void mutexSignalDestroy(signal_t &handle)
{
    if (pthread_cond_destroy(&handle))
    {
        throw RunTimeException(__FILE__, __LINE__);
    }
}

void mutexSignalWait(signal_t &signal, mutex_t &lockedMutex)
{
    if (pthread_cond_wait(&signal, &lockedMutex))
    {
        throw RunTimeException(__FILE__, __LINE__);
    }
}

void mutexSignalWait(signal_t &signal, mutex_t &lockedMutex, const Time &pTimeOut, const Time &currentEpochTime)
{
    struct timespec tout;
    tout.tv_nsec = (long)(pTimeOut.extractNanoseconds() + currentEpochTime.extractNanoseconds());
    tout.tv_sec = pTimeOut.extractSeconds() + currentEpochTime.extractSeconds() + (tout.tv_nsec / 1000000000);
    tout.tv_nsec %= 1000000000;
    int cc = pthread_cond_timedwait(&signal, &lockedMutex, &tout);
    if (cc != 0 && cc != ETIMEDOUT)
    {
        throw RunTimeException(__FILE__, __LINE__);
    }
}

void mutexSignalNotify(signal_t &handle)
{
    if (pthread_cond_signal(&handle))
    {
        throw RunTimeException(__FILE__, __LINE__);
    }
}

/**
 * Linux implementation cannot fail
 * cf: http://man7.org/linux/man-pages/man2/sched_yield.2.html
 */
void threadYield() noexcept { sched_yield(); }

thread_t threadCurrent() { return pthread_self(); }

bool threadEqual(const thread_t &th1, const thread_t &th2) { return pthread_equal(th1, th2) != 0; }

void *tlsGet(tls_t key) { return pthread_getspecific(key); }

void tlsSet(tls_t key, void *value)
{
    int cc = pthread_setspecific(key, value);
    if (cc)
    {
        throw RunTimeException(__FILE__, __LINE__, systemErrorToString(cc));
    }
}

inline
uint64_t    getTicksPerSec(const uint64_t &n_secs = 1)
{
    // depends on core & can change over time
    const uint64_t start_tick = getTSC();
    
    ::sleep(n_secs);
    
    const uint64_t tickPerSec = (getTSC() - start_tick) / n_secs;

    return tickPerSec;
}

DateTime timeGetEpoch()
{
    struct timeval now;
    struct timezone tz;
    gettimeofday(&now, &tz);
    return DateTime((time_t)now.tv_sec, (uint32_t)now.tv_usec / 1000);
}

} // namespace tredzone

/**
 * @fn std::vector<std::string> debugBacktrace(const uint8_t stackTraceSize = 32) noexcept
 * @brief Retrieve a debug backtrace
 * @param stackTraceSize by default is set to 32. This corresponds to the backtrace's maximum size.
 * @return string vector containing the backtrace.
 */
/**
 * @fn std::string demangleFromSymbolName(char*) noexcept
 * @brief Demangle symbol from its mangled name.
 * @param Mangled symbol
 * @return Demangled symbol
 */
/**
 * @fn std::string cppDemangledTypeInfoName(const std::type_info&)
 * @brief Demangle symbol from its type_info
 * @param type_info corresponding to symbol
 * @return Demangled symbol
 */

