/**
 * @file platform.h
 * @brief cross-platform system wrapper
 * @copyright 2013-2018 Tredzone (www.tredzone.com). All rights reserved.
 * Please see accompanying LICENSE file for licensing terms.
 */

#pragma once

//---- Debug/Release -----------------------------------------------------------

#if (defined(NDEBUG) && defined(DEBUG))
    // debug flags error
    #error conflicting NDEBUG & DEBUG defines
#else
    #if (!defined(NDEBUG))
        #define DEBUG
        #define TREDZONE_DEBUG      1
        #define TREDZONE_RELEASE    0
    #else
        #define RELEASE
        #define TREDZONE_DEBUG      0
        #define TREDZONE_RELEASE    1
    #endif
#endif

//---- C++11 support -----------------------------------------------------------

/*
 * Detect c++11 and set macro
 * cf: http://en.cppreference.com/w/cpp/preprocessor/replace
 */
#if __cplusplus > 199711L
    #define TREDZONE_CPP11_SUPPORT
#endif

//---- Endianness --------------------------------------------------------------

#if (defined(TREDZONE_LITTLE_ENDIAN) || defined(TREDZONE_BIG_ENDIAN))
    // endianness error
    #error endianness is defined by COMPILER itself, not user-defined compiler flags
#endif

//---- OS platform -------------------------------------------------------------

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

//---- Log ---------------------------------------------------------------------

#if (!defined(TREDZONE_LOG))
    // if wasn't user-defined
    #if (TREDZONE_DEBUG == 1)
        // in debug is on by default
        #define TREDZONE_LOG        1
    #else
        // in Release default is off
        #define TREDZONE_LOG        0
    #endif
#endif

//---- References --------------------------------------------------------------

#if (defined(REF_DEBUG) || defined(DEBUG_REF))
    // warn about deprecated flags
    #pragma message "REF_DEBUG/DEBUG_REF is unknown, did you mean TRACE_REF ?"
#endif

#ifdef TRACE_REF
    #include "trz/traceref.h"
#else
    // dummy when no Enterprise license
    #include "trz/engine/internal/dummy/traceref_dummy.h"
#endif

#if (!defined(TREDZONE_CHECK_CYCLICAL_REFS))
    // if wasn't user-defined
    #if (TREDZONE_DEBUG == 1)
        #define TREDZONE_CHECK_CYCLICAL_REFS    1
    #else
        #define TREDZONE_CHECK_CYCLICAL_REFS    0
    #endif
#endif

// nada mas