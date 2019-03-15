/**
 * @file endianness.h
 * @brief cros-platform endian-ness
 * @copyright 2013-2019 Tredzone (www.tredzone.com). All rights reserved.
 * Please see accompanying LICENSE file for licensing terms.
 */

#pragma once

#include "trz/engine/platform.h"

#ifdef __APPLE__

	// host-to-B/L-endian (outoing format is "network-endianness")
	#include <machine/endian.h>
	// no POSIX header for endianness

	#define	htobe16		htons			// host to network SHORT (networking == big-endian)
	#define	htobe32		htonl			// host to network LONG

	#define	be16toh		ntohs			// network to host SHORT
	#define	be32toh		ntohl			// network to host LONG

	#define	be64toh(u64)	(static_cast<uint64_t>(ntohl((u64 & 0xFFFFFFFF00000000ull) >> 32)) | (static_cast<uint64_t>(ntohl(u64 & 0x00000000FFFFFFFFull)) << 32))
	#define	htobe64(u64)	(static_cast<uint64_t>(ntohl((u64 & 0xFFFFFFFF00000000ull) >> 32)) | (static_cast<uint64_t>(ntohl(u64 & 0x00000000FFFFFFFFull)) << 32))

#elif defined(WIN32)

	// Windows
	// check Windows headers for faster asm-based byte-swappers
	// NOTE: winsock2.h has
	// unsigned __int64 __inline htonll(unsigned __int64 value);

	// should wrap additional () around arg
	#define	be16toh(u16)	((u16 >> 8) | ((u16 & 0xFF) << 8))
	#define	be32toh(u32)	(((u32 & 0xFF000000) >> 24) | ((u32 & 0x00FF0000) >> 8) | ((u32 & 0x0000FF00) << 8) | ((u32 & 0x000000FF) << 24))
	#define	be64toh(u64)	(((u64 & 0xFF00000000000000ull) >> 56) | \
				((u64 & 0x00FF000000000000ull)  >> 40) | \
				((u64 & 0x0000FF0000000000ull)  >> 24) | \
				((u64 & 0x000000FF00000000ull)  >>  8) | \
				((u64 & 0x00000000FF000000ull)  <<  8) | \
				((u64 & 0x0000000000FF0000ull)  << 24) | \
				((u64 & 0x000000000000FF00ull)  >> 40) | \
				((u64 & 0x00000000000000FFull)  >> 56))


	#define	htobe16(u16)	((u16 >> 8) | ((u16 & 0xFF) << 8))
	#define	htobe32(u32)	(((u32 & 0xFF000000) >> 24) | ((u32 & 0x00FF0000) >> 8) | ((u32 & 0x0000FF00) << 8) | ((u32 & 0x000000FF) << 24))
	#define	htobe64(u64)	(((u64 & 0xFF00000000000000ull) >> 56) | \
				((u64 & 0x00FF000000000000ull) >> 40)  | \
				((u64 & 0x0000FF0000000000ull) >> 24)  | \
				((u64 & 0x000000FF00000000ull) >> 8)   | \
				((u64 & 0x00000000FF000000ull) << 8)   | \
				((u64 & 0x0000000000FF0000ull) << 24)  | \
				((u64 & 0x000000000000FF00ull) >> 40)  | \
				((u64 & 0x00000000000000FFull) >> 56))
	
#elif defined(__linux__)

	// Linux
	// host-to-B/L-endian (outoing formant is "network-endianness")
	#include "endian.h"
	// Linux also has "byteswap.h"
	// no POSIX header for endianness

	/*
	#already defined in above headers
	be16toh
	be32toh
	be64toh

	htobe16
	htobe32
	htobe64
	*/

#else
	
	// platform detection FAILED
	static_assert(false, "StreamEndianness.h platform detection FAILED");
	
#endif

#if 0
	// in gcc
	#define __BYTE_ORDER__		__ORDER_LITTLE_ENDIAN__
	
	// in gcc headers
	/* Define to 1 if you have the <machine/endian.h> header file. */
	/* #undef _GLIBCXX_HAVE_MACHINE_ENDIAN_H */
#endif

// network & stream swapping

#if (TREDZONE_STREAM_SWAP == 0)
    // nop swappers
    #define netswap16
    #define netswap32
    #define netswap64
#elif (TREDZONE_STREAM_SWAP == 1)
    #define netswap16   htobe16
    #define netswap32   htobe32
    #define netswap64   htobe64
#else
    #error "illegal TREDZONE_STREAM_SWAP define!"
#endif
    

