/**
 * @author Valerian Vives <valerian.vives@tredzone.com>
 * @file testvariables.hpp
 * @brief global variable used by tests
 * @copyright 2013-2018 Tredzone (www.tredzone.com). All rights reserved.
 * Please see accompanying LICENSE file for licensing terms.
 */

#pragma once

#include <condition_variable>
#include <mutex>
#include <thread>

//#include <simplx.h>

namespace tredzone
{
namespace connector
{
namespace tcp
{

std::mutex              g_mutex;
std::condition_variable g_conditionVariable;
bool                    g_finished         = false;
bool                    g_expectingTimeout = false;
bool                    g_failOnTimeout    = false;
} // namespace tcp
} // namespace connector
} // namespace tredzone