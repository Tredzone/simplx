/**
 * @file actor.cpp
 * @brief actor class
 * @copyright 2013-2018 Tredzone (www.tredzone.com). All rights reserved.
 * Please see accompanying LICENSE file for licensing terms.
 */

#include <string>
#include <sstream>
#include <thread>

#include "trz/engine/internal/node.h"

using namespace std;

namespace tredzone
{

const int Actor::MAX_NODE_COUNT;
const int Actor::MAX_EVENT_ID_COUNT;

Actor::EventId Actor::Event::retainEventId(EventToOStreamFunction peventNameToOStreamFunction,
                                                     EventToOStreamFunction peventContentToOStreamFunction,
                                                     EventIsE2ECapableFunction peventIsE2ECapableFunction)
{ // throw (std::bad_alloc)
    assert(std::numeric_limits<EventId>::max() >= MAX_EVENT_ID_COUNT);
    Mutex::Lock lock(AsyncNodeBase::s_StaticShared.mutex);
    EventId eventId = 0;
    for (; eventId < AsyncNodeBase::s_StaticShared.eventIdBitSet.size() &&
           AsyncNodeBase::s_StaticShared.eventIdBitSet[eventId];
         ++eventId)
    {
    }
    if (eventId == AsyncNodeBase::s_StaticShared.eventIdBitSet.size())
    {
        throw UndersizedException(UndersizedException::EVENT_ID);
    }

    EventE2ESerializeFunction eventE2ESerializeFunction;
    EventE2EDeserializeFunction eventE2EDeserializeFunction;
    const char *eventE2EAbsoluteId;
    if (peventIsE2ECapableFunction(eventE2EAbsoluteId, eventE2ESerializeFunction, eventE2EDeserializeFunction))
    {
        AsyncNodeBase::s_StaticShared.absoluteEventIds.addEventId(eventId, eventE2EAbsoluteId);
    }

    AsyncNodeBase::s_StaticShared.eventIdBitSet[eventId] = true;
    AsyncNodeBase::s_StaticShared.eventToStreamFunctions[eventId].eventNameToOStreamFunction =
        peventNameToOStreamFunction;
    AsyncNodeBase::s_StaticShared.eventToStreamFunctions[eventId].eventContentToOStreamFunction =
        peventContentToOStreamFunction;
    AsyncNodeBase::s_StaticShared.eventIsE2ECapableFunction[eventId] = peventIsE2ECapableFunction;
    return eventId;
}
/*
    absoluteEventIds map is shared among all nodes of an engine
    integer event Ids may be recycled?
*/

void Actor::Event::releaseEventId(EventId eventId) noexcept
{
    Mutex::Lock lock(AsyncNodeBase::s_StaticShared.mutex);
    assert(eventId < AsyncNodeBase::s_StaticShared.eventIdBitSet.size());
    assert(AsyncNodeBase::s_StaticShared.eventIdBitSet[eventId]);

    EventE2ESerializeFunction eventE2ESerializeFunction;
    EventE2EDeserializeFunction eventE2EDeserializeFunction;
    const char *eventE2EAbsoluteId;
    if (AsyncNodeBase::s_StaticShared.eventIsE2ECapableFunction[eventId](eventE2EAbsoluteId, eventE2ESerializeFunction,
                                                                       eventE2EDeserializeFunction))
    {
        assert(AsyncNodeBase::s_StaticShared.absoluteEventIds.findEventId(eventE2EAbsoluteId).second == eventId);
        AsyncNodeBase::s_StaticShared.absoluteEventIds.removeEventId(eventId);
    }

    AsyncNodeBase::s_StaticShared.eventIdBitSet[eventId] = false;
    AsyncNodeBase::s_StaticShared.eventToStreamFunctions[eventId].eventNameToOStreamFunction = 0;
    AsyncNodeBase::s_StaticShared.eventToStreamFunctions[eventId].eventContentToOStreamFunction = 0;
    AsyncNodeBase::s_StaticShared.eventIsE2ECapableFunction[eventId] = 0;
}

std::pair<bool, Actor::EventId> Actor::Event::findEventId(const char *absoluteEventId) noexcept
{
    Mutex::Lock lock(AsyncNodeBase::s_StaticShared.mutex);
    return AsyncNodeBase::s_StaticShared.absoluteEventIds.findEventId(absoluteEventId);
}

bool Actor::Event::isE2ECapable(EventId eventId, const char *&absoluteEventId,
                                     EventE2ESerializeFunction &serializeFn, EventE2EDeserializeFunction &deserializeFn)
{
    Mutex::Lock lock(AsyncNodeBase::s_StaticShared.mutex);
    assert(eventId < AsyncNodeBase::s_StaticShared.eventIdBitSet.size());
    if (AsyncNodeBase::s_StaticShared.eventIdBitSet[eventId])
    {
        assert(AsyncNodeBase::s_StaticShared.eventIsE2ECapableFunction[eventId] != 0);
        return AsyncNodeBase::s_StaticShared.eventIsE2ECapableFunction[eventId](absoluteEventId, serializeFn,
                                                                              deserializeFn);
    }
    else
    {
        return false;
    }
}

bool Actor::Event::isE2ECapable(const char *absoluteEventId, EventId &eventId,
                                     EventE2ESerializeFunction &serializeFn, EventE2EDeserializeFunction &deserializeFn)
{
    Mutex::Lock lock(AsyncNodeBase::s_StaticShared.mutex);
    std::pair<bool, EventId> retFindEventId = AsyncNodeBase::s_StaticShared.absoluteEventIds.findEventId(absoluteEventId);
    assert(retFindEventId.first == false || retFindEventId.second < AsyncNodeBase::s_StaticShared.eventIdBitSet.size());
    if (retFindEventId.first && AsyncNodeBase::s_StaticShared.eventIdBitSet[eventId = retFindEventId.second])
    {
        assert(AsyncNodeBase::s_StaticShared.eventIsE2ECapableFunction[eventId] != 0);
        return AsyncNodeBase::s_StaticShared.eventIsE2ECapableFunction[eventId](absoluteEventId, serializeFn,
                                                                              deserializeFn);
    }
    else
    {
        return false;
    }
}

void Actor::Event::nameToOStream(std::ostream &os, const Event &event) { os << event.classId; }

void Actor::Event::contentToOStream(std::ostream &os, const Event &) { os << '?'; }

bool Actor::Event::isE2ECapable(const char *&, EventE2ESerializeFunction &, EventE2EDeserializeFunction &)
{
    return false;
}

void Actor::Event::toOStream(std::ostream &os, const Event::OStreamName &eventName)
{
    assert(eventName.event.classId < MAX_EVENT_ID_COUNT);
    assert(AsyncNodeBase::s_StaticShared.eventToStreamFunctions[eventName.event.classId].eventNameToOStreamFunction != 0);
    (*AsyncNodeBase::s_StaticShared.eventToStreamFunctions[eventName.event.classId].eventNameToOStreamFunction)(
        os, eventName.event);
}

void Actor::Event::toOStream(std::ostream &os, const Event::OStreamContent &eventContent)
{
    assert(eventContent.event.classId < MAX_EVENT_ID_COUNT);
    assert(
        AsyncNodeBase::s_StaticShared.eventToStreamFunctions[eventContent.event.classId].eventContentToOStreamFunction !=
        0);
    (*AsyncNodeBase::s_StaticShared.eventToStreamFunctions[eventContent.event.classId].eventContentToOStreamFunction)(
        os, eventContent.event);
}

Actor::CoreId Actor::ActorId::getCore() const
{
    assert(isInProcess());
    if (isInProcess() == false)
    {
        throw NotInProcessException();
    }
    return Engine::getEngine().getCoreSet().at(nodeId);
}

//---- vanilla CTOR ------------------------------------------------------------

    Actor::Actor()
        : ActorBase(), eventTable((assert(asyncNode != 0), asyncNode->retainEventTable(*this))),
          singletonActorIndex(AsyncNodeBase::StaticShared::SINGLETON_ACTOR_INDEX_SIZE),
          actorId(asyncNode->id, eventTable.nodeActorId, &eventTable), chain(0), onUnreachableChain(0),
          m_ReferenceFromCount(0), m_DestroyRequestedFlag(false), onUnreferencedDestroyFlag(false), processOutPipeCount(0)
    #ifndef NDEBUG
          , debugPipeCount(0)
    #endif
{
    (chain = &asyncNode->asyncActorChain)->push_back(this);
    
    onAdded(*asyncNode);
}

//---- Actor DTOR --------------------------------------------------------------

    Actor::~Actor() noexcept
{
    assert(debugPipeCount == 0);
    assert(processOutPipeCount == 0);
    assert(m_ReferenceFromCount == 0);        // make sure is not referenced FROM other actors
    
    if (chain == &asyncNode->asyncActorChain)
    {
        assert(!m_DestroyRequestedFlag);
        chain->remove(this);
    }
    else
    {
        assert(m_DestroyRequestedFlag);
        assert(chain == &asyncNode->destroyedActorChain);
    }
    
    if (onUnreachableChain != 0)
    {
        assert(onUnreachableChain == &asyncNode->actorOnUnreachableChain);
        onUnreachableChain->remove(this);
    }
    
    asyncNode->releaseEventTable(eventTable);
    
    // decrement all references to other actors
    while (!m_ReferenceToChain.empty())
    {
        m_ReferenceToChain.front()->unchain(m_ReferenceToChain);
    }
    
    onRemoved(*asyncNode);
}

//---- Append to Reference Log -------------------------------------------------

// static
void Actor::appendRefLog(AsyncNode* paN, std::string str)
{

    #ifdef TRACE_REF

        // micro-seoncd timestamp
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        std::ostringstream stm;
        
        stm << (ts.tv_sec * 1000000 + ts.tv_nsec / 1000) << ";" << str;
        
        paN->dbgRefLogStr += (stm.str());
    #else
        (void)paN;
        (void)str;
    #endif
}

//---- Dump Actor --------------------------------------------------------------

template<typename _T>
class TD;

string    Actor::ActorReferenceBase::Dump(void) const
{
    stringstream ss;

    ss << " m_SelfActor = " << cppDemangledTypeInfoName(typeid(*m_SelfActor)) << endl;

    ss << " count = " << m_SelfActor->m_ReferenceFromCount << endl;

    ss << " m_RefDestChain = " << endl;

    #if 1
    
        // for (auto it : *m_RefDestChain)
        for (Actor::ActorReferenceBase it : *m_RefDestChain)
        {
            // TD<decltype(it)>   devine;
            
            const ActorReferenceBase    *link = reinterpret_cast<const ActorReferenceBase*>(&it);
            
            ss << "  link = 0x" << hex << setw(16) << setfill('0') << link << " " << cppDemangledTypeInfoName(typeid(*link->m_SelfActor)) << endl;
        }
    
    #else
    
        // !doesn't work!
        /* resolves to
        MultiDoubleChain<Actor::ActorReferenceBase, 1u, 0u, MultiDoubleChainItemAccessor<Actor::ActorReferenceBase, 1u> >::iterator_base<MultiDoubleChain<Actor::ActorReferenceBase, 1u, 0u, MultiDoubleChainItemAccessor<Actor::ActorReferenceBase, 1u> >, Actor::ActorReferenceBase>
        */
        
        for (auto it = m_RefDestChain->begin(); it != m_RefDestChain->end(); it++)
        // for (MultiDoubleChain<Actor::ActorReferenceBase> it = m_RefDestChain->begin(); it != m_RefDestChain->end(); it++)
        {
            // TD<decltype(it)>   devine;
            
            const ActorReferenceBase    *link = reinterpret_cast<const ActorReferenceBase*>(&it);
            
            // crashes in demangler if pass *Actor
            ss << "  link = 0x" << hex << setw(16) << setfill('0') << link << " " << cppDemangledTypeInfoName(typeid(*link->m_SelfActor)) << endl;
        }
        
    #endif
    
    return ss.str();
}

Actor::CoreId Actor::getCore() const noexcept
{
    return asyncNode->nodeManager.getCoreSet().at(actorId.nodeId);
}

Engine &Actor::getEngine() noexcept { return Engine::getEngine(); }

const Engine &Actor::getEngine() const noexcept { return Engine::getEngine(); }

Actor::AllocatorBase Actor::getAllocator() const noexcept { return AllocatorBase(asyncNode->nodeAllocator); }

Actor::SingletonActorIndex Actor::retainSingletonActorIndex()
{ // throw (std::bad_alloc)
    assert(std::numeric_limits<SingletonActorIndex>::max() >= AsyncNodeBase::StaticShared::SINGLETON_ACTOR_INDEX_SIZE);
    Mutex::Lock lock(AsyncNodeBase::s_StaticShared.mutex);
    SingletonActorIndex singletonActorIndex = 0;
    for (; singletonActorIndex < AsyncNodeBase::s_StaticShared.singletonActorIndexBitSet.size() &&
           AsyncNodeBase::s_StaticShared.singletonActorIndexBitSet[singletonActorIndex];
         ++singletonActorIndex)
    {
    }
    if (singletonActorIndex == AsyncNodeBase::s_StaticShared.singletonActorIndexBitSet.size())
    {
        throw UndersizedException(UndersizedException::SINGLETON_ACTOR_INDEX);
    }
    AsyncNodeBase::s_StaticShared.singletonActorIndexBitSet[singletonActorIndex] = true;
    return singletonActorIndex;
}

void Actor::releaseSingletonActorIndex(SingletonActorIndex singletonActorIndex) noexcept
{
    Mutex::Lock lock(AsyncNodeBase::s_StaticShared.mutex);
    assert(singletonActorIndex < AsyncNodeBase::s_StaticShared.singletonActorIndexBitSet.size());
    assert(AsyncNodeBase::s_StaticShared.singletonActorIndexBitSet[singletonActorIndex]);
    AsyncNodeBase::s_StaticShared.singletonActorIndexBitSet[singletonActorIndex] = false;
}

void Actor::onUnreachable(const ActorId::RouteIdComparable &) {}

void Actor::onDestroyRequest() noexcept
{
    // flag for (later) destruction
    assert(!m_DestroyRequestedFlag);
    m_DestroyRequestedFlag = true;
}

void Actor::acceptDestroy(void) noexcept
{
    Actor::onDestroyRequest();
}

void Actor::requestDestroy() noexcept
{
    onUnreferencedDestroyFlag = true;
    if (m_ReferenceFromCount == 0 && chain != &asyncNode->destroyedActorChain)
    {
        TraceREF(asyncNode, __func__, "-1.-1", cppDemangledTypeInfoName(typeid(*this)), actorId, cppDemangledTypeInfoName(typeid(*this)))
        
        chain->remove(this);
        (chain = &asyncNode->destroyedActorChain)->push_back(this);
    }
}

EngineEventLoop &Actor::getEventLoop() const noexcept { return *asyncNode->eventLoop; }

const Actor::CorePerformanceCounters &Actor::getCorePerformanceCounters() const noexcept
{
    return asyncNode->corePerformanceCounters;
}

/**
 * throw (std::bad_alloc)
 */
uint8_t Actor::registerLowPriorityEventHandler(void *pregisteredEventArray, EventId eventId, void *eventHandler,
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
void Actor::registerLowPriorityEventHandler(EventId eventId, void *eventHandler,
                                                 bool (*staticEventHandler)(void *, const Event &))
{
    assert(!isRegisteredEventHandler(eventId));
    registerLowPriorityEventHandler(&eventTable.lfEvent, eventId, eventHandler, staticEventHandler);
}

/**
 * throw (std::bad_alloc)
 */
void Actor::registerHighPriorityEventHandler(EventId eventId, void *eventHandler,
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
void Actor::registerUndeliveredEventHandler(EventId eventId, void *eventHandler,
                                                 bool (*staticEventHandler)(void *, const Event &))
{
    assert(!isRegisteredUndeliveredEventHandler(eventId));
    eventTable.undeliveredEventCount +=
        registerLowPriorityEventHandler(&eventTable.undeliveredEvent, eventId, eventHandler, staticEventHandler);
    assert(eventTable.debugCheckUndeliveredEventCount());
    assert(eventTable.undeliveredEventCount > 0);
    if (processOutPipeCount != 0 && onUnreachableChain == 0)
    {
        (onUnreachableChain = &asyncNode->actorOnUnreachableChain)->push_back(this);
    }
}

uint8_t Actor::unregisterLowPriorityEventHandler(void *pregisteredEvent, EventId eventId) noexcept
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

void Actor::unregisterLowPriorityEventHandlers(void *pregisteredEvent) noexcept
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

bool Actor::isRegisteredLowPriorityEventHandler(void *pregisteredEvent, EventId eventId) const noexcept
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

bool Actor::isRegisteredHighPriorityEventHandler(EventId eventId) const noexcept
{
    EventTable::RegisteredEvent *hfEvent = eventTable.hfEvent;
    int i = 0;
    for (; i < EventTable::HIGH_FREQUENCY_CALLBACK_ARRAY_SIZE && hfEvent[i].eventId != eventId; ++i)
    {
    }
    return i != EventTable::HIGH_FREQUENCY_CALLBACK_ARRAY_SIZE;
}

void Actor::unregisterHighPriorityEventHandler(EventId eventId) noexcept
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

void Actor::unregisterEventHandler(EventId eventId) noexcept
{
    unregisterHighPriorityEventHandler(eventId);
    unregisterLowPriorityEventHandler(eventTable.lfEvent, eventId);
}

void Actor::unregisterUndeliveredEventHandler(EventId eventId) noexcept
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

void Actor::unregisterAllEventHandlers() noexcept
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

bool Actor::isRegisteredEventHandler(EventId eventId) const noexcept
{
    return isRegisteredHighPriorityEventHandler(eventId) ||
           isRegisteredLowPriorityEventHandler(eventTable.lfEvent, eventId);
}

bool Actor::isRegisteredUndeliveredEventHandler(EventId eventId) const noexcept
{
    return isRegisteredLowPriorityEventHandler(eventTable.undeliveredEvent, eventId);
}

Actor *Actor::getSingletonActor(SingletonActorIndex singletonActorIndex) noexcept
{
    assert(singletonActorIndex < AsyncNodeBase::StaticShared::SINGLETON_ACTOR_INDEX_SIZE);
    return asyncNode->singletonActorIndex[singletonActorIndex].asyncActor;
}

/**
 * throw (CircularReferenceException)
 */
void Actor::reserveSingletonActor(SingletonActorIndex singletonActorIndex)
{
    assert(singletonActorIndex < AsyncNodeBase::StaticShared::SINGLETON_ACTOR_INDEX_SIZE);
    if (asyncNode->singletonActorIndex[singletonActorIndex].reservedFlag)
    {
        throw CircularReferenceException();
    }
    assert(asyncNode->singletonActorIndex[singletonActorIndex].asyncActor == 0);
    asyncNode->singletonActorIndex[singletonActorIndex].reservedFlag = true;
}

void Actor::setSingletonActor(SingletonActorIndex singletonActorIndex, Actor &actor) noexcept
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

void Actor::unsetSingletonActor(SingletonActorIndex singletonActorIndex) noexcept
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
Actor &Actor::getReferenceToLocalActor(const ActorId &pactorId)
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

void Actor::registerCallback(void (*ponCallback)(Callback &) noexcept, Callback &pcallback) noexcept
{
    pcallback.unregister();
    (pcallback.chain = &asyncNode->asyncActorCallbackChain)->push_back(&pcallback);
    pcallback.actorEventTable = &eventTable;
    pcallback.nodeActorId = eventTable.nodeActorId;
    pcallback.onCallback = ponCallback;
}

void Actor::registerPerformanceNeutralCallback(void (*ponCallback)(Callback &) noexcept,
                                                    Callback &pcallback) noexcept
{
    pcallback.unregister();
    (pcallback.chain = &asyncNode->asyncActorPerformanceNeutralCallbackChain)->push_back(&pcallback);
    pcallback.actorEventTable = &eventTable;
    pcallback.nodeActorId = eventTable.nodeActorId;
    pcallback.onCallback = ponCallback;
}

Actor::AllocatorBase::AllocatorBase(AsyncNode &asyncNode) noexcept : asyncNodeAllocator(&asyncNode.nodeAllocator)
{
}

#ifndef NDEBUG
const ThreadId &Actor::AllocatorBase::debugGetThreadId() const noexcept
{
    assert(asyncNodeAllocator != 0);
    return asyncNodeAllocator->debugGetThreadId();
}
#endif

void *Actor::AllocatorBase::allocate(size_t sz, const void *hint)
{
    assert(asyncNodeAllocator != 0);
    
    if (asyncNodeAllocator == 0)
    {
        throw std::bad_alloc();
    }
    return asyncNodeAllocator->allocate(sz, hint);
}

void Actor::AllocatorBase::deallocate(size_t sz, void *p) noexcept
{
    return asyncNodeAllocator->deallocate(sz, p);
}

size_t Actor::Event::AllocatorBase::max_size() const noexcept
{
    return factory == 0 ? 0 : Engine::getEngine().getEventAllocatorPageSizeByte();
}

Actor::Event::Pipe::Pipe(Actor &asyncActor, const ActorId &pdestinationActorId) noexcept
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

void Actor::Event::Pipe::setDestinationActorId(const ActorId &pdestinationActorId) noexcept
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

Actor::Event::Pipe::EventFactory Actor::Event::Pipe::getEventFactory() noexcept
{
    if (destinationActorId.isInProcess())
    {
        return EventFactory(&asyncNode.getReferenceToWriterShared(destinationActorId.nodeId).writeCache,
                            &Pipe::newInProcessEvent, &allocateInProcessEvent, &allocateInProcessEvent,
                            &batchInProcessEvent);
    }
    else if (destinationActorId.getRouteId().getNodeId() == sourceActor.getActorId().nodeId &&
             destinationActorId.getRouteId().nodeConnection->isEngineToEngineSharedMemoryConnectorFlag)
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

void Actor::Event::Pipe::registerProcessOutPipe() noexcept
{
    if (destinationActorId.isInProcess() == false &&
        (++sourceActor.processOutPipeCount, sourceActor.onUnreachableChain == 0) &&
        sourceActor.eventTable.undeliveredEventCount != 0)
    {
        (sourceActor.onUnreachableChain = &asyncNode.actorOnUnreachableChain)->push_back(&sourceActor);
    }
}

#ifndef NDEBUG

// debug
Actor::Event::Batch::Batch() noexcept : batchId(0), debugContext(0), debugBatchFn(0) {}

Actor::Event::Batch::Batch(void *pdebugContext, DebugBatchFn pdebugBatchFn) noexcept
    : batchId((*pdebugBatchFn)(pdebugContext, true)),
      debugContext(pdebugContext),
      debugBatchFn(pdebugBatchFn)
{
}

bool Actor::Event::Batch::debugCheckHasChanged() noexcept
{
    assert(debugContext != 0);
    uint64_t currentBatchId = (*debugBatchFn)(debugContext, false);
    return currentBatchId != std::numeric_limits<uint64_t>::max() && currentBatchId != batchId;
}
#endif

void *Actor::Event::Pipe::newInProcessEvent(size_t sz, EventChain *&destinationEventChain, uintptr_t,
                                                 Event::route_offset_type &)
{
    destinationEventChain =
        &static_cast<AsyncNodesHandle::Shared::WriteCache *>(eventFactory.context)->toBeDeliveredEventChain;
    asyncNode.setWriteSignal(destinationActorId.nodeId);
    return static_cast<AsyncNodesHandle::Shared::WriteCache *>(eventFactory.context)->allocateEvent(sz);
}

void *Actor::Event::Pipe::allocateInProcessEvent(size_t sz, void *destinationEventPipe)
{
    return static_cast<AsyncNodesHandle::Shared::WriteCache *>(destinationEventPipe)->allocateEvent(sz);
}

void *Actor::Event::Pipe::allocateInProcessEvent(size_t sz, void *destinationEventPipe, uint32_t &eventPageIndex,
                                                      size_t &eventPageOffset)
{
    return static_cast<AsyncNodesHandle::Shared::WriteCache *>(destinationEventPipe)
        ->allocateEvent(sz, eventPageIndex, eventPageOffset);
}

#ifndef NDEBUG

// debug
uint64_t Actor::Event::Pipe::batchInProcessEvent(void *destinationEventPipe, bool incrementFlag) noexcept
{
    if (incrementFlag)
    {
        static_cast<AsyncNodesHandle::Shared::WriteCache *>(destinationEventPipe)->batchIdIncrement = 1;
    }
    return static_cast<AsyncNodesHandle::Shared::WriteCache *>(destinationEventPipe)->batchId;
}

#else

// release

uint64_t Actor::Event::Pipe::batchInProcessEvent(void *destinationEventPipe) noexcept
{
    static_cast<AsyncNodesHandle::Shared::WriteCache *>(destinationEventPipe)->batchIdIncrement = 1;
    return static_cast<AsyncNodesHandle::Shared::WriteCache *>(destinationEventPipe)->batchId;
}
#endif

// static
bool Actor::ActorReferenceBase::recursiveFind(const Actor &referencedActor, const Actor &referencingActor)
{
    return referencedActor.asyncNode->m_RefMapper.recursiveFind(referencedActor, referencingActor);
}

size_t  Actor::CountReferencesTo(void) const
{
    size_t  n = 0;
    
    for (const auto &it : m_ReferenceToChain)
    {
            (void)it;
            
            n++;
    }
    
    return n;
}

size_t  Actor::ActorReferenceBase::CountRefDest(void) const
{
    size_t  n = 0;
    
    for (const auto &it : *m_RefDestChain)
    {
            (void)it;
            n++;
    }
    
    return n;
}

size_t  Actor::CountRefDest(void) const
{
    return ((Actor::ActorReferenceBase*)this)->CountRefDest();
}

void    Actor::onAdded(AsyncNode &nod)
{
    nod.onActorAdded(this);
}

void    Actor::onRemoved(AsyncNode &nod)
{
    nod.onActorRemoved(this);
}

} // namespace