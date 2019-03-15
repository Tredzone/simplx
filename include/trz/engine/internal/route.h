/**
 * @file route.h
 * @brief route class
 * @copyright 2013-2019 Tredzone (www.tredzone.com). All rights reserved.
 * Please see accompanying LICENSE file for licensing terms.
 */

#pragma once

#include "trz/engine/actor.h"

namespace tredzone
{

namespace e2econnector
{
// import into namespace

// forward-declarations

using connection_distance_type = uint8_t;
using engine_id_type = uint64_t;

#pragma pack(push)
#pragma pack(1)
/**
 * @brief Route entry base information.
 */
struct RouteEntryBase
{
    Actor::ActorId::RouteId     routeId;
    bool                        isSharedMemory;
    connection_distance_type    serialConnectionDistance;
};

struct RouteEntry : public RouteEntryBase
{
public:

    RouteEntry *nextRouteEntry;
    /**
     * @brief Get the number of routes for a given RouteEntry
     * @param routeEntry route entry to check
     * @return number of routes for the given routeEntry
     */
    inline static size_t getRouteEntryCount(RouteEntry *routeEntry) noexcept
    {
        size_t ret = 0;
        for (; routeEntry != 0; ++ret, routeEntry = routeEntry->nextRouteEntry)
        {
        }
        return ret;
    }
};

#pragma pack(pop)
    
} // namespace e2econnector

} // namespace tredzone

