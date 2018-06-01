/**
 * @file actor.cpp
 * @brief actor class
 * @copyright 2013-2018 Tredzone (www.tredzone.com). All rights reserved.
 * Please see accompanying LICENSE file for licensing terms.
 */

#include <cstring>

#include "trz/engine/internal/node.h"

namespace tredzone
{

const int AsyncActor::MAX_NODE_COUNT;
const int AsyncActor::MAX_EVENT_ID_COUNT;

AsyncActor::EventId AsyncActor::Event::retainEventId(EventToOStreamFunction peventNameToOStreamFunction,
                                                     EventToOStreamFunction peventContentToOStreamFunction,
                                                     EventIsE2ECapableFunction peventIsE2ECapableFunction)
{ // throw (std::bad_alloc)
    assert(std::numeric_limits<EventId>::max() >= MAX_EVENT_ID_COUNT);
    Mutex::Lock lock(AsyncNodeBase::staticShared.mutex);
    EventId eventId = 0;
    for (; eventId < AsyncNodeBase::staticShared.eventIdBitSet.size() &&
           AsyncNodeBase::staticShared.eventIdBitSet[eventId];
         ++eventId)
    {
    }
    if (eventId == AsyncNodeBase::staticShared.eventIdBitSet.size())
    {
        throw UndersizedException(UndersizedException::EVENT_ID);
    }

    EventE2ESerializeFunction eventE2ESerializeFunction;
    EventE2EDeserializeFunction eventE2EDeserializeFunction;
    const char *eventE2EAbsoluteId;
    if (peventIsE2ECapableFunction(eventE2EAbsoluteId, eventE2ESerializeFunction, eventE2EDeserializeFunction))
    {
        AsyncNodeBase::staticShared.absoluteEventIds.addEventId(eventId, eventE2EAbsoluteId);
    }

    AsyncNodeBase::staticShared.eventIdBitSet[eventId] = true;
    AsyncNodeBase::staticShared.eventToStreamFunctions[eventId].eventNameToOStreamFunction =
        peventNameToOStreamFunction;
    AsyncNodeBase::staticShared.eventToStreamFunctions[eventId].eventContentToOStreamFunction =
        peventContentToOStreamFunction;
    AsyncNodeBase::staticShared.eventIsE2ECapableFunction[eventId] = peventIsE2ECapableFunction;
    return eventId;
}

void AsyncActor::Event::releaseEventId(EventId eventId) noexcept
{
    Mutex::Lock lock(AsyncNodeBase::staticShared.mutex);
    assert(eventId < AsyncNodeBase::staticShared.eventIdBitSet.size());
    assert(AsyncNodeBase::staticShared.eventIdBitSet[eventId]);

    EventE2ESerializeFunction eventE2ESerializeFunction;
    EventE2EDeserializeFunction eventE2EDeserializeFunction;
    const char *eventE2EAbsoluteId;
    if (AsyncNodeBase::staticShared.eventIsE2ECapableFunction[eventId](eventE2EAbsoluteId, eventE2ESerializeFunction,
                                                                       eventE2EDeserializeFunction))
    {
        assert(AsyncNodeBase::staticShared.absoluteEventIds.findEventId(eventE2EAbsoluteId).second == eventId);
        AsyncNodeBase::staticShared.absoluteEventIds.removeEventId(eventId);
    }

    AsyncNodeBase::staticShared.eventIdBitSet[eventId] = false;
    AsyncNodeBase::staticShared.eventToStreamFunctions[eventId].eventNameToOStreamFunction = 0;
    AsyncNodeBase::staticShared.eventToStreamFunctions[eventId].eventContentToOStreamFunction = 0;
    AsyncNodeBase::staticShared.eventIsE2ECapableFunction[eventId] = 0;
}

std::pair<bool, AsyncActor::EventId> AsyncActor::Event::findEventId(const char *absoluteEventId) noexcept
{
    Mutex::Lock lock(AsyncNodeBase::staticShared.mutex);
    return AsyncNodeBase::staticShared.absoluteEventIds.findEventId(absoluteEventId);
}

bool AsyncActor::Event::isE2ECapable(EventId eventId, const char *&absoluteEventId,
                                     EventE2ESerializeFunction &serializeFn, EventE2EDeserializeFunction &deserializeFn)
{
    Mutex::Lock lock(AsyncNodeBase::staticShared.mutex);
    assert(eventId < AsyncNodeBase::staticShared.eventIdBitSet.size());
    if (AsyncNodeBase::staticShared.eventIdBitSet[eventId])
    {
        assert(AsyncNodeBase::staticShared.eventIsE2ECapableFunction[eventId] != 0);
        return AsyncNodeBase::staticShared.eventIsE2ECapableFunction[eventId](absoluteEventId, serializeFn,
                                                                              deserializeFn);
    }
    else
    {
        return false;
    }
}

bool AsyncActor::Event::isE2ECapable(const char *absoluteEventId, EventId &eventId,
                                     EventE2ESerializeFunction &serializeFn, EventE2EDeserializeFunction &deserializeFn)
{
    Mutex::Lock lock(AsyncNodeBase::staticShared.mutex);
    std::pair<bool, EventId> retFindEventId = AsyncNodeBase::staticShared.absoluteEventIds.findEventId(absoluteEventId);
    assert(retFindEventId.first == false || retFindEventId.second < AsyncNodeBase::staticShared.eventIdBitSet.size());
    if (retFindEventId.first && AsyncNodeBase::staticShared.eventIdBitSet[eventId = retFindEventId.second])
    {
        assert(AsyncNodeBase::staticShared.eventIsE2ECapableFunction[eventId] != 0);
        return AsyncNodeBase::staticShared.eventIsE2ECapableFunction[eventId](absoluteEventId, serializeFn,
                                                                              deserializeFn);
    }
    else
    {
        return false;
    }
}

void AsyncActor::Event::nameToOStream(std::ostream &os, const Event &event) { os << event.classId; }

void AsyncActor::Event::contentToOStream(std::ostream &os, const Event &) { os << '?'; }

bool AsyncActor::Event::isE2ECapable(const char *&, EventE2ESerializeFunction &, EventE2EDeserializeFunction &)
{
    return false;
}

void AsyncActor::Event::toOStream(std::ostream &os, const Event::OStreamName &eventName)
{
    assert(eventName.event.classId < MAX_EVENT_ID_COUNT);
    assert(AsyncNodeBase::staticShared.eventToStreamFunctions[eventName.event.classId].eventNameToOStreamFunction != 0);
    (*AsyncNodeBase::staticShared.eventToStreamFunctions[eventName.event.classId].eventNameToOStreamFunction)(
        os, eventName.event);
}

void AsyncActor::Event::toOStream(std::ostream &os, const Event::OStreamContent &eventContent)
{
    assert(eventContent.event.classId < MAX_EVENT_ID_COUNT);
    assert(
        AsyncNodeBase::staticShared.eventToStreamFunctions[eventContent.event.classId].eventContentToOStreamFunction !=
        0);
    (*AsyncNodeBase::staticShared.eventToStreamFunctions[eventContent.event.classId].eventContentToOStreamFunction)(
        os, eventContent.event);
}

AsyncActor::CoreId AsyncActor::ActorId::getCore() const
{
    assert(isInProcess());
    if (isInProcess() == false)
    {
        throw NotInProcessException();
    }
    return AsyncEngine::getEngine().getCoreSet().at(nodeId);
}

AsyncActor::AsyncActor()
    : AsynActorBase(), eventTable((assert(asyncNode != 0), asyncNode->retainEventTable(*this))),
      singletonActorIndex(AsyncNodeBase::StaticShared::SINGLETON_ACTOR_INDEX_SIZE),
      actorId(asyncNode->id, eventTable.nodeActorId, &eventTable), chain(0), onUnreachableChain(0),
      referenceFromCount(0), m_DestroyRequestedFlag(false), onUnreferencedDestroyFlag(false), processOutPipeCount(0)
#ifndef NDEBUG
      ,
      debugPipeCount(0)
#endif
{
    (chain = &asyncNode->asyncActorChain)->push_back(this);
    asyncNode->onNodeActorCountChange(1);
}

AsyncActor::AsyncActor(const AsyncActor &)
    : AsynActorBase(), super(), eventTable((assert(asyncNode != 0), asyncNode->retainEventTable(*this))),
      singletonActorIndex(AsyncNodeBase::StaticShared::SINGLETON_ACTOR_INDEX_SIZE),
      actorId(asyncNode->id, eventTable.nodeActorId, &eventTable), chain(0), onUnreachableChain(0),
      referenceFromCount(0), m_DestroyRequestedFlag(false), onUnreferencedDestroyFlag(false), processOutPipeCount(0)
#ifndef NDEBUG
      ,
      debugPipeCount(0)
#endif
{
    (chain = &asyncNode->asyncActorChain)->push_back(this);
    asyncNode->onNodeActorCountChange(1);
}

AsyncActor::~AsyncActor() noexcept
{
    assert(debugPipeCount == 0);
    assert(processOutPipeCount == 0);
    assert(referenceFromCount == 0);
    if (chain == &asyncNode->asyncActorChain)
    {
        assert(!m_DestroyRequestedFlag);
        chain->remove(this);
    }
    else
    {
        assert(m_DestroyRequestedFlag);
        assert(chain == &asyncNode->destroyedAsyncActorChain);
    }
    if (onUnreachableChain != 0)
    {
        assert(onUnreachableChain == &asyncNode->asyncActorOnUnreachableChain);
        onUnreachableChain->remove(this);
    }
    asyncNode->releaseEventTable(eventTable);
    while (!referenceToChain.empty())
    {
        referenceToChain.front()->unchain(referenceToChain);
    }
    asyncNode->onNodeActorCountChange(-1);
}

AsyncActor::CoreId AsyncActor::getCore() const noexcept
{
    return asyncNode->nodeManager.getCoreSet().at(actorId.nodeId);
}

AsyncEngine &AsyncActor::getEngine() noexcept { return AsyncEngine::getEngine(); }

const AsyncEngine &AsyncActor::getEngine() const noexcept { return AsyncEngine::getEngine(); }

AsyncActor::AllocatorBase AsyncActor::getAllocator() const noexcept { return AllocatorBase(asyncNode->nodeAllocator); }

AsyncActor::SingletonActorIndex AsyncActor::retainSingletonActorIndex()
{ // throw (std::bad_alloc)
    assert(std::numeric_limits<SingletonActorIndex>::max() >= AsyncNodeBase::StaticShared::SINGLETON_ACTOR_INDEX_SIZE);
    Mutex::Lock lock(AsyncNodeBase::staticShared.mutex);
    SingletonActorIndex singletonActorIndex = 0;
    for (; singletonActorIndex < AsyncNodeBase::staticShared.singletonActorIndexBitSet.size() &&
           AsyncNodeBase::staticShared.singletonActorIndexBitSet[singletonActorIndex];
         ++singletonActorIndex)
    {
    }
    if (singletonActorIndex == AsyncNodeBase::staticShared.singletonActorIndexBitSet.size())
    {
        throw UndersizedException(UndersizedException::SINGLETON_ACTOR_INDEX);
    }
    AsyncNodeBase::staticShared.singletonActorIndexBitSet[singletonActorIndex] = true;
    return singletonActorIndex;
}

void AsyncActor::releaseSingletonActorIndex(SingletonActorIndex singletonActorIndex) noexcept
{
    Mutex::Lock lock(AsyncNodeBase::staticShared.mutex);
    assert(singletonActorIndex < AsyncNodeBase::staticShared.singletonActorIndexBitSet.size());
    assert(AsyncNodeBase::staticShared.singletonActorIndexBitSet[singletonActorIndex]);
    AsyncNodeBase::staticShared.singletonActorIndexBitSet[singletonActorIndex] = false;
}

void AsyncActor::onUnreachable(const ActorId::RouteIdComparable &) {}

void AsyncActor::onDestroyRequest() noexcept
{
    // flag for (later) destruction
    assert(!m_DestroyRequestedFlag);
    m_DestroyRequestedFlag = true;
}

void AsyncActor::acceptDestroy(void) noexcept
{
    AsyncActor::onDestroyRequest();
}

void AsyncActor::requestDestroy() noexcept
{
    onUnreferencedDestroyFlag = true;
    if (referenceFromCount == 0 && chain != &asyncNode->destroyedAsyncActorChain)
    {
        chain->remove(this);
        (chain = &asyncNode->destroyedAsyncActorChain)->push_back(this);
    }
}

AsyncEngineEventLoop &AsyncActor::getEventLoop() const noexcept { return *asyncNode->eventLoop; }

const AsyncActor::CorePerformanceCounters &AsyncActor::getCorePerformanceCounters() const noexcept
{
    return asyncNode->corePerformanceCounters;
}

/**
 * throw (std::bad_alloc)
 */
uint8_t AsyncActor::registerLowPriorityEventHandler(void *pregisteredEventArray, EventId eventId, void *eventHandler,
                                                    bool (*staticEventHandler)(void *, const Event &))
{
    assert(eventId < MAX_EVENT_ID_COUNT);
    EventTable::RegisteredEvent *&registeredEventArray =
        *static_cast<EventTable::RegisteredEvent **>(pregisteredEventArray);
    if (registeredEventArray == 0)
    {
        registeredEventArray = static_cast<EventTable::RegisteredEvent *>(asyncNode->nodeAllocator.allocate(
            EventTable::LOW_FREQUENCY_ARRAY_ALIGNEMENT * sizeof(EventTable::RegisteredEvent)));
        registeredEventArray->eventId = MAX_EVENT_ID_COUNT;
    }
    size_t i = 0;
    for (; registeredEventArray[i].eventId != MAX_EVENT_ID_COUNT && registeredEventArray[i].eventId != eventId; ++i)
    {
        assert(i < EventTable::lfRegisteredEventArraySize(registeredEventArray));
    }
    if (registeredEventArray[i].eventId == eventId)
    {
        assert(registeredEventArray[i].staticEventHandler != &EventTable::onUnregisteredEvent ||
               (registeredEventArray[i].staticEventHandler == &EventTable::onUnregisteredEvent &&
                registeredEventArray[i].eventHandler == 0));
        uint8_t ret = (registeredEventArray[i].staticEventHandler == &EventTable::onUnregisteredEvent ? 1 : 0);
        registeredEventArray[i].eventHandler = eventHandler;
        registeredEventArray[i].staticEventHandler = staticEventHandler;
        return ret;
    }
    else
    {
        size_t registeredEventArraySz = EventTable::lfRegisteredEventArraySize(registeredEventArray);
        if (i + 1 == registeredEventArraySz)
        {
            assert(registeredEventArraySz % EventTable::LOW_FREQUENCY_ARRAY_ALIGNEMENT == 0);
            EventTable::RegisteredEvent *newRegisteredEventArray =
                static_cast<EventTable::RegisteredEvent *>(asyncNode->nodeAllocator.allocate(
                    (registeredEventArraySz + EventTable::LOW_FREQUENCY_ARRAY_ALIGNEMENT) *
                    sizeof(EventTable::RegisteredEvent)));
            std::memcpy(newRegisteredEventArray, registeredEventArray,
                        registeredEventArraySz * sizeof(EventTable::RegisteredEvent));
            asyncNode->nodeAllocator.deallocate(registeredEventArraySz * sizeof(EventTable::RegisteredEvent),
                                                registeredEventArray);
            registeredEventArray = newRegisteredEventArray;
        }
        registeredEventArray[i].eventId = eventId;
        registeredEventArray[i].eventHandler = eventHandler;
        registeredEventArray[i].staticEventHandler = staticEventHandler;
        registeredEventArray[i + 1].eventId = MAX_EVENT_ID_COUNT;
        return 1;
    }
}

/**
 * throw (std::bad_alloc)
 */
void AsyncActor::registerLowPriorityEventHandler(EventId eventId, void *eventHandler,
                                                 bool (*staticEventHandler)(void *, const Event &))
{
    assert(!isRegisteredEventHandler(eventId));
    registerLowPriorityEventHandler(&eventTable.lfEvent, eventId, eventHandler, staticEventHandler);
}

/**
 * throw (std::bad_alloc)
 */
void AsyncActor::registerHighPriorityEventHandler(EventId eventId, void *eventHandler,
                                                  bool (*staticEventHandler)(void *, const Event &))
{
    assert(!isRegisteredEventHandler(eventId));
    EventTable::RegisteredEvent *hfEvent = eventTable.hfEvent;
    int i = 0;
    for (; i < EventTable::HIGH_FREQUENCY_CALLBACK_ARRAY_SIZE && hfEvent[i].eventId != eventId; ++i)
    {
    }
    if (i == EventTable::HIGH_FREQUENCY_CALLBACK_ARRAY_SIZE)
    {
        for (i = 0; i < EventTable::HIGH_FREQUENCY_CALLBACK_ARRAY_SIZE && hfEvent[i].eventId != MAX_EVENT_ID_COUNT; ++i)
        {
        }
        if (i == EventTable::HIGH_FREQUENCY_CALLBACK_ARRAY_SIZE)
        {
            registerLowPriorityEventHandler(&eventTable.lfEvent, eventId, eventHandler, staticEventHandler);
        }
        else
        {
            hfEvent[i].eventId = eventId;
            hfEvent[i].eventHandler = eventHandler;
            hfEvent[i].staticEventHandler = staticEventHandler;
        }
    }
    else
    {
        hfEvent[i].eventHandler = eventHandler;
        hfEvent[i].staticEventHandler = staticEventHandler;
    }
}

/**
 * throw (std::bad_alloc)
 */
void AsyncActor::registerUndeliveredEventHandler(EventId eventId, void *eventHandler,
                                                 bool (*staticEventHandler)(void *, const Event &))
{
    assert(!isRegisteredUndeliveredEventHandler(eventId));
    eventTable.undeliveredEventCount +=
        registerLowPriorityEventHandler(&eventTable.undeliveredEvent, eventId, eventHandler, staticEventHandler);
    assert(eventTable.debugCheckUndeliveredEventCount());
    assert(eventTable.undeliveredEventCount > 0);
    if (processOutPipeCount != 0 && onUnreachableChain == 0)
    {
        (onUnreachableChain = &asyncNode->asyncActorOnUnreachableChain)->push_back(this);
    }
}

uint8_t AsyncActor::unregisterLowPriorityEventHandler(void *pregisteredEvent, EventId eventId) noexcept
{
    assert(eventId < MAX_EVENT_ID_COUNT);
    if (pregisteredEvent != 0)
    {
        EventTable::RegisteredEvent *registeredEvent = static_cast<EventTable::RegisteredEvent *>(pregisteredEvent);
        int i = 0;
        for (; registeredEvent[i].eventId != MAX_EVENT_ID_COUNT && registeredEvent[i].eventId != eventId; ++i)
        {
        }
        if (registeredEvent[i].eventId != MAX_EVENT_ID_COUNT)
        {
            registeredEvent[i].eventHandler = 0;
            assert(registeredEvent[i].staticEventHandler != &EventTable::onUnregisteredEvent);
            registeredEvent[i].staticEventHandler = EventTable::onUnregisteredEvent;
            return 1;
        }
    }
    return 0;
}

void AsyncActor::unregisterLowPriorityEventHandlers(void *pregisteredEvent) noexcept
{
    if (pregisteredEvent != 0)
    {
        EventTable::RegisteredEvent *registeredEvent = static_cast<EventTable::RegisteredEvent *>(pregisteredEvent);
        for (int i = 0; registeredEvent[i].eventId != MAX_EVENT_ID_COUNT; ++i)
        {
            if (registeredEvent[i].eventHandler != 0)
            {
                registeredEvent[i].eventHandler = 0;
                assert(registeredEvent[i].staticEventHandler != &EventTable::onUnregisteredEvent);
                registeredEvent[i].staticEventHandler = EventTable::onUnregisteredEvent;
            }
            else
            {
                assert(registeredEvent[i].staticEventHandler == &EventTable::onUnregisteredEvent);
            }
        }
    }
}

bool AsyncActor::isRegisteredLowPriorityEventHandler(void *pregisteredEvent, EventId eventId) const noexcept
{
    assert(eventId < MAX_EVENT_ID_COUNT);
    if (pregisteredEvent != 0)
    {
        EventTable::RegisteredEvent *registeredEvent = static_cast<EventTable::RegisteredEvent *>(pregisteredEvent);
        int i = 0;
        for (; registeredEvent[i].eventId != MAX_EVENT_ID_COUNT && registeredEvent[i].eventId != eventId; ++i)
        {
        }
        return registeredEvent[i].eventId == eventId && registeredEvent[i].eventHandler != 0;
    }
    return false;
}

bool AsyncActor::isRegisteredHighPriorityEventHandler(EventId eventId) const noexcept
{
    EventTable::RegisteredEvent *hfEvent = eventTable.hfEvent;
    int i = 0;
    for (; i < EventTable::HIGH_FREQUENCY_CALLBACK_ARRAY_SIZE && hfEvent[i].eventId != eventId; ++i)
    {
    }
    return i != EventTable::HIGH_FREQUENCY_CALLBACK_ARRAY_SIZE;
}

void AsyncActor::unregisterHighPriorityEventHandler(EventId eventId) noexcept
{
    assert(eventId < MAX_EVENT_ID_COUNT);
    EventTable::RegisteredEvent *hfEvent = eventTable.hfEvent;
    int i = 0;
    for (; i < EventTable::HIGH_FREQUENCY_CALLBACK_ARRAY_SIZE && hfEvent[i].eventId != eventId; ++i)
    {
    }
    if (i < EventTable::HIGH_FREQUENCY_CALLBACK_ARRAY_SIZE)
    {
        hfEvent[i].eventId = MAX_EVENT_ID_COUNT;
    }
}

void AsyncActor::unregisterEventHandler(EventId eventId) noexcept
{
    unregisterHighPriorityEventHandler(eventId);
    unregisterLowPriorityEventHandler(eventTable.lfEvent, eventId);
}

void AsyncActor::unregisterUndeliveredEventHandler(EventId eventId) noexcept
{
    uint8_t n = unregisterLowPriorityEventHandler(eventTable.undeliveredEvent, eventId);
    assert(n == 0 || (n == 1 && eventTable.undeliveredEventCount > 0));
    eventTable.undeliveredEventCount -= n;
    assert(eventTable.debugCheckUndeliveredEventCount());
    if (onUnreachableChain != 0 && eventTable.undeliveredEventCount == 0)
    {
        onUnreachableChain->remove(this);
        onUnreachableChain = 0;
    }
}

void AsyncActor::unregisterAllEventHandlers() noexcept
{
    EventTable::RegisteredEvent *hfEvent = eventTable.hfEvent;
    for (int i = 0; i < EventTable::HIGH_FREQUENCY_CALLBACK_ARRAY_SIZE; ++i)
    {
        hfEvent[i].eventId = MAX_EVENT_ID_COUNT;
    }
    unregisterLowPriorityEventHandlers(eventTable.lfEvent);
    unregisterLowPriorityEventHandlers(eventTable.undeliveredEvent);
    eventTable.undeliveredEventCount = 0;
    assert(eventTable.debugCheckUndeliveredEventCount());
    if (onUnreachableChain != 0)
    {
        onUnreachableChain->remove(this);
        onUnreachableChain = 0;
    }
}

bool AsyncActor::isRegisteredEventHandler(EventId eventId) const noexcept
{
    return isRegisteredHighPriorityEventHandler(eventId) ||
           isRegisteredLowPriorityEventHandler(eventTable.lfEvent, eventId);
}

bool AsyncActor::isRegisteredUndeliveredEventHandler(EventId eventId) const noexcept
{
    return isRegisteredLowPriorityEventHandler(eventTable.undeliveredEvent, eventId);
}

AsyncActor *AsyncActor::getSingletonActor(SingletonActorIndex singletonActorIndex) noexcept
{
    assert(singletonActorIndex < AsyncNodeBase::StaticShared::SINGLETON_ACTOR_INDEX_SIZE);
    return asyncNode->singletonActorIndex[singletonActorIndex].asyncActor;
}

/**
 * throw (CircularReferenceException)
 */
void AsyncActor::reserveSingletonActor(SingletonActorIndex singletonActorIndex)
{
    assert(singletonActorIndex < AsyncNodeBase::StaticShared::SINGLETON_ACTOR_INDEX_SIZE);
    if (asyncNode->singletonActorIndex[singletonActorIndex].reservedFlag)
    {
        throw CircularReferenceException();
    }
    assert(asyncNode->singletonActorIndex[singletonActorIndex].asyncActor == 0);
    asyncNode->singletonActorIndex[singletonActorIndex].reservedFlag = true;
}

void AsyncActor::setSingletonActor(SingletonActorIndex singletonActorIndex, AsyncActor &actor) noexcept
{
    assert(singletonActorIndex < AsyncNodeBase::StaticShared::SINGLETON_ACTOR_INDEX_SIZE);
    assert(asyncNode->singletonActorIndex[singletonActorIndex].reservedFlag);
    assert(actor.singletonActorIndex == AsyncNodeBase::StaticShared::SINGLETON_ACTOR_INDEX_SIZE);
    actor.singletonActorIndex = singletonActorIndex;
    AsyncNodeBase::SingletonActorIndexEntry &singletonActorIndexEntry =
        asyncNode->singletonActorIndex[singletonActorIndex];
    singletonActorIndexEntry.asyncActor = &actor;
    singletonActorIndexEntry.reservedFlag = false;
    asyncNode->singletonActorIndexChain.push_back(&singletonActorIndexEntry);
}

void AsyncActor::unsetSingletonActor(SingletonActorIndex singletonActorIndex) noexcept
{
    assert(singletonActorIndex < AsyncNodeBase::StaticShared::SINGLETON_ACTOR_INDEX_SIZE);
    AsyncNodeBase::SingletonActorIndexEntry &singletonActorIndexEntry =
        asyncNode->singletonActorIndex[singletonActorIndex];
    if (singletonActorIndexEntry.asyncActor != 0)
    {
        asyncNode->singletonActorIndexChain.remove(&singletonActorIndexEntry);
    }
    singletonActorIndexEntry.asyncActor = 0;
    singletonActorIndexEntry.reservedFlag = false;
}

/**
 *	throw (ReferenceLocalActorException)
 */
AsyncActor &AsyncActor::getReferenceToLocalActor(const ActorId &pactorId)
{
    if (pactorId.isSameCoreAs(actorId) == false)
    {
        throw ReferenceLocalActorException(true);
    }
    assert(pactorId.eventTable != 0);
    const EventTable &otherEventTable = *pactorId.eventTable;
    if (pactorId.getNodeActorId() != otherEventTable.nodeActorId)
    {
        throw ReferenceLocalActorException(false);
    }
    assert(otherEventTable.asyncActor != 0);
    return *otherEventTable.asyncActor;
}

void AsyncActor::registerCallback(void (*ponCallback)(Callback &) noexcept, Callback &pcallback) noexcept
{
    pcallback.unregister();
    (pcallback.chain = &asyncNode->asyncActorCallbackChain)->push_back(&pcallback);
    pcallback.actorEventTable = &eventTable;
    pcallback.nodeActorId = eventTable.nodeActorId;
    pcallback.onCallback = ponCallback;
}

void AsyncActor::registerPerformanceNeutralCallback(void (*ponCallback)(Callback &) noexcept,
                                                    Callback &pcallback) noexcept
{
    pcallback.unregister();
    (pcallback.chain = &asyncNode->asyncActorPerformanceNeutralCallbackChain)->push_back(&pcallback);
    pcallback.actorEventTable = &eventTable;
    pcallback.nodeActorId = eventTable.nodeActorId;
    pcallback.onCallback = ponCallback;
}

AsyncActor::AllocatorBase::AllocatorBase(AsyncNode &asyncNode) noexcept : asyncNodeAllocator(&asyncNode.nodeAllocator)
{
}

#ifndef NDEBUG
const ThreadId &AsyncActor::AllocatorBase::debugGetThreadId() const noexcept
{
    assert(asyncNodeAllocator != 0);
    return asyncNodeAllocator->debugGetThreadId();
}
#endif

void *AsyncActor::AllocatorBase::allocate(size_t sz, const void *hint)
{
    assert(asyncNodeAllocator != 0);
    if (asyncNodeAllocator == 0)
    {
        throw std::bad_alloc();
    }
    return asyncNodeAllocator->allocate(sz, hint);
}

void AsyncActor::AllocatorBase::deallocate(size_t sz, void *p) noexcept
{
    return asyncNodeAllocator->deallocate(sz, p);
}

size_t AsyncActor::Event::AllocatorBase::max_size() const noexcept
{
    return factory == 0 ? 0 : AsyncEngine::getEngine().getEventAllocatorPageSizeByte();
}

AsyncActor::Event::Pipe::Pipe(AsyncActor &asyncActor, const ActorId &pdestinationActorId) noexcept
    : sourceActor(asyncActor),
      destinationActorId(pdestinationActorId.getNodeActorId() == 0 ? ActorId(sourceActor.getActorId().nodeId, 0, 0)
                                                                   : pdestinationActorId),
      asyncNode(*asyncActor.asyncNode),
      eventFactory(getEventFactory())
{
#ifndef NDEBUG
    ++sourceActor.debugPipeCount;
#endif
    assert((destinationActorId.getNodeActorId() != 0 && destinationActorId.eventTable != 0) ||
           (destinationActorId.nodeId == asyncActor.getActorId().nodeId && destinationActorId.getNodeActorId() == 0 &&
            destinationActorId.eventTable == 0));
    registerProcessOutPipe();
}

void AsyncActor::Event::Pipe::setDestinationActorId(const ActorId &pdestinationActorId) noexcept
{
    unregisterProcessOutPipe();
    destinationActorId = pdestinationActorId.getNodeActorId() == 0 ? ActorId(sourceActor.getActorId().nodeId, 0, 0)
                                                                   : pdestinationActorId;
    eventFactory = getEventFactory();
    assert((destinationActorId.getNodeActorId() != 0 && destinationActorId.eventTable != 0) ||
           (destinationActorId.nodeId == sourceActor.getActorId().nodeId && destinationActorId.getNodeActorId() == 0 &&
            destinationActorId.eventTable == 0));
    registerProcessOutPipe();
}

AsyncActor::Event::Pipe::EventFactory AsyncActor::Event::Pipe::getEventFactory() noexcept
{
    if (destinationActorId.isInProcess())
    {
        return EventFactory(&asyncNode.getReferenceToWriterShared(destinationActorId.nodeId).writeCache,
                            &Pipe::newInProcessEvent, &allocateInProcessEvent, &allocateInProcessEvent,
                            &batchInProcessEvent);
    }
    else if (destinationActorId.getRouteId().getNodeId() == sourceActor.getActorId().nodeId &&
             destinationActorId.getRouteId().nodeConnection->isAsyncEngineToEngineSharedMemoryConnectorFlag)
    {
        return EventFactory(this, &Pipe::newOutOfProcessSharedMemoryEvent, &allocateOutOfProcessSharedMemoryEvent,
                            &allocateOutOfProcessSharedMemoryEvent, &batchOutOfProcessSharedMemoryEvent);
    }
    else
    {
        return EventFactory(
            &asyncNode.getReferenceToWriterShared(destinationActorId.getRouteId().getNodeId()).writeCache,
            &Pipe::newOutOfProcessEvent, &allocateInProcessEvent, &allocateInProcessEvent, &batchInProcessEvent);
    }
}

void AsyncActor::Event::Pipe::registerProcessOutPipe() noexcept
{
    if (destinationActorId.isInProcess() == false &&
        (++sourceActor.processOutPipeCount, sourceActor.onUnreachableChain == 0) &&
        sourceActor.eventTable.undeliveredEventCount != 0)
    {
        (sourceActor.onUnreachableChain = &asyncNode.asyncActorOnUnreachableChain)->push_back(&sourceActor);
    }
}

#ifndef NDEBUG

// debug
AsyncActor::Event::Batch::Batch() noexcept : batchId(0), debugContext(0), debugBatchFn(0) {}

AsyncActor::Event::Batch::Batch(void *pdebugContext, DebugBatchFn pdebugBatchFn) noexcept
    : batchId((*pdebugBatchFn)(pdebugContext, true)),
      debugContext(pdebugContext),
      debugBatchFn(pdebugBatchFn)
{
}

bool AsyncActor::Event::Batch::debugCheckHasChanged() noexcept
{
    assert(debugContext != 0);
    uint64_t currentBatchId = (*debugBatchFn)(debugContext, false);
    return currentBatchId != std::numeric_limits<uint64_t>::max() && currentBatchId != batchId;
}
#endif

void *AsyncActor::Event::Pipe::newInProcessEvent(size_t sz, EventChain *&destinationEventChain, uintptr_t,
                                                 Event::route_offset_type &)
{
    destinationEventChain =
        &static_cast<AsyncNodesHandle::Shared::WriteCache *>(eventFactory.context)->toBeDeliveredEventChain;
    asyncNode.setWriteSignal(destinationActorId.nodeId);
    return static_cast<AsyncNodesHandle::Shared::WriteCache *>(eventFactory.context)->allocateEvent(sz);
}

void *AsyncActor::Event::Pipe::allocateInProcessEvent(size_t sz, void *destinationEventPipe)
{
    return static_cast<AsyncNodesHandle::Shared::WriteCache *>(destinationEventPipe)->allocateEvent(sz);
}

void *AsyncActor::Event::Pipe::allocateInProcessEvent(size_t sz, void *destinationEventPipe, uint32_t &eventPageIndex,
                                                      size_t &eventPageOffset)
{
    return static_cast<AsyncNodesHandle::Shared::WriteCache *>(destinationEventPipe)
        ->allocateEvent(sz, eventPageIndex, eventPageOffset);
}

#ifndef NDEBUG

// debug
uint64_t AsyncActor::Event::Pipe::batchInProcessEvent(void *destinationEventPipe, bool incrementFlag) noexcept
{
    if (incrementFlag)
    {
        static_cast<AsyncNodesHandle::Shared::WriteCache *>(destinationEventPipe)->batchIdIncrement = 1;
    }
    return static_cast<AsyncNodesHandle::Shared::WriteCache *>(destinationEventPipe)->batchId;
}

#else

// release

uint64_t AsyncActor::Event::Pipe::batchInProcessEvent(void *destinationEventPipe) noexcept
{
    static_cast<AsyncNodesHandle::Shared::WriteCache *>(destinationEventPipe)->batchIdIncrement = 1;
    return static_cast<AsyncNodesHandle::Shared::WriteCache *>(destinationEventPipe)->batchId;
}
#endif

void *AsyncActor::Event::Pipe::newOutOfProcessEvent(size_t sz, EventChain *&destinationEventChain,
                                                    uintptr_t eventOffset, Event::route_offset_type &routeOffset)
{
    return newOutOfProcessEvent(eventFactory.context, sz, destinationEventChain, eventOffset, routeOffset);
}

//---- STUBs for WIP engine-to-engine (should never get here) ------------------

void *AsyncActor::Event::Pipe::newOutOfProcessSharedMemoryEvent(size_t, EventChain *&, uintptr_t,
                                                                Event::route_offset_type &)
{
    breakThrow(FeatureNotImplementedException());
    return nullptr;
}

void *AsyncActor::Event::Pipe::newOutOfProcessEvent(void *, size_t, EventChain *&, uintptr_t,
                                                    Event::route_offset_type &)
{
    breakThrow(FeatureNotImplementedException());
    return nullptr;
}

void *AsyncActor::Event::Pipe::allocateOutOfProcessSharedMemoryEvent(size_t, void *, uint32_t &, size_t &)
{
    breakThrow(FeatureNotImplementedException());
    return nullptr;
}

void *AsyncActor::Event::Pipe::allocateOutOfProcessSharedMemoryEvent(size_t, void *)
{
    breakThrow(FeatureNotImplementedException());
    return 0;
}

#ifndef NDEBUG
uint64_t AsyncActor::Event::Pipe::batchOutOfProcessSharedMemoryEvent(void *, bool) noexcept
{
    TRZ_DEBUG_BREAK();
    return 0;
}
#else

uint64_t AsyncActor::Event::Pipe::batchOutOfProcessSharedMemoryEvent(void *) noexcept
{
    TRZ_DEBUG_BREAK()
    return 0;
}
#endif

} // namespace