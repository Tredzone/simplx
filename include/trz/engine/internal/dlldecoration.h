/**
 * @file dlldecoration.h
 * @brief Windows DLL handling macros
 * @copyright 2013-2018 Tredzone (www.tredzone.com). All rights reserved.
 * Please see accompanying LICENSE file for licensing terms.
 */

#if defined(TREDZONE_DLL_EXPORT) && defined(TREDZONE_DLL_IMPORT)
#error Simultaneous definition of TREDZONE_DLL_EXPORT and TREDZONE_DLL_IMPORT
#endif

#ifdef _MSC_VER // MS VISUAL C++ predefined macro
#ifdef TREDZONE_DLL_EXPORT
#undef TREDZONE_DLL
#define TREDZONE_DLL __declspec(dllexport)
#elif defined(TREDZONE_DLL_IMPORT)
#undef TREDZONE_DLL
#define TREDZONE_DLL __declspec(dllimport)
#endif
#else
#undef TREDZONE_DLL
#define TREDZONE_DLL
#endif