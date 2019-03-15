/**
 * @file serialbufferchain.h
 * @brief serialized buffer list (deprecated, replaced by mmap serial buffer)
 * @copyright 2013-2019 Tredzone (www.tredzone.com). All rights reserved.
 * Please see accompanying LICENSE file for licensing terms.
 */

#pragma once

// replacement using mmap
#include "trz/engine/internal/mmapserialbuffer.h"

namespace tredzone
{
    using SerialBuffer = mmapSerialBuffer;
    
} // namespace tredzone
