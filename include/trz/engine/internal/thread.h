/**
 * @file thread.h
 * @brief custom thread class
 * @copyright 2013-2018 Tredzone (www.tredzone.com). All rights reserved.
 * Please see accompanying LICENSE file for licensing terms.
 */

#pragma once

#include <cassert>
#include <list>

#include "trz/engine/platform.h"

namespace tredzone
{

template <class _Mutex> class Lock;
template <class _Mutex> class TryLock;
template <class _Mutex> class ReverseLock;
class Mutex;
class Signal;
class Thread;
class ThreadId;
template <class T> class ThreadLocalStorage;
template <class T> class ThreadLocalStorage<T *>;

/**
 * @brief Tredzone ThreadId class
 */
class ThreadId
{
  public:
    /** @brief Constructor */
    inline ThreadId(const thread_t &) noexcept;
    /** @brief Get the current thread */
    inline static ThreadId current() noexcept;
    /** @brief comparison operator */
    inline bool operator==(const ThreadId &) const noexcept;
    /** @brief comparison operator */
    inline bool operator!=(const ThreadId &) const noexcept;

  private:
    thread_t id;
};

/**
 * @brief Tredzone Mutex class
 */
class Mutex
{
  public:
    typedef ::tredzone::Lock<Mutex> Lock;
    typedef ::tredzone::TryLock<Mutex> TryLock;
    typedef ::tredzone::ReverseLock<Mutex> ReverseLock;

    /** @brief Constructor */
    inline Mutex() noexcept;
    /** @brief Destructor */
    inline ~Mutex() noexcept;

#ifndef NDEBUG
    /**
     * @brief Check if the mutex is locked
     * @return true if locked
     */
    inline bool debugIsLocked() noexcept;
#endif

  protected:
    friend class Signal;
    friend class ::tredzone::Lock<Mutex>;
    friend class ::tredzone::TryLock<Mutex>;
    friend class ::tredzone::ReverseLock<Mutex>;

    mutex_t handle;
#ifndef NDEBUG
    bool debugLocked;
    ThreadId debugLockingThread;
#endif

    /**
     * @brief Lock mutex
     * @note thread-safe method
     * @throws RunTimeException
     */
    inline void lock();
    /**
     * @brief Try to lock the mutex
     * @return true if lock is successful
     * @note thread-safe method
     * @throws RunTimeException
     */
    inline bool tryLock();
    /**
     * @brief Unlock mutex
     * @note thread-safe method
     * @throws RunTimeException
     */
    inline void unlock();

  private:
    Mutex &operator=(const Mutex &);
};

/**
 * @brief Tredzone Lock class
 */
template <class _Mutex> class Lock
{
  public:
    /** @brief Constructor */
    inline Lock(_Mutex &) noexcept;
    /** @brief Destructor */
    inline ~Lock() noexcept;

  private:
    _Mutex &mutex;
    /** @brief Equal operator */
    Lock &operator=(const Lock &);
};

/**
 * @brief Tredzone TryLock class
 */
template <class _Mutex> class TryLock
{
  public:
    const bool isLocked;
    /** @brief Constructor */
    inline TryLock(_Mutex &) noexcept;
    /** @brief Destructor */
    inline ~TryLock() noexcept;

  private:
    _Mutex &mutex;
    /** @brief Equal operator */
    TryLock &operator=(const TryLock &);
};

/**
 * @brief Tredzone ReverseLock class
 */
template <class _Mutex> class ReverseLock
{
  public:
    /** @brief Constructor */
    inline ReverseLock(_Mutex &) noexcept;
    /** @brief Destructor */
    inline ~ReverseLock() noexcept;

  private:
    _Mutex &mutex;
    /** @brief Equal operator */
    ReverseLock &operator=(const ReverseLock &);
};

/**
 * @brief Tredzone Signal class
 */
class Signal
{
  public:
    /** @brief Constructor */
    inline Signal(Mutex &);
    /** @brief Destructor */
    inline ~Signal();
    /** @brief Block thread until condition variable (signal) is notified */
    inline void wait();
    /** @brief Block thread until condition variable (signal) is notified or delay has been reached */
    inline void wait(const Time &delay);
    /** @brief Block thread until condition variable (signal) is notified or delay has been reached */
    inline void wait(const Time &delay, const Time &currentEpochTime);
    /** @brief Notify waiting thread */
    inline void notify();

  private:
    Mutex &mutex;
    signal_t handle;
    Signal &operator=(const Signal &);
};

/**
 * @brief Tredzone Thread class
 */
class TREDZONE_DLL Thread
{
  private:
    bool isRunningFlag;
    thread_t threadId;
    inline static void callback(void *);
    Thread &operator=(const Thread &);

  protected:
    virtual void onRun() = 0;

  public:
    /** @brief Constructor */
    inline Thread();
    /** @brief Destructor */
    inline virtual ~Thread();
    /**
     * @brief Create thread
     * @param stackSizeBytes thread's stack size
     * @return ThreadId
     */
    inline ThreadId run(size_t stackSizeBytes = 0);
    /**
     * @brief Block until thread is running
     * @throws RunTimeException
     */
    inline void join();
    /**
     * @brief Sleep thread for a defined time
     * @param Time to sleep
     * @throws RunTimeException
     */
    inline static void sleep(const Time & = Time());
    /**
     * @brief Check if the thread is running
     * @return true if thread is running, else otherwise
     */
    inline bool isRunning() const noexcept;
};

template <class T> class ThreadLocalStorage;

/**
 * @brief Tredzone ThreadLocalStorage (TLS) class
 */
template <class T> class ThreadLocalStorage<T *>
{
  public:
    /** @brief Constructor */
    inline ThreadLocalStorage() : tlsKey(tlsCreate()) {}
    /** @brief Destructor */
    inline ~ThreadLocalStorage() noexcept { tlsDestroy(tlsKey); }
    /**
     * @brief Get the TLS value
     * @return TLS value
     */
    inline T *get() noexcept { return static_cast<T *>(tlsGet(tlsKey)); }
    /**
     * @brief Get the TLS value
     * @return TLS value
     */
    inline const T *get() const noexcept { return static_cast<const T *>(tlsGet(tlsKey)); }
    /**
     * @brief Set a value to the TLS
     * @param p value to set
     */
    inline void set(T *p) { tlsSet(tlsKey, const_cast<void *>(static_cast<const void *>(p))); }

  private:
    tls_t tlsKey;
};

ThreadId::ThreadId(const thread_t &pId) noexcept : id(pId) {}

ThreadId ThreadId::current() noexcept { return ThreadId(threadCurrent()); }

bool ThreadId::operator==(const ThreadId &other) const noexcept
{
    return threadEqual(const_cast<thread_t &>(id), const_cast<thread_t &>(other.id));
}

bool ThreadId::operator!=(const ThreadId &other) const noexcept
{
    return !threadEqual(const_cast<thread_t &>(id), const_cast<thread_t &>(other.id));
}

Thread::Thread() : isRunningFlag(false) {}

Thread::~Thread()
{
    assert(!isRunning());
    join();
}

ThreadId Thread::run(size_t stackSizeBytes)
{
    if (!isRunning())
    {
        isRunningFlag = true;
        memoryBarrier();
        try
        {
            threadId = threadCreate(callback, this, stackSizeBytes);
        }
        catch (...)
        {
            isRunningFlag = false;
            throw;
        }
    }
    return threadId;
}

void Thread::join()
{
    for (; isRunning(); sleep())
    {
    }
}

void Thread::callback(void *th)
{
    assert(((Thread *)th)->isRunning());
    ((Thread *)th)->onRun();
    memoryBarrier();
    ((Thread *)th)->isRunningFlag = false;
    memoryBarrier();
}

void Thread::sleep(const Time &delay) { threadSleep(delay); }

bool Thread::isRunning() const noexcept
{
    memoryBarrier();
    return isRunningFlag;
}

#ifdef NDEBUG
// Release
Mutex::Mutex() noexcept : handle(mutexCreate(false)) {}

void Mutex::lock() { mutexLock(handle); }

bool Mutex::tryLock() { return mutexTryLock(handle); }

void Mutex::unlock() { mutexUnlock(handle); }
#else
// Debug
Mutex::Mutex() noexcept : handle(mutexCreate(true)), debugLocked(false), debugLockingThread(ThreadId::current()) {}

void Mutex::lock()
{
    mutexLock(handle);
    assert(!debugLocked);

    debugLocked = true;
    debugLockingThread = ThreadId::current();
}

bool Mutex::tryLock()
{
    bool ret = mutexTryLock(handle);
    if (ret == true)
    {
        assert(!debugLocked);

        debugLocked = true;
        debugLockingThread = ThreadId::current();
    }
    return ret;
}

void Mutex::unlock()
{
    assert(debugIsLocked());
    debugLocked = false;
    mutexUnlock(handle);
}

bool Mutex::debugIsLocked() noexcept
{
    TREDZONE_TRY
    mutexLock(handle);
    bool ret = debugLocked;
    if (ret == true)
    {
        assert(debugLockingThread == ThreadId::current());
    }
    mutexUnlock(handle);
    return ret;
    TREDZONE_CATCH_AND_EXIT_FAILURE_WITH_CERR_MESSAGE
    return false; // this never happens
}
#endif

Mutex::~Mutex() noexcept
{
    TREDZONE_TRY
    assert(!debugIsLocked());
    mutexDestroy(handle); // throw(tredzone::RunTimeException)
    TREDZONE_CATCH_AND_EXIT_FAILURE_WITH_CERR_MESSAGE
}

template <class _Mutex> Lock<_Mutex>::Lock(_Mutex &pMutex) noexcept : mutex(pMutex) { mutex.lock(); }

template <class _Mutex> Lock<_Mutex>::~Lock() noexcept { mutex.unlock(); }

template <class _Mutex> TryLock<_Mutex>::TryLock(_Mutex &pMutex) noexcept : isLocked(pMutex.tryLock()), mutex(pMutex) {}

template <class _Mutex> TryLock<_Mutex>::~TryLock() noexcept
{
    if (isLocked)
    {
        mutex.unlock();
    }
}

template <class _Mutex> ReverseLock<_Mutex>::ReverseLock(_Mutex &pMutex) noexcept : mutex(pMutex) { mutex.unlock(); }

template <class _Mutex> ReverseLock<_Mutex>::~ReverseLock() noexcept { mutex.lock(); }

Signal::Signal(Mutex &pMutex) : mutex(pMutex), handle(mutexSignalCreate()) {}

Signal::~Signal() { mutexSignalDestroy(handle); }

void Signal::wait()
{
#ifndef NDEBUG
    assert(mutex.debugIsLocked());
    mutex.debugLocked = false;
#endif
    mutexSignalWait(handle, mutex.handle);
#ifndef NDEBUG
    mutex.debugLocked = true;
    mutex.debugLockingThread = ThreadId::current();
#endif
}

void Signal::wait(const Time &delay)
{
#ifndef NDEBUG
    assert(mutex.debugIsLocked());
    mutex.debugLocked = false;
#endif
    mutexSignalWait(handle, mutex.handle, delay);
#ifndef NDEBUG
    mutex.debugLocked = true;
    mutex.debugLockingThread = ThreadId::current();
#endif
}

void Signal::wait(const Time &delay, const Time &currentEpochTime)
{
#ifndef NDEBUG
    assert(mutex.debugIsLocked());
    mutex.debugLocked = false;
#endif
    mutexSignalWait(handle, mutex.handle, delay, currentEpochTime);
#ifndef NDEBUG
    mutex.debugLocked = true;
    mutex.debugLockingThread = ThreadId::current();
#endif
}

void Signal::notify() { mutexSignalNotify(handle); }

} // namespace


namespace tredzone
{

/**
 * @typedef tredzone::mutex_t
 * @brief System dependant mutex handle.
 * (@ref wrapper-types)
 */

/**
 * @typedef tredzone::signal_t
 * @brief System dependant mutex condition signal event handle.
 * (@ref wrapper-types)
 */

/**
 * @typedef tredzone::tls_t
 * @brief System dependant thread local storage handle.
 * (@ref wrapper-types)
 */

/**
 * @fn pid_t getPID()
 * @brief Get the current process's process id (PID)
 * @return Process id
 */
/**
 * @fn std::string systemErrorToString(int)
 * @brief Convert a system error number (errno) to a human readable string
 * @param Error number (errno) corresponding to error
 * @return String representation of error
 */
/**
 * @fn size_t systemPageSize()
 * @brief Retrieve the process's page size.
 * This value is fixed for the runtime of the process but can vary in different runs of the application.
 * @return Process's page size
 */
/**
 * @fn void memoryBarrier()
 * @brief Full memory barrier
 * No memory operand will be moved across the operation, either forward or backward. Further, instructions will be
 * issued as
 * necessary to prevent the processor from speculating loads across the operation and from queuing stores after the
 * operation.
 */
/**
 * @fn void* alignMalloc(size_t alignement, size_t size)
 * @brief Allocate size bytes aligned on a boundary specified by alignment
 * @param alignement Must be a multiple of sizeof(void*), that is also a power of two.
 * @param size to be allocated
 * @throws RunTimeException if posix_memalign() fails with an error different than ENOMEM
 * @throws std::bad_alloc if there is insufficient memory available with the requested alignment
 */
/**
 * @fn void alignFree(size_t, void*)
 * @brief Free memory buffer
 * @param Buffer size
 * @param Buffer to be freed
 */
/**
 * @fn template<typename _Type> inline bool atomicCompareAndSwap(_Type* ptr, _Type oldval, _Type newval)
 * @param ptr Value to be compared and replaced by newval
 * @param oldval Value to be compared with *ptr
 * @param newval Value that is to replace *ptr
 * @return returns true if the comparison is successful and newval was written
 */
/**
 * @fn template<typename _Type> inline _Type atomicAddAndFetch(_Type* ptr, unsigned delta)
 * @brief Atomically add and return pointer
 * @param ptr pointer
 * @param delta to be added to pointer
 * @return pointer
 */
/**
 * @fn template<typename _Type> inline _Type atomicSubAndFetch(_Type* ptr, unsigned delta)
 * @brief Atomically substract and return pointer
 * @param ptr pointer
 * @param delta to be substracted to pointer
 * @return pointer
 */
/**
 * @fn inline uint64_t getTSC()
 * @brief Get the current Time Stamp Counter (TSC)
 * @return Current TSC
 */
/**
 * @fn inline DateTime timeGetEpoch()
 * @brief Get a DateTime object containing the current date and time
 * @note Uses gettimeofday()
 * @return Initialized DateTime object
 */

/**
 * @fn mutex_t mutexCreate(bool recursive = true)
 * @brief Create a mutex
 * @param recursive if set to true, mutex attribute is set to PTHREAD_MUTEX_RECURSIVE_NP otherwise it is set to
 * PTHREAD_MUTEX_FAST_NP
 * @return mutex object
 */
/**
 * @fn inline void mutexDestroy(mutex_t&)
 * @brief Destroy a mutex
 * @param mutex to destroy
 */
/**
 * @fn inline void mutexLock(mutex_t&)
 * @brief Lock a mutex. If the mutex is already locked, the calling thread shall block until the mutex becomes
 * available.
 * @param mutex to lock
 */
/**
 * @fn inline bool mutexTryLock(mutex_t&)
 * @brief Equivalent to mutexLock(), except that if the mutex object referenced by mutex is currently locked
 * (by any thread, including the current thread), the call shall return immediately.
 * @param mutex to try and lock
 * @return true if mutex has been locked, false otherwise.
 */
/**
 * @fn inline void mutexUnlock(mutex_t&)
 * @brief Unlock a mutex
 * @param  mutex to unlock
 */
/**
 * @fn inline signal_t mutexSignalCreate()
 * @brief Retrieve a thread condition that is initialized with default attributes
 * @return initialized thread condition
 */
/**
 * @fn inline void mutexSignalDestroy(signal_t&)
 * @brief Destroy thread condition
 * @param Signal to destroy
 */
/**
 * @fn inline void mutexSignalWait(signal_t&, mutex_t& lockedMutex)
 * @brief Atomically release mutex and cause the calling thread to block on the condition variable
 * @param condition variable
 * @param lockedMutex mutex
 */
/**
 * @fn inline void mutexSignalWait(signal_t&, mutex_t& lockedMutex, const Time& timeOut, const Time& = timeGetEpoch())
 * @brief Atomically release mutex and cause the calling thread to block on the condition variable
 * @param condition variable
 * @param lockedMutex mutex
 * @param timeOut mutex timeout
 * @param Current time
 */
/**
 * @fn inline void mutexSignalNotify(signal_t&)
 * @brief Unblocks at least one of the threads that are blocked on the specified condition variable cond
 * @param condition variable
 */

/**
 * @fn size_t cpuGetCount()
 * @brief Get the number of logical cores present on the System as seen my the OS.
 * @return Logical CPU count
 */

/**
 * @fn thread_t threadCreate(void (*)(void*), void*, size_t stackSizeBytes = 0)
 * @brief Start a new thread
 * @param Function to be run in thread
 * @param Function argument
 * @param stackSizeBytes thread stack size
 * @return Newly created thread identifier
 */
/**
 * @fn cpuset_type threadGetAffinity()
 * @brief Get the current thread's affinity
 * @return Core affinity bitset
 */
/**
 * @fn void threadSetAffinity(const cpuset_type& bitset)
 * @brief Set thread affinity
 * @param bitset representing the cpu cores to be set
 */
/**
 * @fn void threadSetAffinity(unsigned)
 * @brief Set thread affinity
 * @param cpu core to set affinity to
 */
/**
 * @fn void threadSetRealTime(bool, const ThreadRealTimeParam&)
 * @brief Set real time (RT) priority to current thread
 * @param If set to true, RT priority will be set to current thread. Otherwise default priority is set.
 * @param Thread priority level
 * @throws RunTimeException if the thread's priority could not be set
 */
/**
 * @fn inline void threadYield() noexcept
 * @brief Voluntarily gives up the process' claim on the CPU.
 */
/**
 * @fn void threadSleep(const Time& delay = Time())
 * @brief As the name suggests, sleep thread for a defined time
 * @param delay Default is 0 for minimum sleep time
 */
/**
 * @fn inline thread_t threadCurrent()
 * @brief Get current thread identifier
 * @return Current thread identifier
 */
/**
 * @fn inline bool threadEqual(const thread_t&, const thread_t&)
 * @brief Compare if two thread identifiers are equal
 * @param First thread to compare
 * @param Second thread to compare
 * @return true if threads are equal
 */
/**
 * @fn tls_t tlsCreate()
 * @brief Create a Thread Local Storage (TLS)
 * @return Newly created TLS
 */
/**
 * @fn void tlsDestroy(tls_t)
 * @brief Destroy given Thread Local Storage (TLS)
 * @param TLS identifer to be destroyed
 */
/**
 * @fn inline void* tlsGet(tls_t)
 * @brief Retrieve value associated to TLS key
 * @param TLS key
 * @return Value associated to TLS key
 */
/**
 * @fn inline void tlsSet(tls_t, void*)
 * @brief Set a value to given TLS key
 * @param TLS key
 * @param Value to set
 */
 
} // namespace