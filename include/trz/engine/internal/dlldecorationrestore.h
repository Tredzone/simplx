/**
 * @file dlldecorationrestore.h
 * @brief Windows DLL decoration restore macros
 * @copyright 2013-2018 Tredzone (www.tredzone.com). All rights reserved.
 * Please see accompanying LICENSE file for licensing terms.
 */

#undef TREDZONE_DLL_EXPORT
#ifdef TREDZONE_DLL_EXPORT_SAVE
#define TREDZONE_DLL_EXPORT
#endif

#undef TREDZONE_DLL_IMPORT
#ifdef TREDZONE_DLL_IMPORT_SAVE
#define TREDZONE_DLL_IMPORT
#endif

#undef TREDZONE_DLL_EXPORT_SAVE
#undef TREDZONE_DLL_IMPORT_SAVE