/**
 * @file dlldecorationsave.h
 * @brief Windows DLL decoration save macros
 * @copyright 2013-2019 Tredzone (www.tredzone.com). All rights reserved.
 * Please see accompanying LICENSE file for licensing terms.
 */

#if defined(TREDZONE_DLL_EXPORT_SAVE) || defined(TREDZONE_DLL_IMPORT_SAVE)
#error Illegal definition of TREDZONE_DLL_EXPORT_SAVE and/or TREDZONE_DLL_IMPORT_SAVE
#endif

#ifdef TREDZONE_DLL_EXPORT
#define TREDZONE_DLL_EXPORT_SAVE
#endif

#ifdef TREDZONE_DLL_IMPORT
#define TREDZONE_DLL_IMPORT_SAVE
#endif

#undef TREDZONE_DLL_EXPORT
#undef TREDZONE_DLL_IMPORT