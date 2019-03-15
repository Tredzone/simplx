/**
 * @file service.h
 * @brief lowest-level service types
 * @copyright 2013-2019 Tredzone (www.tredzone.com). All rights reserved.
 * Please see accompanying LICENSE file for licensing terms.
 */
 
#pragma once

namespace tredzone
{
    
struct Service
{
    inline static const char *name() noexcept { return ""; }
};
    
/**
 * @brief Helper class for Service tags.
 * @note Service tags must have a name() method.
 */
namespace service
{
    struct E2ERoute : public Service
    {
    };

} // namespace service


} // namespace tredzone
