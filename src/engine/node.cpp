/**
 * @file node.cpp
 * @brief Simplx node class
 * @copyright 2013-2019 Tredzone (www.tredzone.com). All rights reserved.
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
            ENTERPRISE_0X501C(static_cast<Actor*>(static_cast<const Actor::EventTable*>(event.getDestinationActorId().eventTable)->asyncActor)->getAsyncNode(), &event, static_cast<void*>(undeliveredEvent[i].eventHandler));
            (*undeliveredEvent[i].staticEventHandler)(undeliveredEvent[i].eventHandler, event);
            ENTERPRISE_0X501D(static_cast<Actor*>(static_cast<const Actor::EventTable*>(event.getDestinationActorId().eventTable)->asyncActor)->getAsyncNode());
        }
    }
}

// returns [dispatched ok]
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
                asyncExceptionHandler.onUnreachableExceptionSynchrononous(
                    *asyncActor, typeid(*asyncActor),
                    Actor::ActorId::RouteIdComparable(writerNodeId, i->nodeConnectionId), e.what());
            }
            catch (...)
            {
                asyncExceptionHandler.onUnreachableExceptionSynchrononous(
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
            cl1.writerNodeHandle->node->nodeManager.exceptionHandler.onEventExceptionSynchronous(
                eventTable.asyncActor, typeid(*eventTable.asyncActor), "onUndeliveredEvent", event, e.what());
        }
        catch (...)
        {
            assert(eventTable.asyncActor != 0);
            cl1.writerNodeHandle->node->nodeManager.exceptionHandler.onEventExceptionSynchronous(
                eventTable.asyncActor, typeid(*eventTable.asyncActor), "onUndeliveredEvent", event,
                "unknown exception");
        }
    }
}

AsyncNodeManager::AsyncNodeManager(size_t eventAllocatorPageSize, const CoreSet &pcoreSet)
    : std::unique_ptr<AsyncExceptionHandler>(new AsyncExceptionHandler),
      Parallel<AsyncNodesHandle>(std::make_pair(eventAllocatorPageSize, &pcoreSet)), exceptionHandler(**this),
      coreSet(pcoreSet)
{
}

AsyncNodeManager::AsyncNodeManager(AsyncExceptionHandler &pexceptionHandler, size_t eventAllocatorPageSize, const CoreSet &pcoreSet)
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
    : singletonActorIndex(StaticShared::SINGLETON_ACTOR_INDEX_SIZE, SingletonActorIndexEntry(), Actor::AllocatorBase(nodeAllocator)),
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
        debugCheckMaps();
        throw;
    }
    
    debugCheckMaps();
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
    
    debugCheckMaps();
}

// why is protected by mutex? [PL]
//  - probably because is global, used by all nodes

pair<bool, Actor::EventId>
AsyncNodeBase::StaticShared::AbsoluteEventIds::findEventId(const char *absoluteEventId) const noexcept
{
    debugCheckMaps();
    
    EventNameIdMap::const_iterator ieventNameIdMap = eventNameIdMap.find(absoluteEventId);
    if (ieventNameIdMap == eventNameIdMap.end())
    {   // not found... happens WHEN? [PL]
        return std::make_pair(false, Actor::MAX_EVENT_ID_COUNT);
    }
    else
    {
        return std::make_pair(true, ieventNameIdMap->second);
    }
}

//---- Dump Absolute Event ID map ----------------------------------------------

AbsoluteEventVector    AsyncNodeBase::StaticShared::AbsoluteEventIds::getAbsoluteEventDictionary(void) const
{
    AbsoluteEventVector res;
    
    for (const auto &it : eventIdNameMap)
    {
            const uint16_t  id = it.first;
            const string    &s = it.second;
            
            res.emplace_back(id, s);
    }
    
    return res;
}

void AsyncNodeBase::StaticShared::AbsoluteEventIds::debugCheckMaps() const
{
    #ifndef NDEBUG
        assert(eventIdNameMap.size() == eventNameIdMap.size());
        
        for (EventIdNameMap::const_iterator i = eventIdNameMap.begin(), endi = eventIdNameMap.end(); i != endi; ++i)
        {
            EventNameIdMap::const_iterator ieventNameIdMap = eventNameIdMap.find(i->second.c_str());
            assert(ieventNameIdMap != eventNameIdMap.end());
            assert(i->first == ieventNameIdMap->second);
            assert(i->second.c_str() == ieventNameIdMap->first);
        }
    #endif
}

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

//only use this method from the same core as the eventTable->asyncActor
Actor* Actor::ActorId::getEventTableActor()
{
    return eventTable->asyncActor;
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

void AsyncNodesHandle::ReaderSharedHandle::read(void) noexcept
{
    assert(cl1.sharedReadWriteLocked);
    
    Shared::ReadWriteLocked &sharedReadWriteLocked = *cl1.sharedReadWriteLocked;
    assert(!sharedReadWriteLocked.deliveredEventsFlag);
    sharedReadWriteLocked.deliveredEventsFlag = true;
    assert(sharedReadWriteLocked.readerNodeHandle);
    assert(sharedReadWriteLocked.readerNodeHandle->node);
    AsyncNode &node = *sharedReadWriteLocked.readerNodeHandle->node;
    
    if (sharedReadWriteLocked.checkUndeliveredEventsFlag)
    {   // flag will be falsed by next peer write
        node.setWriteSignal(sharedReadWriteLocked.writerNodeId);
    }
    
    for (EventChain::iterator i = sharedReadWriteLocked.toBeDeliveredEventChain.begin(), endi = sharedReadWriteLocked.toBeDeliveredEventChain.end(); i != endi; node.loopUsagePerformanceCounterIncrement = 1)
    {
        assert(i->getSourceActorId() != i->getDestinationActorId());
        assert(i->getDestinationInProcessActorId().nodeId == node.id);
        
        Actor::Event                    &event = *i;
        const Actor::InProcessActorId   &eventDestinationInProcessActorId = event.getDestinationInProcessActorId();
        const Actor::EventTable         &eventTable = *eventDestinationInProcessActorId.eventTable;
        
        // is event destination in this node?
        if (eventDestinationInProcessActorId.getNodeActorId() == eventTable.nodeActorId)
        {
            try
            {
                if (eventTable.onEvent(event, node.corePerformanceCounters.onEventCount))
                {
                    // was dispatched ok
                    ++i;
                }
                else
                {   // couldn't dispatch anomaly
                    i = sharedReadWriteLocked.undeliveredEvent(i, sharedReadWriteLocked.toBeDeliveredEventChain);
                }
            }
            catch (Actor::ReturnToSenderException &)
            {
                i = sharedReadWriteLocked.undeliveredEvent(i, sharedReadWriteLocked.toBeDeliveredEventChain);
            }
            catch (std::exception &e)
            {
                ++i;
                assert(eventTable.asyncActor != 0);
                assert(sharedReadWriteLocked.readerNodeHandle != 0);
                assert(sharedReadWriteLocked.readerNodeHandle->node != 0);
                node.nodeManager.exceptionHandler.onEventExceptionSynchronous(eventTable.asyncActor, typeid(*eventTable.asyncActor), "onEvent", event, e.what());
            }
            catch (...)
            {
                ++i;
                assert(eventTable.asyncActor != 0);
                assert(sharedReadWriteLocked.readerNodeHandle != 0);
                assert(sharedReadWriteLocked.readerNodeHandle->node != 0);
                node.nodeManager.exceptionHandler.onEventExceptionSynchronous(eventTable.asyncActor, typeid(*eventTable.asyncActor), "onEvent", event, "unkwown exception");
            }
        }
        else
        {   // move from to-be-delivered into undelivered chain
            i = sharedReadWriteLocked.undeliveredEvent(i, sharedReadWriteLocked.toBeDeliveredEventChain);
        }
    }
    
    // routed events (NOT always e2e-related) [PL]
    for (EventChain::iterator i = sharedReadWriteLocked.toBeRoutedEventChain.begin(), endi = sharedReadWriteLocked.toBeRoutedEventChain.end(); i != endi; node.loopUsagePerformanceCounterIncrement = 1)
    {
        assert(i->getSourceActorId().isInProcess());
        assert(!i->getDestinationActorId().isInProcess());
        assert(!i->isRouteToSource());
        assert(i->isRouteToDestination());
        assert(!i->getRouteId().isInProcess());
        assert(i->getRouteId().getNodeId() == node.id);
        
        const Actor::ActorId::RouteId &routeId = i->getRouteId();
        assert(routeId.nodeConnection);
        const Actor::NodeConnection *nodeConnection = routeId.nodeConnection;
        
        if (routeId.getNodeConnectionId() == nodeConnection->nodeConnectionId)
        {
            Actor::Event &event = *i;
            try
            {
                nodeConnection->onOutboundEventFn(nodeConnection->connector, event);
                ++i;
            }
            catch (Actor::ReturnToSenderException &)
            {
                i = sharedReadWriteLocked.undeliveredEvent(i, sharedReadWriteLocked.toBeRoutedEventChain);
                // continues execution!
            }
            catch (std::exception &e)
            {
                ++i;
                assert(nodeConnection->connector);
                node.nodeManager.exceptionHandler.onEventExceptionSynchronous(0, typeid(void*/**nodeConnection->connector*/),            // error
                                                                               "onOutboundEvent", event, e.what());
            }
            catch (...)
            {
                ++i;
                assert(nodeConnection->connector);
                node.nodeManager.exceptionHandler.onEventExceptionSynchronous(
                    0, typeid(void*/**nodeConnection->connector*/), "onOutboundEvent", event, "unknown exception");                      // error
            }
        }
        else
        {
            i = sharedReadWriteLocked.undeliveredEvent(i, sharedReadWriteLocked.toBeRoutedEventChain);
        }
    }
    
    // routed, undelivered events (NOT always e2e-related) [PL]
    for (EventChain::iterator i = sharedReadWriteLocked.toBeUndeliveredRoutedEventChain.begin(), endi = sharedReadWriteLocked.toBeUndeliveredRoutedEventChain.end(); i != endi; ++i, node.loopUsagePerformanceCounterIncrement = 1)
    {
        assert(i->getSourceActorId().isInProcess());
        assert(!i->getDestinationActorId().isInProcess());
        assert(i->getSourceInProcessActorId().nodeId == node.id);
        Actor::Event &event = *i;
        const Actor::InProcessActorId &eventSourceInProcessActorId = event.getSourceInProcessActorId();
        const Actor::EventTable &eventTable = *eventSourceInProcessActorId.eventTable;
        
        // is source actor in this node?
        if (eventSourceInProcessActorId.getNodeActorId() == eventTable.nodeActorId)
        {
            // yes, send undelivered right away
            try
            {
                eventTable.onUndeliveredEvent(event);
            }
            catch (std::exception &e)
            {
                assert(eventTable.asyncActor != 0);
                assert(sharedReadWriteLocked.readerNodeHandle != 0);
                assert(sharedReadWriteLocked.readerNodeHandle->node != 0);
                node.nodeManager.exceptionHandler.onEventExceptionSynchronous(
                    eventTable.asyncActor, typeid(*eventTable.asyncActor), "onUndeliveredEvent", event, e.what());
            }
            catch (...)
            {
                assert(eventTable.asyncActor != 0);
                assert(sharedReadWriteLocked.readerNodeHandle != 0);
                assert(sharedReadWriteLocked.readerNodeHandle->node != 0);
                node.nodeManager.exceptionHandler.onEventExceptionSynchronous(
                    eventTable.asyncActor, typeid(*eventTable.asyncActor), "onUndeliveredEvent", event, "unknown exception");
            }
        }
    }
    
    // unreacheble nodes?
    Actor::OnUnreachableChain   *actorOnUnreachableChain;
    
    if (!sharedReadWriteLocked.unreachableNodeConnectionChain.empty() && !(actorOnUnreachableChain = &node.actorOnUnreachableChain)->empty())
    {
        node.loopUsagePerformanceCounterIncrement = 1;
        dispatchUnreachableNodes(*actorOnUnreachableChain, sharedReadWriteLocked.unreachableNodeConnectionChain,
                                 sharedReadWriteLocked.writerNodeId, node.nodeManager.exceptionHandler);
    }
}

bool AsyncNodesHandle::ReaderSharedHandle::returnToSender(const Actor::Event &event) noexcept
{
    assert(!event.isRouted());
    assert(cl1.sharedReadWriteLocked);
    Shared::ReadWriteLocked &sharedReadWriteLocked = *cl1.sharedReadWriteLocked;
    EventChain::iterator i = sharedReadWriteLocked.toBeDeliveredEventChain.begin(), endi = sharedReadWriteLocked.toBeDeliveredEventChain.end();
    
    for (; i != endi && &*i != &event; ++i)
    {
    }
    
    if (i != endi)
    {
        sharedReadWriteLocked.undeliveredEvent(i, sharedReadWriteLocked.toBeDeliveredEventChain);
        return true;
    }
    return false;
}

bool AsyncNodesHandle::WriterSharedHandle::write() noexcept
{
    cl2.shared.readWriteLocked.deliveredEventsFlag = false;
    bool isWriteWorthy = cl2.shared.readWriteLocked.checkUndeliveredEventsFlag =
        cl2.shared.writeCache.checkUndeliveredEventsFlag;
    cl2.shared.writeCache.checkUndeliveredEventsFlag = false;
    
    if (!cl2.shared.readWriteLocked.undeliveredEventChain.empty())
    {
        assert(cl1.writerNodeHandle);
        assert(cl1.writerNodeHandle->node);
        cl1.writerNodeHandle->node->loopUsagePerformanceCounterIncrement = 1;
        writeDispatchAndClearUndeliveredEvents(cl2.shared.readWriteLocked.undeliveredEventChain);
    }
    {
        EventChain emptyEventChain;
        cl2.shared.readWriteLocked.toBeDeliveredEventChain.swap(emptyEventChain);
    }
    {
        EventChain emptyEventChain;
        cl2.shared.readWriteLocked.toBeRoutedEventChain.swap(emptyEventChain);
    }
    {
        EventChain emptyEventChain;
        cl2.shared.readWriteLocked.toBeUndeliveredRoutedEventChain.swap(emptyEventChain);
    }
    {
        Shared::UnreachableNodeConnectionChain emptyUnreachableNodeConnectionChain;
        cl2.shared.readWriteLocked.unreachableNodeConnectionChain.swap(emptyUnreachableNodeConnectionChain);
    }
    cl2.shared.writeCache.freeEventAllocatorPageChain.push_back(cl2.shared.readWriteLocked.usedEventAllocatorPageChain);
    if (!cl2.shared.writeCache.toBeDeliveredEventChain.empty() || !cl2.shared.writeCache.toBeRoutedEventChain.empty() ||
        !cl2.shared.writeCache.toBeUndeliveredRoutedEventChain.empty() ||
        !cl2.shared.writeCache.unreachableNodeConnectionChain.empty())
    {
        isWriteWorthy = true;
        if (cl2.shared.writeCache.batchId != std::numeric_limits<uint64_t>::max())
        {
            cl2.shared.writeCache.batchId += cl2.shared.writeCache.batchIdIncrement;
            cl2.shared.writeCache.batchIdIncrement = 0;
        }
        cl2.shared.readWriteLocked.toBeDeliveredEventChain.swap(cl2.shared.writeCache.toBeDeliveredEventChain);
        cl2.shared.readWriteLocked.toBeRoutedEventChain.swap(cl2.shared.writeCache.toBeRoutedEventChain);
        cl2.shared.readWriteLocked.toBeUndeliveredRoutedEventChain.swap(
            cl2.shared.writeCache.toBeUndeliveredRoutedEventChain);
        cl2.shared.readWriteLocked.unreachableNodeConnectionChain.swap(
            cl2.shared.writeCache.unreachableNodeConnectionChain);
        cl2.shared.readWriteLocked.usedEventAllocatorPageChain.swap(cl2.shared.writeCache.usedEventAllocatorPageChain);
        assert(cl2.shared.writeCache.toBeDeliveredEventChain.empty());
        assert(cl2.shared.writeCache.toBeRoutedEventChain.empty());
        assert(cl2.shared.writeCache.toBeUndeliveredRoutedEventChain.empty());
        assert(cl2.shared.writeCache.unreachableNodeConnectionChain.empty());
        assert(cl2.shared.writeCache.usedEventAllocatorPageChain.empty());
        if (!cl2.shared.writeCache.freeEventAllocatorPageChain.empty())
        {
            cl2.shared.writeCache.usedEventAllocatorPageChain.push_front(
                cl2.shared.writeCache.freeEventAllocatorPageChain.pop_front());
        }
        cl2.shared.writeCache.frontUsedEventAllocatorPageChainOffset = 0;
    }
    return isWriteWorthy;
}

bool AsyncNodesHandle::WriterSharedHandle::localOnEvent(const Actor::Event &event,
                                                        uint64_t &performanceCounter) noexcept
{
    assert(cl1.writerNodeHandle != 0);
    assert(cl1.writerNodeHandle->node != 0);
    assert((!event.isRouted() && event.getSourceInProcessActorId().nodeId == cl1.writerNodeHandle->node->id) ||
           (event.isRouted() && event.getRouteId().getNodeId() == cl1.writerNodeHandle->node->id));
    assert(event.getDestinationInProcessActorId().nodeId == cl1.writerNodeHandle->node->id);
    const Actor::ActorId &actorId = event.getDestinationActorId();
    Actor::NodeActorId nodeActorId = actorId.getNodeActorId();
    const Actor::EventTable *eventTable = actorId.eventTable;
    if (nodeActorId == 0 || nodeActorId != eventTable->nodeActorId)
    {
        return false;
    }
    try
    {
        return eventTable->onEvent(event, performanceCounter);
    }
    catch (Actor::ReturnToSenderException &)
    {
        return false;
    }
    catch (std::exception &e)
    {
        assert(eventTable->asyncActor != 0);
        cl1.writerNodeHandle->node->nodeManager.exceptionHandler.onEventExceptionSynchronous(
            eventTable->asyncActor, typeid(*eventTable->asyncActor), "onEvent", event, e.what());
    }
    catch (...)
    {
        assert(eventTable->asyncActor != 0);
        cl1.writerNodeHandle->node->nodeManager.exceptionHandler.onEventExceptionSynchronous(
            eventTable->asyncActor, typeid(*eventTable->asyncActor), "onEvent", event, "unknown exception");
    }
    return true;
}

void AsyncNodesHandle::Shared::WriteCache::newEventPage()
{
    if (freeEventAllocatorPageChain.empty())
    {
        if (nextEventAllocatorPageIndex + 1 == 0)
        {
            throw std::bad_alloc();
        }
        usedEventAllocatorPageChain.push_front(
            new (eventAllocatorPageAllocator.insert(CACHE_LINE_SIZE + eventAllocatorPageSize))
                AsyncNodesHandle::Shared::EventAllocatorPage(nextEventAllocatorPageIndex));
        ++nextEventAllocatorPageIndex;
    }
    else
    {
        usedEventAllocatorPageChain.push_front(freeEventAllocatorPageChain.pop_front());
    }
    frontUsedEventAllocatorPageChainOffset = 0;
}

void *AsyncNodesHandle::Shared::WriteCache::allocateEvent(size_t sz)
{
    assert(frontUsedEventAllocatorPageChainOffset <= eventAllocatorPageSize);
    if (usedEventAllocatorPageChain.empty() || eventAllocatorPageSize - frontUsedEventAllocatorPageChainOffset < sz)
    {
        if (sz > eventAllocatorPageSize)
        {
            breakThrow(std::bad_alloc());
        }
        newEventPage();
    }
    assert(!usedEventAllocatorPageChain.empty());
    assert(eventAllocatorPageSize - frontUsedEventAllocatorPageChainOffset >= sz);
    void *ret = usedEventAllocatorPageChain.front()->at(frontUsedEventAllocatorPageChainOffset);
    frontUsedEventAllocatorPageChainOffset += sz;
    totalWrittenByteSize += sz;
    return ret;
}

void *AsyncNodesHandle::Shared::WriteCache::allocateEvent(size_t sz, uint32_t &eventPageIndex, size_t &eventPageOffset)
{
    assert(frontUsedEventAllocatorPageChainOffset <= eventAllocatorPageSize);
    if (usedEventAllocatorPageChain.empty() || eventAllocatorPageSize - frontUsedEventAllocatorPageChainOffset < sz)
    {
        if (sz > eventAllocatorPageSize)
        {
            breakThrow(std::bad_alloc());
        }
        newEventPage();
    }
    assert(!usedEventAllocatorPageChain.empty());
    assert(eventAllocatorPageSize - frontUsedEventAllocatorPageChainOffset >= sz);
    AsyncNodesHandle::Shared::EventAllocatorPage &eventPage = *usedEventAllocatorPageChain.front();
    void *ret = eventPage.at(frontUsedEventAllocatorPageChainOffset);
    eventPageIndex = eventPage.index;
    eventPageOffset = frontUsedEventAllocatorPageChainOffset;
    frontUsedEventAllocatorPageChainOffset += sz;
    totalWrittenByteSize += sz;
    return ret;
}

//---- default implementations -------------------------------------------------

void AsyncExceptionHandler::onEventException(Actor *, const std::type_info &asyncActorTypeInfo,
                                             const char *onXXX_FunctionName, const Actor::Event &event,
                                             const char *whatException) noexcept
{
    // what's the point of nested exception handlers? [PL]
    try
    {
        stringstream oss;
        oss << cppDemangledTypeInfoName(asyncActorTypeInfo) << "::" << onXXX_FunctionName << '(' << event << ") threw (" << whatException << ')' << endl;
        cout << oss.str();
    }
    catch (...)
    {
        try
        {
            cout << asyncActorTypeInfo.name() << "::" << onXXX_FunctionName << '(' << event.classId << ") threw (" << whatException << ")" << endl;
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
    // what's the point of nested exception handlers? [PL]
    try
    {
        stringstream oss;
        
        oss << cppDemangledTypeInfoName(asyncActorTypeInfo) << "::onUnreachable("
          << (unsigned)routeIdComparable.getNodeId() << '-' << routeIdComparable.getNodeConnectionId() << ") threw ("
          << whatException << ")" << endl;
          
        cout << oss.str();
    }
    catch (...)
    {
        try
        {
            cout << asyncActorTypeInfo.name() << "::onUnreachable(" << (unsigned)routeIdComparable.getNodeId()
                      << '-' << routeIdComparable.getNodeConnectionId() << ") threw (" << whatException << ')'
                      << endl;
        }
        catch (...)
        {
        }
    }
#ifndef NDEBUG
    std::exit(-1);
#endif
}

} // namespace tredzone
