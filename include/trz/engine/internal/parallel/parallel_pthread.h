/**
 * @file parallel_pthread.h
 * @brief parallel operations as pthread implementation
 * @copyright 2013-2019 Tredzone (www.tredzone.com). All rights reserved.
 * Please see accompanying LICENSE file for licensing terms.
 */
 
#pragma once

#include <pthread.h>

namespace tredzone
{

using mutex_t = pthread_mutex_t;

void mutexDestroy(mutex_t &);
void mutexLock(mutex_t &);
void mutexUnlock(mutex_t &);

inline
void memoryBarrier()
{
    __sync_synchronize();
}

using thread_t = pthread_t;

} // namespace tredzone




 