/**
 * @file platform.h
 * @brief cross-platform system wrapper
 * @copyright 2013-2018 Tredzone (www.tredzone.com). All rights reserved.
 * Please see accompanying LICENSE file for licensing terms.
 */

#pragma once

#if defined(TREDZONE_LITTLE_ENDIAN) || defined(TREDZONE_BIG_ENDIAN)
    #error endianness macros should not be user-defined!
#endif

#if !defined(TREDZONE_PLATFORM_LINUX) && defined(__linux__)
    #define TREDZONE_PLATFORM_LINUX
#elif !defined(TREDZONE_PLATFORM_APPLE) && defined(__APPLE__)
    #error Apple platform is currently under development!
#elif !defined(TREDZONE_PLATFORM_WINDOWS) && (defined(_WIN32) || defined(__CYGWIN__))
    #error Windows platform is currently under development!
#endif

#if defined(TREDZONE_PLATFORM_LINUX)
    #if defined(__GNUG__)
        #include "trz/engine/internal/linux/platform_gcc.h"
    #else
        #error No supported C++ compiler for LINUX
    #endif
#elif defined(TREDZONE_PLATFORM_APPLE)
    #error Apple platform is currently under development!
#elif defined(TREDZONE_PLATFORM_WINDOWS)
    #error Windows platform is currently under development!
#else
    // unsupported platform
    #error Undefined OS level symbol TREDZONE_PLATFORM_xxx (where xxx is LINUX or APPLE or WINDOWS)
#endif


