/**
 * @file macro.h
 * @brief internally-used C preprocessor macros
 * @copyright 2013-2019 Tredzone (www.tredzone.com). All rights reserved.
 * Please see accompanying LICENSE file for licensing terms.
 */

#pragma once

#include <iostream>

/**
 * @def TRZ_DEBUG_BREAK
 * @brief break to debugger upon fatal exception
 */
#ifndef TRZ_DEBUG_BREAK
    // wasn't user-defined
    #ifndef NDEBUG
        // debug
        #define TRZ_DEBUG_BREAK() assert(!true) // break to debugger
    #else
        // release
        #define TRZ_DEBUG_BREAK()
    #endif
#endif

/**
 * @def TREDZONE_TRY
 * @brief Can be used to start a 'try' block
 */
#define TREDZONE_TRY                                                                                                   \
    try                                                                                                                \
    {
/**
 * @def TREDZONE_CATCH_AND_EXIT_FAILURE_WITH_CERR_MESSAGE
 * @brief Will catch all exceptions and exit(EXIT_FAILURE)
 */
#define TREDZONE_CATCH_AND_EXIT_FAILURE_WITH_CERR_MESSAGE                                                              \
    }                                                                                                                  \
    catch (const std::exception &e)                                                                                    \
    {                                                                                                                  \
        std::cerr << e.what() << std::endl;                                                                            \
        exit(EXIT_FAILURE);                                                                                            \
    }                                                                                                                  \
    catch (...)                                                                                                        \
    {                                                                                                                  \
        std::cerr << "Unknown exception" << std::endl;                                                                 \
        exit(EXIT_FAILURE);                                                                                            \
    }

#define TREDZONE_EXPAND_STRING(x) TREDZONE_STRING(x)
/**
 * @def TREDZONE_STRING(x)
 * @brief Convert a define to string
 */
#define TREDZONE_STRING(x) #x

