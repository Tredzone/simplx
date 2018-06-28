/**
 * @file actor.cpp
 * @brief actor class
 * @copyright 2013-2018 Tredzone (www.tredzone.com). All rights reserved.
 * Please see accompanying LICENSE file for licensing terms.
 */

#include <cstring>
#include <sstream>

#include "trz/engine/internal/node.h"

using namespace std;

namespace tredzone
{
    
//---- STUBs for WIP engine-to-engine (should never get here) ------------------

void *Actor::Event::Pipe::newOutOfProcessSharedMemoryEvent(size_t, EventChain *&, uintptr_t,
                                                                Event::route_offset_type &)
{
    breakThrow(FeatureNotImplementedException());
    return nullptr;
}

void *Actor::Event::Pipe::newOutOfProcessEvent(void *, size_t, EventChain *&, uintptr_t,
                                                    Event::route_offset_type &)
{
    breakThrow(FeatureNotImplementedException());
    return nullptr;
}

void *Actor::Event::Pipe::newOutOfProcessEvent(size_t sz, EventChain *&destinationEventChain,
                                                    uintptr_t eventOffset, Event::route_offset_type &routeOffset)
{
    return newOutOfProcessEvent(eventFactory.context, sz, destinationEventChain, eventOffset, routeOffset);
}



void *Actor::Event::Pipe::allocateOutOfProcessSharedMemoryEvent(size_t, void *, uint32_t &, size_t &)
{
    breakThrow(FeatureNotImplementedException());
    return nullptr;
}

void *Actor::Event::Pipe::allocateOutOfProcessSharedMemoryEvent(size_t, void *)
{
    breakThrow(FeatureNotImplementedException());
    return 0;
}

#ifndef NDEBUG
    uint64_t Actor::Event::Pipe::batchOutOfProcessSharedMemoryEvent(void *, bool) noexcept
    {
        TRZ_DEBUG_BREAK();
        return 0;
    }
#else
    uint64_t Actor::Event::Pipe::batchOutOfProcessSharedMemoryEvent(void *) noexcept
    {
        TRZ_DEBUG_BREAK()
        return 0;
    }
#endif

} // namespace

