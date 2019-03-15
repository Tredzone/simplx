/**
 * @file intrinsics.h
 * @brief intrinsics class
 * @copyright 2013-2019 Tredzone (www.tredzone.com). All rights reserved.
 * Please see accompanying LICENSE file for licensing terms.
 */

#pragma once

#include <algorithm>

#include "trz/engine/platform.h"

#if defined(__SSE2__)
#include <emmintrin.h>
#endif

namespace tredzone
{

#if defined(__SSE2__)
/**
 * @brief Returns the greater of the given values.
 * @param lhs first value to compare
 * @param rhs second value to compare
 * @return The greater of lhs and rhs. If the values are equivalent, returns lhs.
 */
inline double max(const double &lhs, const double &rhs) noexcept
{
    double ret;
    _mm_store_sd(&ret, _mm_max_sd(_mm_load_sd(&lhs), _mm_load_sd(&rhs)));
    return ret;
}
/**
 * @brief Returns the smaller of the given values.
 * @param lhs first value to compare
 * @param rhs second value to compare
 * @return The smaller of lhs and rhs. If the values are equivalent, returns lhs.
 */
inline double min(const double &lhs, const double &rhs) noexcept
{
    double ret;
    _mm_store_sd(&ret, _mm_min_sd(_mm_load_sd(&lhs), _mm_load_sd(&rhs)));
    return ret;
}
#endif

/**
 * @brief Returns the highest order bit
 * @param value to check
 */
template <typename T> inline unsigned highestBit(const T &pv) noexcept
{ // should use plateform-dependent intrinsics (e.g. gcc's int __builtin_clz())
    unsigned ret = 0;
    for (T v = pv; v != 0; ++ret, v >>= 1)
    {
    }
    return --ret;
}
/**
 * @brief Returns the lowest order bit
 * @param value to check
 */
template <typename T> inline unsigned lowestBit(const T &pv) noexcept
{
    unsigned ret = 0;
    for (T v = pv; v != 0 && (v & 1) == 0; ++ret, v >>= 1)
    {
    }
    return ret;
}

} // namespace