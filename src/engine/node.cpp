/**
 * @file node.cpp
 * @brief Simplx node class
 * @copyright 2013-2018 Tredzone (www.tredzone.com). All rights reserved.
 * Please see accompanying LICENSE file for licensing terms.
 */

#include <iostream>
#include <fstream>

#include "trz/engine/internal/node.h"

using namespace std;

namespace tredzone
{

#ifndef NDEBUG
volatile bool AsyncNodeAllocator::debugActivateMemoryLeakBacktraceFlag = false;
#endif

AsyncNodeBase::StaticShared AsyncNodeBase::s_StaticShared;

Actor::EventTable::EventTable(void *pdeallocatePointer) noexcept : nodeActorId(0),
                                                                        asyncActor(0),
                                                                        lfEvent(0),
                                                                        undeliveredEvent(0),
                                                                        undeliveredEventCount(0),
                                                                        nextUnused(0),
                                                                        deallocatePointer(pdeallocatePointer)
{
    CRITICAL_ASSERT((uintptr_t) this % CACHE_LINE_SIZE == 0);
}

//---- DTOR --------------------------------------------------------------------

    Actor::EventTable::~EventTable() noexcept
{
    assert(nodeActorId == 0);
    assert(asyncActor == 0);
    assert(lfEvent == 0);
    assert(undeliveredEvent == 0);
    assert(undeliveredEventCount == 0);
}

void Actor::EventTable::onUndeliveredEvent(const Event &event) const
{
    if (undeliveredEvent != 0)
    {
        int i = 0;
        for (; undeliveredEvent[i].eventId != MAX_EVENT_ID_COUNT && event.getClassId() != undeliveredEvent[i].eventId;
             ++i)
        {
        }
        if (undeliveredEvent[i].eventId != MAX_EVENT_ID_COUNT)
        {
            assert(undeliveredEvent[i].staticEventHandler != 0);
            (*undeliveredEvent[i].staticEventHandler)(undeliveredEvent[i].eventHandler, event);
        }
    }
}

bool Actor::EventTable::onLowFrequencyEvent(const Event &event, uint64_t &performanceCounter) const
{
    if (lfEvent == 0)
    {
        return false;
    }
    int i = 0;
    for (; lfEvent[i].eventId != MAX_EVENT_ID_COUNT && event.getClassId() != lfEvent[i].eventId; ++i)
    {
    }
    if (lfEvent[i].eventId == MAX_EVENT_ID_COUNT)
    {
        return false;
    }
    ++performanceCounter;
    assert(lfEvent[i].staticEventHandler != 0);
    return (*lfEvent[i].staticEventHandler)(lfEvent[i].eventHandler, event);
}

size_t Actor::EventTable::lfRegisteredEventArraySize(RegisteredEvent *registeredEvent) noexcept
{
    size_t i = 0;
    for (; registeredEvent[i].eventId != MAX_EVENT_ID_COUNT; ++i)
    {
    }
    return LOW_FREQUENCY_ARRAY_ALIGNEMENT * ((i + LOW_FREQUENCY_ARRAY_ALIGNEMENT) / LOW_FREQUENCY_ARRAY_ALIGNEMENT);
}

#ifndef NDEBUG
    bool Actor::EventTable::onUnregisteredEvent(void *eventHandler, const Event &)
    {
        assert(eventHandler == 0);
        return false;
    }

    bool Actor::EventTable::debugCheckUndeliveredEventCount() const noexcept
    {
        size_t n = 0;
        if (undeliveredEvent != 0)
        {
            for (size_t i = 0; undeliveredEvent[i].eventId != MAX_EVENT_ID_COUNT; ++i)
            {
                assert((undeliveredEvent[i].staticEventHandler != &onUnregisteredEvent &&
                        undeliveredEvent[i].eventHandler != 0) ||
                       (undeliveredEvent[i].staticEventHandler == &onUnregisteredEvent &&
                        undeliveredEvent[i].eventHandler == 0));
                if (undeliveredEvent[i].staticEventHandler != &onUnregisteredEvent)
                {
                    ++n;
                }
            }
        }
        return n == undeliveredEventCount;
    }
#else
    bool Actor::EventTable::onUnregisteredEvent(void *, const Event &) { return false; }
#endif

//---- Nodes Handle CTOR -------------------------------------------------------

    AsyncNodesHandle::AsyncNodesHandle(const std::pair<size_t, const CoreSet *> &init)
        : size(init.second->size()), eventAllocatorPageSize(init.first), activeNodeHandles(size, false), nodeHandles(size)
{
    cpuset_type currentThreadAffinity = threadGetAffinity();
    for (size_t i = 0; i < size; ++i)
    {
        try
        {
            threadSetAffinity(init.second->at(
                (NodeId)i)); // For NUMA to allocate nodeHandle in the correponding core NUMA memory node
            nodeHandles[i].init(*this, *init.second);
        }
        catch (...)
        {
            threadSetAffinity(currentThreadAffinity);
            throw;
        }
    }
    threadSetAffinity(currentThreadAffinity);

    for (NodeId i = 0; i < size; ++i)
    {
        for (NodeId j = 0; j < size; ++j)
        {
            nodeHandles[i]->readerSharedHandles[j].init(nodeHandles[j]->writerSharedHandles[i]);
            nodeHandles[i]->writerSharedHandles[j].init(i, *nodeHandles[j], *nodeHandles[i],
                                                        nodeHandles[j]->readerSharedHandles[i]);
        }
    }
}

AsyncNodesHandle::ReaderSharedHandle::ReaderSharedHandle() noexcept
{
    CRITICAL_ASSERT((uintptr_t)&cl1 % CACHE_LINE_SIZE == 0);
    CRITICAL_ASSERT((uintptr_t)&cl2 % CACHE_LINE_SIZE == 0);
    CRITICAL_ASSERT(sizeof(CacheLine1) <= (size_t)CACHE_LINE_SIZE);
    CRITICAL_ASSERT(sizeof(CacheLine2) <= (size_t)CACHE_LINE_SIZE);
}

void AsyncNodesHandle::ReaderSharedHandle::init(WriterSharedHandle &writerSharedHandle) noexcept
{
    cl1.isReaderActive = &writerSharedHandle.cl2.isReaderActive;
    cl1.readerCAS = &writerSharedHandle.cl2.readerCAS;
    cl1.sharedReadWriteLocked = &writerSharedHandle.cl2.shared.readWriteLocked;
}

void AsyncNodesHandle::ReaderSharedHandle::dispatchUnreachableNodes(
    Actor::OnUnreachableChain &actorOnUnreachableChain,
    Shared::UnreachableNodeConnectionChain &sharedUnreachableNodeConnectionChain, NodeId writerNodeId,
    AsyncExceptionHandler &asyncExceptionHandler) noexcept
{
    assert(!actorOnUnreachableChain.empty());
    assert(!sharedUnreachableNodeConnectionChain.empty());
    Actor::OnUnreachableChain localActorOnUnreachableChain;
    localActorOnUnreachableChain.swap(actorOnUnreachableChain);
    assert(actorOnUnreachableChain.empty());
    for (Actor::OnUnreachableChain::iterator i = localActorOnUnreachableChain.begin(),
                                                  endi = localActorOnUnreachableChain.end();
         i != endi; ++i)
    {
        assert(i->onUnreachableChain == &actorOnUnreachableChain);
        i->onUnreachableChain = &localActorOnUnreachableChain;
    }
    while (!localActorOnUnreachableChain.empty())
    {
        Actor *asyncActor = localActorOnUnreachableChain.pop_front();
        assert(asyncActor->eventTable.undeliveredEventCount > 0);
        assert(asyncActor->onUnreachableChain == &localActorOnUnreachableChain);
        (asyncActor->onUnreachableChain = &actorOnUnreachableChain)->push_back(asyncActor);
        for (Shared::UnreachableNodeConnectionChain::iterator i = sharedUnreachableNodeConnectionChain.begin(),
                                                              endi = sharedUnreachableNodeConnectionChain.end();
             i != endi; ++i)
        {
            try
            {
                asyncActor->onUnreachable(Actor::ActorId::RouteIdComparable(writerNodeId, i->nodeConnectionId));
            }
            catch (std::exception &e)
            {
                asyncExceptionHandler.onUnreachableExceptionSynchronized(
                    *asyncActor, typeid(*asyncActor),
                    Actor::ActorId::RouteIdComparable(writerNodeId, i->nodeConnectionId), e.what());
            }
            catch (...)
            {
                asyncExceptionHandler.onUnreachableExceptionSynchronized(
                    *asyncActor, typeid(*asyncActor),
                    Actor::ActorId::RouteIdComparable(writerNodeId, i->nodeConnectionId), "unknown exception");
            }
        }
    }
}

//---- WriterSharedHandle CTOR -------------------------------------------------

    AsyncNodesHandle::WriterSharedHandle::WriterSharedHandle(size_t eventAllocatorPageSize) : cl2(eventAllocatorPageSize)
{

    CRITICAL_ASSERT((uintptr_t)&cl1 % CACHE_LINE_SIZE == 0);
    CRITICAL_ASSERT((uintptr_t)&cl2 % CACHE_LINE_SIZE == 0);
    CRITICAL_ASSERT((uintptr_t)&cl2.shared.readWriteLocked % CACHE_LINE_SIZE == 0);
    CRITICAL_ASSERT(sizeof(CacheLine1) <= (size_t)CACHE_LINE_SIZE);
    CRITICAL_ASSERT(sizeof(CacheLine2) % CACHE_LINE_SIZE == 0);
}

void AsyncNodesHandle::WriterSharedHandle::init(NodeId writerNodeId, NodeHandle &readerNodeHandle,
                                                NodeHandle &writerNodeHandle,
                                                ReaderSharedHandle &readerSharedHandle) noexcept
{
    cl1.isWriterActive = &readerSharedHandle.cl2.isWriterActive;
    cl1.isWriteLocked = &readerSharedHandle.cl2.isWriteLocked;
    cl1.writerNodeHandle = &writerNodeHandle;
    cl2.shared.readWriteLocked.writerNodeId = writerNodeId;
    cl2.shared.readWriteLocked.readerNodeHandle = &readerNodeHandle;
}

AsyncNodesHandle::EventChain::iterator
AsyncNodesHandle::Shared::ReadWriteLocked::undeliveredEvent(const AsyncNodesHandle::EventChain::iterator &ievent,
                                                            AsyncNodesHandle::EventChain &eventChain) noexcept
{
    assert(readerNodeHandle != 0);
    assert(readerNodeHandle->node != 0);
    Actor::Event &event = *ievent;
    EventChain::iterator ret = eventChain.erase(ievent);
    undeliveredEventChain.push_back(&event);
    AsyncNode &node = *readerNodeHandle->node;
    NodeId eventSourceNodeId;
    if (event.isRouteToSource())
    {
        assert(event.getSourceActorId().getRouteId() == event.getRouteId());
        eventSourceNodeId = event.getRouteId().nodeId;
    }
    else
    {
        assert(event.getSourceActorId().isInProcess());
        eventSourceNodeId = event.getSourceInProcessActorId().nodeId;
    }
    node.getReferenceToWriterShared(eventSourceNodeId).writeCache.checkUndeliveredEventsFlag = true;
    node.setWriteSignal(eventSourceNodeId);
    return ret;
}

/**
 * throw (std::bad_alloc)
 */
AsyncNodesHandle::Shared::WriteCache::WriteCache(size_t peventAllocatorPageSize)
    : checkUndeliveredEventsFlag(false), batchIdIncrement(0), batchId(0), totalWrittenByteSize(0),
      eventAllocatorPageSize(peventAllocatorPageSize), frontUsedEventAllocatorPageChainOffset(0),
      nextEventAllocatorPageIndex(0)
{
    CRITICAL_ASSERT(nextEventAllocatorPageIndex + 1 != 0);
    freeEventAllocatorPageChain.push_back(new (eventAllocatorPageAllocator.insert(
        CACHE_LINE_SIZE + eventAllocatorPageSize)) EventAllocatorPage(nextEventAllocatorPageIndex));
    ++nextEventAllocatorPageIndex;
    CRITICAL_ASSERT(nextEventAllocatorPageIndex + 1 != 0);
    usedEventAllocatorPageChain.push_back(new (eventAllocatorPageAllocator.insert(
        CACHE_LINE_SIZE + eventAllocatorPageSize)) EventAllocatorPage(nextEventAllocatorPageIndex));
    ++nextEventAllocatorPageIndex;
}

/**
 * throw (std::bad_alloc)
 */
AsyncNodesHandle::Shared::Shared(size_t eventAllocatorPageSize) : writeCache(eventAllocatorPageSize)
{
    CRITICAL_ASSERT((uintptr_t)&readWriteLocked % CACHE_LINE_SIZE == 0);
}

/**
 * throw (std::bad_alloc)
 */
AsyncNodesHandle::NodeHandle::NodeHandle(const std::pair<AsyncNodesHandle *, const CoreSet *> &init)
    : readerSharedHandles(init.first->size), writerSharedHandles(init.first->size, init.first->eventAllocatorPageSize),
      node(0), nextHanlerId(1), stopFlag(false), shutdownFlag(false), interruptFlag(true), coreSet(*init.second)
#ifndef NDEBUG
      ,
      debugNodePtrWasSet(false)
#endif
{
    assert(coreSet.size() == init.first->size);
}

AsyncNodesHandle::NodeHandle::~NodeHandle() noexcept
{
    // assert(node == 0); // Silenced as to much defensive - typically over-stepping a failure in the thread preventing
    // call to delete node (required to comply with this assert) in AsyncNode::Thread::inThread()
}

void AsyncNodesHandle::WriterSharedHandle::writeFailed() noexcept
{
    cl2.shared.readWriteLocked.checkUndeliveredEventsFlag = cl2.shared.writeCache.checkUndeliveredEventsFlag = false;
    if (cl2.shared.readWriteLocked.deliveredEventsFlag)
    {
        cl2.shared.readWriteLocked.deliveredEventsFlag = false;
        writeDispatchAndClearUndeliveredEvents(cl2.shared.readWriteLocked.undeliveredEventChain);
        {
            EventChain emptyEventChain;
            cl2.shared.readWriteLocked.toBeDeliveredEventChain.swap(emptyEventChain);
        }
    }
    else
    {
        assert(cl2.shared.readWriteLocked.undeliveredEventChain.empty());
        writeDispatchAndClearUndeliveredEvents(cl2.shared.readWriteLocked.toBeDeliveredEventChain);
    }
    writeDispatchAndClearUndeliveredEvents(cl2.shared.writeCache.toBeDeliveredEventChain);
    cl2.shared.writeCache.freeEventAllocatorPageChain.push_back(cl2.shared.readWriteLocked.usedEventAllocatorPageChain);
    cl2.shared.writeCache.freeEventAllocatorPageChain.push_back(cl2.shared.writeCache.usedEventAllocatorPageChain);
    assert(!cl2.shared.writeCache.freeEventAllocatorPageChain.empty());
    cl2.shared.writeCache.usedEventAllocatorPageChain.push_front(
        cl2.shared.writeCache.freeEventAllocatorPageChain.pop_front());
    cl2.shared.writeCache.frontUsedEventAllocatorPageChainOffset = 0;
}

void AsyncNodesHandle::WriterSharedHandle::writeDispatchAndClearUndeliveredEvents(EventChain &eventChain) noexcept
{
    for (EventChain::iterator i = eventChain.begin(), endi = eventChain.end(); i != endi; ++i)
    {
        onUndeliveredEvent(*i);
    }
    {
        EventChain emptyEventChain;
        eventChain.swap(emptyEventChain);
    }
}

void AsyncNodesHandle::WriterSharedHandle::onUndeliveredEvent(const Actor::Event &event) noexcept
{
    assert(cl1.writerNodeHandle != 0);
    assert(cl1.writerNodeHandle->node != 0);
    if (event.isRouteToSource())
    {
        assert(event.getSourceActorId().getRouteId() == event.getRouteId());
        assert(event.getSourceActorId().getRouteId().nodeId == cl1.writerNodeHandle->node->id);
        assert(event.getSourceActorId().getRouteId().nodeId == cl2.shared.readWriteLocked.writerNodeId);
        const Actor::ActorId::RouteId &routeId = event.getRouteId();
        const Actor::NodeConnection *nodeConnection = routeId.nodeConnection;
        assert(nodeConnection != 0);
        if (routeId.getNodeConnectionId() == nodeConnection->nodeConnectionId)
        {
            try
            {
                nodeConnection->onInboundUndeliveredEventFn(nodeConnection->connector, event);
            }
            catch (std::exception &e)
            {
                assert(nodeConnection->connector != 0);
                cl1.writerNodeHandle->node->nodeManager.exceptionHandler.onEventExceptionSynchronized(
                    0, typeid(*nodeConnection->connector), "onInboundUndeliveredEventFn", event, e.what());
            }
            catch (...)
            {
                assert(nodeConnection->connector != 0);
                cl1.writerNodeHandle->node->nodeManager.exceptionHandler.onEventExceptionSynchronized(
                    0, typeid(*nodeConnection->connector), "onInboundUndeliveredEventFn", event, "unknown exception");
            }
        }
    }
    else
    {
        onUndeliveredEventToSourceActor(event);
    }
}

void AsyncNodesHandle::WriterSharedHandle::onUndeliveredEventToSourceActor(const Actor::Event &event) noexcept
{
    assert(cl1.writerNodeHandle != 0);
    assert(cl1.writerNodeHandle->node != 0);
    assert(!event.isRouteToSource());
    assert(event.getSourceInProcessActorId().nodeId == cl1.writerNodeHandle->node->id);
    assert(event.getSourceInProcessActorId().nodeId == cl2.shared.readWriteLocked.writerNodeId);
    const Actor::InProcessActorId &eventSourceActorId = event.getSourceInProcessActorId();
    const Actor::EventTable &eventTable = *eventSourceActorId.eventTable;
    if (eventSourceActorId.getNodeActorId() == eventTable.nodeActorId)
    {
        try
        {
            eventTable.onUndeliveredEvent(event);
        }
        catch (std::exception &e)
        {
            assert(eventTable.asyncActor != 0);
            cl1.writerNodeHandle->node->nodeManager.exceptionHandler.onEventExceptionSynchronized(
                eventTable.asyncActor, typeid(*eventTable.asyncActor), "onUndeliveredEvent", event, e.what());
        }
        catch (...)
        {
            assert(eventTable.asyncActor != 0);
            cl1.writerNodeHandle->node->nodeManager.exceptionHandler.onEventExceptionSynchronized(
                eventTable.asyncActor, typeid(*eventTable.asyncActor), "onUndeliveredEvent", event,
                "unknown exception");
        }
    }
}

void AsyncExceptionHandler::onEventException(Actor *, const std::type_info &asyncActorTypeInfo,
                                             const char *onXXX_FunctionName, const Actor::Event &event,
                                             const char *whatException) noexcept
{
    try
    {
        std::stringstream s;
        s << cppDemangledTypeInfoName(asyncActorTypeInfo) << "::" << onXXX_FunctionName << '(' << event << ") threw ("
          << whatException << ')' << std::endl;
        std::cout << s.str();
    }
    catch (...)
    {
        try
        {
            std::cout << asyncActorTypeInfo.name() << "::" << onXXX_FunctionName << '(' << event.classId << ") threw ("
                      << whatException << ')' << std::endl;
        }
        catch (...)
        {
        }
    }
#ifndef NDEBUG
    std::exit(-1);
#endif
}

void AsyncExceptionHandler::onUnreachableException(Actor &, const std::type_info &asyncActorTypeInfo,
                                                   const Actor::ActorId::RouteIdComparable &routeIdComparable,
                                                   const char *whatException) noexcept
{
    try
    {
        std::stringstream s;
        s << cppDemangledTypeInfoName(asyncActorTypeInfo) << "::onUnreachable("
          << (unsigned)routeIdComparable.getNodeId() << '-' << routeIdComparable.getNodeConnectionId() << ") threw ("
          << whatException << ')' << std::endl;
        std::cout << s.str();
    }
    catch (...)
    {
        try
        {
            std::cout << asyncActorTypeInfo.name() << "::onUnreachable(" << (unsigned)routeIdComparable.getNodeId()
                      << '-' << routeIdComparable.getNodeConnectionId() << ") threw (" << whatException << ')'
                      << std::endl;
        }
        catch (...)
        {
        }
    }
#ifndef NDEBUG
    std::exit(-1);
#endif
}

void AsyncExceptionHandler::onEventExceptionSynchronized(Actor *asyncActor,
                                                         const std::type_info &asyncActorTypeInfo,
                                                         const char *onXXX_FunctionName, const Actor::Event &event,
                                                         const char *whatException) noexcept
{
    Mutex::Lock lock(mutex);
    onEventException(asyncActor, asyncActorTypeInfo, onXXX_FunctionName, event, whatException);
}

void AsyncExceptionHandler::onUnreachableExceptionSynchronized(
    Actor &asyncActor, const std::type_info &asyncActorTypeInfo,
    const Actor::ActorId::RouteIdComparable &routeIdComparable, const char *whatException) noexcept
{
    Mutex::Lock lock(mutex);
    onUnreachableException(asyncActor, asyncActorTypeInfo, routeIdComparable, whatException);
}

/**
 * throw (std::bad_alloc)
 */
AsyncNodeManager::AsyncNodeManager(size_t eventAllocatorPageSize, const CoreSet &pcoreSet)
    : std::unique_ptr<AsyncExceptionHandler>(new AsyncExceptionHandler),
      Parallel<AsyncNodesHandle>(std::make_pair(eventAllocatorPageSize, &pcoreSet)), exceptionHandler(**this),
      coreSet(pcoreSet)
{
}

/**
 * throw (std::bad_alloc)
 */
AsyncNodeManager::AsyncNodeManager(AsyncExceptionHandler &pexceptionHandler, size_t eventAllocatorPageSize,
                                   const CoreSet &pcoreSet)
    : Parallel<AsyncNodesHandle>(std::make_pair(eventAllocatorPageSize, &pcoreSet)),
      exceptionHandler(pexceptionHandler), coreSet(pcoreSet)
{
}

void AsyncNodeManager::shutdown() noexcept
{
    for (size_t i = 0; i < nodesHandle.size; ++i)
    {
        nodesHandle.getNodeHandle((NodeId)i).shutdownFlag = true;
    }
    memoryBarrier();
    for (size_t i = 0; i < nodesHandle.size; ++i)
    {
        nodesHandle.getNodeHandle((NodeId)i).interruptFlag = true;
    }
    memoryBarrier();
}

    AsyncNodeBase::AsyncNodeBase(AsyncNodeManager &pnodeManager)
    : // throw (std::bad_alloc)
      singletonActorIndex(StaticShared::SINGLETON_ACTOR_INDEX_SIZE, SingletonActorIndexEntry(),
                          Actor::AllocatorBase(nodeAllocator)),
      m_ActorCount(0), freeEventTable(0), nodeManager(pnodeManager), lastNodeConnectionId(0),
      eventAllocatorPageSize(nodeManager.getEventAllocatorPageSize()), loopUsagePerformanceCounterIncrement(0)
{
    assert(std::numeric_limits<Actor::SingletonActorIndex>::max() >= StaticShared::SINGLETON_ACTOR_INDEX_SIZE);
}

//---- AsyncNodeBASE DTOR ------------------------------------------------------

    AsyncNodeBase::~AsyncNodeBase() noexcept
{
    assert(singletonActorIndexChain.empty());
    assert(m_ActorCount == 0);
    assert(asyncActorChain.empty());
    assert(destroyedActorChain.empty());
    assert(actorOnUnreachableChain.empty());
    assert(inUseNodeConnectionChain.empty());
    while (!m_NodeActorCountListenerChain.empty())
    {
        m_NodeActorCountListenerChain.front()->unsubscribe();
    }
    while (!asyncActorCallbackChain.empty())
    {
        asyncActorCallbackChain.front()->unregister();
    }
    while (!asyncActorPerformanceNeutralCallbackChain.empty())
    {
        asyncActorPerformanceNeutralCallbackChain.front()->unregister();
    }
    for (Actor::EventTable *i = freeEventTable; i != 0;)
    {
        Actor::EventTable *tmp = i;
        i = i->nextUnused;
        tmp->~EventTable();
        nodeAllocator.deallocate(sizeof(Actor::EventTable) + CACHE_LINE_SIZE - 1, tmp->deallocatePointer);
    }
    while (!freeNodeConnectionChain.empty())
    {
        nodeAllocator.deallocate(sizeof(Actor::NodeConnection), freeNodeConnectionChain.pop_front());
    }
}

    AsyncNodeBase::StaticShared::AbsoluteEventIds::~AbsoluteEventIds() noexcept
{
    assert(eventNameIdMap.empty());
    assert(eventIdNameMap.empty());
}

void AsyncNodeBase::StaticShared::AbsoluteEventIds::addEventId(Actor::EventId eventId, const char *absoluteEventId)
{
    if (eventNameIdMap.find(absoluteEventId) != eventNameIdMap.end())
    {
        throw Actor::Event::DuplicateAbsoluteEventIdException();            // absolute event id (string) must be unique!
    }
    std::pair<EventIdNameMap::iterator, bool> retInsert = eventIdNameMap.insert(EventIdNameMap::value_type(eventId, absoluteEventId));
    assert(retInsert.second == true);
    try
    {
#ifndef NDEBUG
        std::pair<EventNameIdMap::iterator, bool> debugRetInsert =
#endif
        eventNameIdMap.insert(EventNameIdMap::value_type(retInsert.first->second.c_str(), eventId));
        assert(debugRetInsert.second);
    }
    catch (...)
    {
        eventIdNameMap.erase(retInsert.first);
        TRZ_DEBUG(debugCheckMaps();)
        throw;
    }
    TRZ_DEBUG(debugCheckMaps();)
}

void AsyncNodeBase::StaticShared::AbsoluteEventIds::removeEventId(Actor::EventId eventId) noexcept
{
    EventIdNameMap::iterator ieventIdNameMap = eventIdNameMap.find(eventId);
    if (ieventIdNameMap != eventIdNameMap.end())
    {
#ifndef NDEBUG
        EventNameIdMap::size_type debugRetErase =
#endif
            eventNameIdMap.erase(ieventIdNameMap->second.c_str());
        assert(debugRetErase == 1);
        eventIdNameMap.erase(ieventIdNameMap);
    }
    else
    {
        TRZ_DEBUG_BREAK();
    }
    TRZ_DEBUG(debugCheckMaps();)
}

std::pair<bool, Actor::EventId>
AsyncNodeBase::StaticShared::AbsoluteEventIds::findEventId(const char *absoluteEventId) const noexcept
{
    TRZ_DEBUG(debugCheckMaps();)
    EventNameIdMap::const_iterator ieventNameIdMap = eventNameIdMap.find(absoluteEventId);
    if (ieventNameIdMap == eventNameIdMap.end())
    {
        return std::make_pair(false, Actor::MAX_EVENT_ID_COUNT);
    }
    else
    {
        return std::make_pair(true, ieventNameIdMap->second);
    }
}

#ifndef NDEBUG
void AsyncNodeBase::StaticShared::AbsoluteEventIds::debugCheckMaps() const
{
    assert(eventIdNameMap.size() == eventNameIdMap.size());
    for (EventIdNameMap::const_iterator i = eventIdNameMap.begin(), endi = eventIdNameMap.end(); i != endi; ++i)
    {
        EventNameIdMap::const_iterator ieventNameIdMap = eventNameIdMap.find(i->second.c_str());
        assert(ieventNameIdMap != eventNameIdMap.end());
        assert(i->first == ieventNameIdMap->second);
        assert(i->second.c_str() == ieventNameIdMap->first);
    }
}
#endif

//---- AsyncNode CTOR ----------------------------------------------------------

/**
 * throw (std::bad_alloc, UndefinedCoreException, CoreInUseException)
 */
    AsyncNode::AsyncNode(const Init &init)

    try : AsyncNodeBase(init.nodeManager),
          AsyncNodeManager::Node(init.nodeManager, init.nodeManager.getCoreSet().index(init.coreId)),
        eventLoop(init.customEventLoopFactory.newEventLoop()),
        corePerformanceCounters(Actor::AllocatorBase(*this), getCoreSet().size()),
        
#ifndef NDEBUG
        debugSynchronizePostBarrierFlag(false),
#endif
        m_RefMapperPtr(IRefMapper::Create(*this)), m_RefMapper(*m_RefMapperPtr)
    {
        assert(nodeHandle.node == 0);
        nodeHandle.node = this;
#ifndef NDEBUG
        assert(nodeHandle.debugNodePtrWasSet == false);
        nodeHandle.debugNodePtrWasSet = true;
#endif
        nodeHandle.stopFlag = false;
        eventLoop->init(*this);
        for (Actor::NodeId nodeId = 0; nodeId < getCoreSet().size(); ++nodeId)
        {
            assert(corePerformanceCounters.writtenSizePointerVector.size() == nodeId);
            corePerformanceCounters.writtenSizePointerVector.push_back(
                &getReferenceToWriterShared(nodeId).writeCache.totalWrittenByteSize);
        }
    }
catch (AsyncNodeManager::NodeInUseException &)
{
    throw CoreInUseException();
}

//---- AsyncNode DTOR ----------------------------------------------------------

    AsyncNode::~AsyncNode() noexcept
{
    assert(!debugSynchronizePostBarrierFlag);
    nodeHandle.getWriterSharedHandle(id).cl2.shared.writeCache.freeEventAllocatorPageChain.push_back(
        usedlocalEventAllocatorPageChain);
        
    #ifdef TRACE_REF
        std::ofstream refLogFile;
        
        std::ostringstream stm;
        stm << "reflogfile." << pthread_self() << ".log";
        refLogFile.open(stm.str().c_str(),std::fstream::out | std::fstream::trunc);
        refLogFile << dbgRefLogStr;
        refLogFile.close();
    #endif
}

/**
 * throw (std::bad_alloc, Actor::ShutdownException)
 */
Actor::EventTable &AsyncNode::retainEventTable(Actor &asyncActor)
{
    if (nodeHandle.shutdownFlag || nodeHandle.stopFlag)
    {
        throw Actor::ShutdownException(nodeHandle.shutdownFlag);
    }
    if (nodeHandle.nextHanlerId == 0)
    {
        throw std::bad_alloc();
    }
    Actor::EventTable *ret;
    if (freeEventTable == 0)
    {
        char *deallocatePointer =
            static_cast<char *>(nodeAllocator.allocate(sizeof(Actor::EventTable) + CACHE_LINE_SIZE - 1));
        ret = new (CacheLineAlignedBuffer::cacheLineAlignedPointer(deallocatePointer))
            Actor::EventTable(deallocatePointer);
    }
    else
    {
        ret = freeEventTable;
        freeEventTable = ret->nextUnused;
    }
    assert(ret->asyncActor == 0);
    assert(ret->lfEvent == 0);
    assert(ret->undeliveredEvent == 0);
    ret->nodeActorId = nodeHandle.nextHanlerId;
    ret->asyncActor = &asyncActor;
    ++nodeHandle.nextHanlerId;
    for (int i = 0; i < Actor::EventTable::HIGH_FREQUENCY_CALLBACK_ARRAY_SIZE; ++i)
    {
        ret->hfEvent[i].eventId = Actor::MAX_EVENT_ID_COUNT;
    }
    return *ret;
}

void AsyncNode::releaseEventTable(Actor::EventTable &eventTable) noexcept
{
    eventTable.nodeActorId = 0;
    eventTable.asyncActor = 0;
    if (eventTable.lfEvent != 0)
    {
        nodeAllocator.deallocate(Actor::EventTable::lfRegisteredEventArraySize(eventTable.lfEvent) *
                                     sizeof(Actor::EventTable::RegisteredEvent),
                                 eventTable.lfEvent);
        eventTable.lfEvent = 0;
    }
    if (eventTable.undeliveredEvent != 0)
    {
        nodeAllocator.deallocate(Actor::EventTable::lfRegisteredEventArraySize(eventTable.undeliveredEvent) *
                                     sizeof(Actor::EventTable::RegisteredEvent),
                                 eventTable.undeliveredEvent);
        eventTable.undeliveredEvent = 0;
    }
    eventTable.undeliveredEventCount = 0;
    eventTable.nextUnused = freeEventTable;
    freeEventTable = &eventTable;
}

void AsyncNode::destroyAsyncActors() noexcept
{
    for (AsyncActorChain::iterator i = asyncActorChain.begin(), endi = asyncActorChain.end(); i != endi;)
    {
        Actor &actor = *i;
        ++i;
        actor.requestDestroy();
    }
}

/**
 * throw (Exception)
 */
AsyncNode::Thread::Thread(CoreId pcoreId, bool predZoneFlag, const ThreadRealTimeParam &predZoneParam,
                          size_t stackSizeBytes, StartHook pstartHook, void *pstartHookArg, StopHook pstopHook)
    : coreId(pcoreId), redZoneFlag(predZoneFlag), redZoneParam(predZoneParam),
      inThreadFlag(false), runFlag(false),
      inThreadExceptionFlag(false), startHook(pstartHook), startHookArg(pstartHookArg), stopHook(pstopHook),
      inThreadNode(0)
{
    threadCreate(inThread, this, stackSizeBytes);
    for (; !inThreadFlag && !inThreadExceptionFlag; threadSleep())
    {
    }
    if (inThreadExceptionFlag)
    {
        assert(inThreadFlag == false);
        assert(inThreadNode == 0);
        throw Exception(inThreadExceptionWhat);
    }
    assert(inThreadFlag);
}

AsyncNode::Thread::~Thread() noexcept
{
    assert(inThreadExceptionFlag == false);
    assert(inThreadNode != 0);
    for (runFlag = true; inThreadFlag; threadSleep())
    {
    }
}

void AsyncNode::Thread::run(CacheLineAlignedObject<AsyncNode> &node) noexcept
{
    assert(inThreadExceptionFlag == false);
    assert(inThreadFlag == true);
    assert(runFlag == false);
    assert(inThreadNode != 0);
    *inThreadNode = &node;
    
    memoryBarrier();
    
    runFlag = true;
    
    memoryBarrier();
}

void AsyncNode::Thread::inThread(void *pthread)
{
    try
    {
        std::pair<StartHook, void *> startHook(0, 0);
        StopHook stopHook = 0;
        CacheLineAlignedObject<AsyncNode> *node = 0;
        const bool redZoneFlag = static_cast<Thread *>(pthread)->redZoneFlag;
        const ThreadRealTimeParam redZoneParam = static_cast<Thread *>(pthread)->redZoneParam;
        {
            Thread &thread = *static_cast<Thread *>(pthread);
            startHook = thread.getStartHook();
            stopHook = thread.getStopHook();
            try
            {
                threadSetAffinity(thread.coreId);
                threadSetRealTime(redZoneFlag,
                                  redZoneParam); // attempt - in case of exception, will gracefully terminate the thread
            }
            catch (std::exception &e)
            {
                try
                {
                    thread.inThreadExceptionWhat = e.what();
                }
                catch (...)
                {
                }
                memoryBarrier();
                
                thread.inThreadExceptionFlag = true;
                
                memoryBarrier();
                
                return;
            }
            catch (...)
            {
                memoryBarrier();
                
                thread.inThreadExceptionFlag = true;
                
                memoryBarrier();
                
                return;
            }
            thread.inThreadNode = &node;
            
            memoryBarrier();
            
            thread.inThreadFlag = true;
            
            memoryBarrier();
            
            if (redZoneFlag)
            {
                threadSetRealTime(false, ThreadRealTimeParam()); // first switch back to normal priority to enable
                                                                 // synchronization with triggering thread (e.g. main) -
                                                                 // must not fail
            }
            for (; !thread.runFlag; threadSleep())
            {
            }
            
            memoryBarrier();
            
            thread.inThreadFlag = false;
            
            memoryBarrier();
            
            if (node == 0)
            {
                return;
            }
        }

        if (startHook.first != 0)
        {
            startHook.first(startHook.second);
        }

        AsyncNode &asyncNode = **node;
#ifndef NDEBUG
        asyncNode.nodeAllocator.debugThreadId = ThreadId::current();
#endif
        if (redZoneFlag)
        {
            threadSetRealTime(true, redZoneParam); // switch back to red-zone priority - must not fail
        }

        asyncNode.eventLoop->preRun();
        asyncNode.eventLoop->run();
        asyncNode.eventLoop->postRun();
        delete node;

        if (stopHook != 0)
        {
            stopHook();
        }
    }
    catch (std::exception &e)
    {
        try
        {
            std::cout << "Unexpected in node thread exception (" << e.what() << ')' << std::endl;
        }
        catch (...)
        {
        }
        exit(-2);
    }
    catch (...)
    {
        try
        {
            std::cout << "Unexpected in node thread exception" << std::endl;
        }
        catch (...)
        {
        }
        exit(-2);
    }
}

AsyncNode::Thread::Exception::Exception(const Exception &other) noexcept : std::exception(other)
{
    if (other.message.get() != 0)
    {
        try
        {
            message.reset(new std::string(*other.message));
        }
        catch (...)
        {
        }
    }
}

AsyncNode::Thread::Exception::Exception(const std::string &pmessage) noexcept
{
    try
    {
        message.reset(new std::string(pmessage));
    }
    catch (...)
    {
    }
}

AsyncNode::Thread::Exception::~Exception() noexcept {}

const char *AsyncNode::Thread::Exception::what() const noexcept
{
    if (message.get() != 0 && !message->empty())
    {
        try
        {
            return message->c_str();
        }
        catch (...)
        {
        }
    }
    return "<unknown exception>";
}

void AsyncNode::onNodeActorCountChange_LL(const int delta, const Actor *actor) noexcept
{
    #ifdef DTOR_DEBUG
        cout << "AsyncNode::onNodeActorCountChange_LL(delta = " << delta << ")" << endl;
    #endif
    
    assert((int64_t)m_ActorCount + delta >= 0);
    
    assert((std::abs(delta) == 1));
    assert(actor);
    
    const size_t oldCount = m_ActorCount;
    
    // update Sleem's counter
    m_ActorCount = (size_t)((int64_t)m_ActorCount + delta);
    
    // update my counter
    if (delta == 1)
    {
        m_RefMapper.onActorAdded(actor);
    }
    else if (delta == -1)
    {
        m_RefMapper.onActorRemoved(actor);
    }
    
    const size_t newCount = m_ActorCount;
    
    if (newCount == 0)      stop();         // WTF? [PL]
    
    // is swapping chains?
    NodeActorCountListener::Chain tmp;
    
    tmp.swap(m_NodeActorCountListenerChain);
    
    for (NodeActorCountListener::Chain::iterator it = tmp.begin(), endi = tmp.end(); it != endi; ++it)
    {
        assert(it->chain == &m_NodeActorCountListenerChain);
        it->chain = &tmp;
    }
    
    // tries to do a splice?
    while (!tmp.empty())
    {
        NodeActorCountListener  &listener = *tmp.pop_front();
        
        (listener.chain = &m_NodeActorCountListenerChain)->push_back(&listener);
        
        listener.onNodeActorCountDiff(oldCount, newCount);
    }
}

//---- On Actor Added ----------------------------------------------------------

void    AsyncNode::onActorAdded(Actor *actor)
{
    onNodeActorCountChange_LL(1, actor);
}

//---- On Node Removed ---------------------------------------------------------

void    AsyncNode::onActorRemoved(Actor *actor)
{
    onNodeActorCountChange_LL(-1, actor);    
}
    
} // namespace tredzone
