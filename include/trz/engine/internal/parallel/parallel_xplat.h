/**
 * @file parallel_xplat.h
 * @brief x-platform parallel implementation dispatcher
 * @copyright 2013-2019 Tredzone (www.tredzone.com). All rights reserved.
 * Please see accompanying LICENSE file for licensing terms.
 */
 
#pragma once

#include <cstdint>

#include "trz/engine/internal/parallel/parallel_pthread.h"          // should be CONDITIONAL on PARALLEL_CXX11 or PARALLEL_PTHREAD

namespace tredzone
{

template <class _Mutex> class Lock;

/**
 * @brief ThreadId class
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
 * @brief Mutex class
 */
class Mutex
{
public:
    using Lock = class Lock<Mutex>;
    
    inline Mutex() noexcept;
    ~Mutex() noexcept;

#ifndef NDEBUG
    /**
     * @brief Check if the mutex is locked
     * @return true if locked
     */
    inline bool debugIsLocked() noexcept;
#endif

protected:

    friend class Signal;
    friend class ::tredzone::Lock<Mutex>;           // why needs namespace qualifier?

    mutex_t     handle;
#ifndef NDEBUG
    bool        debugLocked;
    ThreadId    debugLockingThread;
    int         m_TriggerCount;
#endif

    /**
     * @brief Lock mutex
     * @note thread-safe method
     * @throws RunTimeException
     */
    inline void lock();
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
 * @brief Lock class
 */
template <class _Mutex>
class Lock
{
public:
  
    inline Lock(_Mutex &) noexcept;
    inline ~Lock() noexcept;

private:

    _Mutex &mutex;
    Lock &operator=(const Lock &);
};

// mutex
enum class MUTEX_T : uint8_t
{
    NON_RECURSIVE = 0,
    RECURSIVE,
};

mutex_t mutexCreate(const MUTEX_T t = MUTEX_T::RECURSIVE);


} // namespace tredzone

