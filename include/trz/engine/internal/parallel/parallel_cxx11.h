/**
 * @file parallel_cxx11.h
 * @brief parallel operations as c++11 implementation
 * @copyright 2013-2019 Tredzone (www.tredzone.com). All rights reserved.
 * Please see accompanying LICENSE file for licensing terms.
 */
 
#pragma once

#include <atomic>
#include <mutex>

namespace tredzone
{
// import into namespace
using std::atomic_thread_fence;

using mutex_t = std::mutex;

inline
void memoryBarrier()
{
    atomic_thread_fence(std::memory_order::memory_order_seq_cst);
}

    
    
} // namespace tredzone