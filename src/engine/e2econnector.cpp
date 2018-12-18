/**
 * @file e2econnector.cpp
 * @brief Simplx Engine-To-Engine (cluster) connector
 * @copyright 2013-2018 Tredzone (www.tredzone.com). All rights reserved.
 * Please see accompanying LICENSE file for licensing terms.
 */

#include "trz/engine/internal/e2econnector.h"
#include "trz/engine/internal/node.h"

namespace tredzone
{

class EngineToEngineConnector::Singleton : public Actor, public Actor::Callback
{
  public:
    inline void unregisterConnection(Actor::NodeConnection &nodeConnection) noexcept
    {
        nodeConnection.connector = 0;
        nodeConnection.onOutboundEventFn = 0;
        nodeConnection.onInboundUndeliveredEventFn = 0;
        nodeConnection.unreachableNodeConnectionId = nodeConnection.nodeConnectionId;
        nodeConnection.nodeConnectionId = 0;
        asyncNode->inUseNodeConnectionChain.remove(&nodeConnection);
        if (unreachableChain.empty())
        {
            try
            {
                assert(unreachableNotifiedNodeBitSet.size() >= asyncNode->getNodeCount());
                for (size_t i = asyncNode->getNodeCount(); i > 0; --i)
                {
                    NodeId nodeId = (NodeId)(i - 1);
                    AsyncNodesHandle::Shared::WriteCache &writeCache =
                        asyncNode->getReferenceToWriterShared(nodeId).writeCache;
                    writeCache.unreachableNodeConnectionChain.push_back(
                        new (writeCache.allocateEvent(sizeof(AsyncNodesHandle::Shared::UnreachableNodeConnection)))
                            AsyncNodesHandle::Shared::UnreachableNodeConnection(
                                nodeConnection.unreachableNodeConnectionId));
                    asyncNode->setWriteSignal(nodeId);
                    unreachableNotifiedNodeBitSet.set(nodeId);
                }
                asyncNode->freeNodeConnectionChain.push_front(&nodeConnection);
                unreachableNotifiedNodeBitSet.reset();
            }
            catch (const std::bad_alloc &)
            {
                unreachableChain.push_front(&nodeConnection);
                registerCallback(*this);
            }
        }
        else
        {
            unreachableChain.push_back(&nodeConnection);
        }
    }
    inline void onCallback() noexcept
    {
        assert(!unreachableChain.empty());
        while (!unreachableChain.empty())
        {
            ActorId::RouteId::NodeConnectionId unreachableNodeConnectionId =
                unreachableChain.front()->unreachableNodeConnectionId;
            bool failedFlag = false;
            for (size_t i = asyncNode->getNodeCount(); i > 0; --i)
            {
                NodeId nodeId = (NodeId)(i - 1);
                UnreachableNotifiedNodeBitSet::reference unreachableNotifiedNodeBit =
                    unreachableNotifiedNodeBitSet[nodeId];
                if (unreachableNotifiedNodeBit == false)
                {
                    try
                    {
                        AsyncNodesHandle::Shared::WriteCache &writeCache =
                            asyncNode->getReferenceToWriterShared(nodeId).writeCache;
                        writeCache.unreachableNodeConnectionChain.push_back(
                            new (writeCache.allocateEvent(sizeof(AsyncNodesHandle::Shared::UnreachableNodeConnection)))
                                AsyncNodesHandle::Shared::UnreachableNodeConnection(unreachableNodeConnectionId));
                        asyncNode->setWriteSignal(nodeId);
                        unreachableNotifiedNodeBit = true;
                    }
                    catch (const std::bad_alloc &)
                    {
                        failedFlag = true;
                    }
                }
            }
            if (failedFlag)
            {
                registerCallback(*this);
                return;
            }
            asyncNode->freeNodeConnectionChain.push_front(unreachableChain.pop_front());
            unreachableNotifiedNodeBitSet.reset();
        }
    }

  private:
    AsyncNode::NodeConnectionChain unreachableChain;
    typedef std::bitset<Actor::MAX_NODE_COUNT> UnreachableNotifiedNodeBitSet;
    UnreachableNotifiedNodeBitSet unreachableNotifiedNodeBitSet;

    virtual void onDestroyRequest() noexcept
    {
        if (unreachableChain.empty())
        {
            acceptDestroy();
        }
        else
        {
            requestDestroy();
        }
    }
};

EngineToEngineConnector::EngineToEngineConnector(Actor &actor)
    : asyncNode(*actor.asyncNode), nodeId(asyncNode.id), nodeConnectionId(0), nodeConnection(0),
      peerEngineName(Actor::AllocatorBase(asyncNode)), peerEngineSuffix(Actor::AllocatorBase(asyncNode)),
      singleton(actor.newReferencedSingletonActor<Singleton>()), innerActor(actor.newReferencedActor<InnerActor>(this)),
      allocator(actor.getAllocator())
{
    if (Engine::getEngine().getServiceIndex().getServiceActorId<service::E2ERoute>() == tredzone::null)
    {
        throw E2ERouteServiceMissingException();
    }
}

EngineToEngineConnector::~EngineToEngineConnector() noexcept
{
    innerActor->connector = 0;
    unregisterConnection();
    assert(nodeConnection == 0);
}

void EngineToEngineConnector::onConnectionServiceFailure() {}

EngineToEngineConnector::ServiceEntryVector EngineToEngineConnector::getServiceVector() const
{
    const Engine::ServiceIndex &serviceIndex = Engine::getEngine().getServiceIndex();
    size_t n = 0;
    for (int i = 0; i < Engine::ServiceIndex::MAX_SIZE; ++i)
    {
        if (serviceIndex.table[i].actorId != tredzone::null && !serviceIndex.table[i].name.empty())
        {
            ++n;
        }
    }
    ServiceEntryVector ret(innerActor->getAllocator());
    ret.reserve(n);
    for (int i = 0; i < Engine::ServiceIndex::MAX_SIZE; ++i)
    {
        if (serviceIndex.table[i].actorId != tredzone::null && !serviceIndex.table[i].name.empty())
        {
            ret.push_back(ServiceEntry(serviceIndex.table[i].name.c_str(), serviceIndex.table[i].actorId,
                                       innerActor->getAllocator()));
        }
    }
    return ret;
}

void EngineToEngineConnector::registerConnection(const char *ppeerEngineName, const char *ppeerEngineSuffix,
                                                      const ServiceEntryVector &serviceEntryVector,
                                                      OnOutboundEventFn onOutboundEventFn,
                                                      OnInboundUndeliveredEventFn onInboundUndeliveredEventFn,
                                                      bool isEngineToEngineSharedMemoryConnectorFlag,
                                                      e2econnector::connection_distance_type serialConnectionDistance)
{
    unregisterConnection();
    if (asyncNode.lastNodeConnectionId + 1 == 0)
    {
        throw std::bad_alloc();
    }
    peerEngineName = ppeerEngineName;
    peerEngineSuffix = ppeerEngineSuffix;
    if (asyncNode.freeNodeConnectionChain.empty())
    {
        asyncNode.freeNodeConnectionChain.push_back(
            new (asyncNode.nodeAllocator.allocate(sizeof(Actor::NodeConnection))) Actor::NodeConnection);
    }
    innerActor->registerConnectionService(Actor::ActorId::RouteId(nodeId, asyncNode.lastNodeConnectionId + 1,
                                                                       asyncNode.freeNodeConnectionChain.front()),
                                          ppeerEngineName, ppeerEngineSuffix,
                                          isEngineToEngineSharedMemoryConnectorFlag, serialConnectionDistance,
                                          serviceEntryVector);
    nodeConnection = asyncNode.freeNodeConnectionChain.pop_front();
    asyncNode.inUseNodeConnectionChain.push_back(nodeConnection);
    nodeConnection->connector = this;
    nodeConnection->nodeConnectionId = nodeConnectionId = ++asyncNode.lastNodeConnectionId;
    nodeConnection->onOutboundEventFn = onOutboundEventFn;
    nodeConnection->onInboundUndeliveredEventFn = onInboundUndeliveredEventFn;
    nodeConnection->isEngineToEngineSharedMemoryConnectorFlag = isEngineToEngineSharedMemoryConnectorFlag;
}

void EngineToEngineConnector::unregisterConnection() noexcept
{
    if (nodeConnection != 0)
    {
        assert(nodeConnection->connector == this);
        assert(nodeConnection->onOutboundEventFn != 0);
        assert(nodeConnection->onInboundUndeliveredEventFn != 0);
        assert(nodeConnection->nodeConnectionId != 0);
        assert(nodeConnection->nodeConnectionId == nodeConnectionId);
        singleton->unregisterConnection(*nodeConnection);
        nodeConnectionId = 0;
        nodeConnection = 0;
        innerActor->unregisterConnectionService();
    }
}

EngineToEngineConnector::InnerActor::InnerActor(EngineToEngineConnector *connector) : connector(connector)
{
    registerUndeliveredEventHandler<e2econnector::ConnectorRegisterEvent>(*this);
}

void EngineToEngineConnector::InnerActor::registerConnectionService(
    const Actor::ActorId::RouteId &pconnectionRouteId, const char *pengineName, const char *pengineSuffix,
    bool isSharedMemory, e2econnector::connection_distance_type serialConnectionDistance,
    const ServiceEntryVector &serviceEntryVector)
{
    if (connectionRouteId != tredzone::null)
    {
        throw std::bad_alloc();
    }
    const Engine::ServiceIndex &serviceIndex = Engine::getEngine().getServiceIndex();
    const ActorId &connectionServiceActorId = serviceIndex.getServiceActorId<service::E2ERoute>();
    assert(connectionServiceActorId != tredzone::null); // tested at EngineToEngineConnector ctor
    if (connectionServiceActorId != tredzone::null)
    {
        Event::Pipe pipe(*this, connectionServiceActorId);
        Event::AllocatorBase eventAllocator = pipe.getAllocator();
        const char *engineName = Event::newCString(eventAllocator, pengineName);
        const char *engineSuffix = Event::newCString(eventAllocator, pengineSuffix);
        e2econnector::ServiceEntry *firstServiceEntry = 0;
        e2econnector::ServiceEntry *lastServiceEntry = 0;
        for (ServiceEntryVector::const_iterator i = serviceEntryVector.begin(), endi = serviceEntryVector.end();
             i != endi; ++i)
        {
            e2econnector::ServiceEntry *serviceEntry = pipe.allocate<e2econnector::ServiceEntry>(1);
            serviceEntry->nextServiceEntry = 0;
            serviceEntry->serviceName = Event::newCString(eventAllocator, i->serviceName);
            serviceEntry->serviceInProcessActorId = i->serviceInProcessActorId;
            if (lastServiceEntry == 0)
            {
                assert(firstServiceEntry == 0);
                firstServiceEntry = lastServiceEntry = serviceEntry;
            }
            else
            {
                assert(lastServiceEntry->nextServiceEntry == 0);
                lastServiceEntry->nextServiceEntry = serviceEntry;
                lastServiceEntry = serviceEntry;
            }
            assert(lastServiceEntry->nextServiceEntry == 0);
        }
        e2econnector::ConnectorRegisterEvent &event = pipe.push<e2econnector::ConnectorRegisterEvent>();
        // below: no exception can be thrown
        event.routeId = pconnectionRouteId;
        event.isSharedMemory = isSharedMemory;
        event.serialConnectionDistance = serialConnectionDistance;
        event.engineName = engineName;
        event.engineSuffix = engineSuffix;
        event.firstServiceEntry = firstServiceEntry;
    }
    connectionRouteId = pconnectionRouteId;
}

void EngineToEngineConnector::InnerActor::unregisterConnectionService() noexcept
{
    assert(connectionRouteId != tredzone::null);
    assert(!Actor::Callback::isRegistered());
    const ActorId &connectionServiceActorId =
        Engine::getEngine().getServiceIndex().getServiceActorId<service::E2ERoute>();
    if (connectionServiceActorId != tredzone::null && connectionRouteId != tredzone::null &&
        !Actor::Callback::isRegistered())
    {
        try
        {
            Event::Pipe(*this, connectionServiceActorId).push<e2econnector::ConnectorUnregisterEvent>().routeId =
                connectionRouteId;
            connectionRouteId = tredzone::null;
        }
        catch (std::bad_alloc &)
        {
            registerCallback(*this);
        }
    }
    else
    {
        connectionRouteId = tredzone::null;
    }
}

void EngineToEngineConnector::InnerActor::onUndeliveredEvent(const e2econnector::ConnectorRegisterEvent &event)
{
    if (connector != 0 && event.routeId == connector->getRouteId())
    {
        connector->onConnectionServiceFailure();
    }
}

void EngineToEngineConnector::InnerActor::onCallback() noexcept { unregisterConnectionService(); }

EngineToEngineConnector::ServiceActor::ServiceActor() : eventHandler(*this)
{
    registerEventHandler<e2econnector::ConnectorRegisterEvent>(eventHandler);
    registerEventHandler<e2econnector::ConnectorUnregisterEvent>(eventHandler);
    registerEventHandler<e2econnector::SingletonClientSubscriptionEvent>(eventHandler);
    registerUndeliveredEventHandler<e2econnector::SingletonClientEngineEvent>(eventHandler);
}

EngineToEngineConnector::ServiceActor::~ServiceActor() noexcept {}

EngineToEngineConnector::ServiceActor::EventHandler::EventHandler(ServiceActor &actor)
    : serviceActor(actor), lastEngineId(0), engineEntryList(actor.getAllocator()),
      subscriberNodeEntryVector(Engine::getEngine().getCoreSet().size(), SubscriberNodeEntry(actor),
                                actor.getAllocator())
{
}

void EngineToEngineConnector::ServiceActor::EventHandler::onEvent(
    const e2econnector::ConnectorRegisterEvent &event)
{
    EngineEntry *engineEntry;
    EngineEntryList::iterator ilist = findEngineEntry(event.engineName, event.engineSuffix);
    if (ilist == engineEntryList.end())
    {
        if (lastEngineId == std::numeric_limits<engine_id_type>::max())
        {
            throw ReturnToSenderException();
        }
        try
        {
            engineEntryList.push_back(
                EngineEntry(++lastEngineId, event.engineName, event.engineSuffix, serviceActor.getAllocator()));
        }
        catch (...)
        {
            throw ReturnToSenderException();
        }
        engineEntry = &engineEntryList.back();
        try
        {
            engineEntry->routeEntryVector.reserve(EngineEntry::ROUTE_ENTRY_VECTOR_RESERVE);
            engineEntry->serviceEntryVector.reserve(
                e2econnector::ServiceEntry::getServiceEntryCount(event.firstServiceEntry));
            for (e2econnector::ServiceEntry *serviceEntry = event.firstServiceEntry; serviceEntry != 0;
                 serviceEntry = serviceEntry->nextServiceEntry)
            {
                assert(engineEntry->serviceEntryVector.size() < engineEntry->serviceEntryVector.capacity());
                engineEntry->serviceEntryVector.push_back(ServiceEntry(
                    serviceEntry->serviceName, serviceEntry->serviceInProcessActorId, serviceActor.getAllocator()));
            }
        }
        catch (...)
        {
            engineEntryList.pop_back();
            throw ReturnToSenderException();
        }
    }
    else
    {
        engineEntry = &*ilist;
    }
#ifndef NDEBUG
    assert(engineEntry->serviceEntryVector.size() ==
           e2econnector::ServiceEntry::getServiceEntryCount(event.firstServiceEntry));
    {
        ServiceEntryVector::size_type i = 0;
        e2econnector::ServiceEntry *serviceEntry = event.firstServiceEntry;
        for (; serviceEntry != 0 && i < engineEntry->serviceEntryVector.size();
             ++i, serviceEntry = serviceEntry->nextServiceEntry)
        {
            assert(engineEntry->serviceEntryVector[i].serviceName == serviceEntry->serviceName);
            assert(engineEntry->serviceEntryVector[i].serviceInProcessActorId == serviceEntry->serviceInProcessActorId);
        }
        assert(serviceEntry == 0);
        assert(i == engineEntry->serviceEntryVector.size());
    }
#endif
    if (engineEntry->findRouteEntry(event.routeId) != engineEntry->routeEntryVector.end())
    {
        breakThrow(ReturnToSenderException());
    }
    if (engineEntry->routeEntryVector.size() == engineEntry->routeEntryVector.capacity())
    {
        try
        {
            engineEntry->routeEntryVector.reserve(engineEntry->routeEntryVector.capacity() +
                                                  EngineEntry::ROUTE_ENTRY_VECTOR_RESERVE);
        }
        catch (...)
        {
            throw ReturnToSenderException();
        }
    }
    engineEntry->routeEntryVector.push_back(
        RouteEntry(event.routeId, event.isSharedMemory, event.serialConnectionDistance));
    notifySubscribers(*engineEntry);
}

void EngineToEngineConnector::ServiceActor::EventHandler::onEvent(
    const e2econnector::ConnectorUnregisterEvent &event)
{
    for (EngineEntryList::iterator iengine = engineEntryList.begin(), endiengine = engineEntryList.end();
         iengine != endiengine; ++iengine)
    {
        EngineEntry::RouteEntryVector::iterator iroute = iengine->findRouteEntry(event.routeId);
        if (iroute != iengine->routeEntryVector.end())
        {
            iengine->routeEntryVector.erase(iroute);
            notifySubscribers(*iengine);
            return;
        }
    }
}

void EngineToEngineConnector::ServiceActor::EventHandler::onEvent(
    const e2econnector::SingletonClientSubscriptionEvent &event)
{
    assert(event.clientNodeId < MAX_NODE_COUNT);
    assert(event.clientNodeId < subscriberNodeEntryVector.size());
    if (event.unsubscribeFlag)
    {
        assert(event.clientNodeId == singletonClientNodeId(event.getSourceActorId()));
        if (event.clientNodeId == singletonClientNodeId(event.getSourceActorId()))
        {
            singletonClientUnsubscribe(event.clientNodeId);
        }
    }
    else
    {
        SubscriberNodeEntryVector::iterator isubscriber = subscriberNodeEntryVector.begin(),
                                            endisubscriber = subscriberNodeEntryVector.end();
        for (; isubscriber != endisubscriber && isubscriber->nodeId != event.clientNodeId &&
               isubscriber->nodeId != MAX_NODE_COUNT;
             ++isubscriber)
        {
        }
        assert(isubscriber != endisubscriber);
        // isubscriber->nodeId == event.clientNodeId is possible in case client-singleton was recreated, and the
        // previous one failed to unsubscribe
        if (isubscriber != endisubscriber && event.clientNodeId < subscriberNodeEntryVector.size())
        {
            isubscriber->nodeId = event.clientNodeId;
            isubscriber->pipe.setDestinationActorId(event.getSourceActorId());
            for (EngineEntryList::iterator iengine = engineEntryList.begin(), endiengine = engineEntryList.end();
                 iengine != endiengine; ++iengine)
            {
                if (iengine->subscribedNodeBitSet.none())
                {
                    subscribersNotificationEngineEntryChain.push_back(&*iengine);
                }
                iengine->subscribedNodeBitSet.set(event.clientNodeId);
            }
            serviceActor.registerCallback(*this);
        }
    }
}

void EngineToEngineConnector::ServiceActor::EventHandler::onUndeliveredEvent(
    const e2econnector::SingletonClientEngineEvent &event)
{
    NodeId clientNodeId = singletonClientNodeId(event.getDestinationActorId());
    if (event.undeliveredBadAllocCauseFlag)
    {
        EngineEntryList::iterator iengine = engineEntryList.begin(), endiengine = engineEntryList.end();
        for (; iengine != endiengine && iengine->engineId != event.engineId; ++iengine)
        {
        }
        assert(iengine != endiengine); // not supposed to happen as this is a return to sender
                                       // (undeliveredBadAllocCauseFlag was changed) and this was an engine removal
                                       // notification (shouldn't require the client to allocate new memory)
        if (iengine != endiengine && clientNodeId != MAX_NODE_COUNT)
        {
            if (iengine->subscribedNodeBitSet.none())
            {
                subscribersNotificationEngineEntryChain.push_back(&*iengine);
            }
            iengine->subscribedNodeBitSet.set(clientNodeId);
            serviceActor.registerCallback(*this);
        }
    }
    else if (clientNodeId != MAX_NODE_COUNT)
    {
        singletonClientUnsubscribe(clientNodeId);
    }
}

void EngineToEngineConnector::ServiceActor::EventHandler::onCallback() noexcept
{
    try
    {
        while (!subscribersNotificationEngineEntryChain.empty())
        {
            EngineEntry &engineEntry = *subscribersNotificationEngineEntryChain.front();
            assert(engineEntry.subscribedNodeBitSet.any());
            for (SubscriberNodeEntryVector::iterator isubscriber = subscriberNodeEntryVector.begin(),
                                                     endisubscriber = subscriberNodeEntryVector.end();
                 isubscriber != endisubscriber && isubscriber->nodeId < MAX_NODE_COUNT; ++isubscriber)
            {
                assert(isubscriber->nodeId < engineEntry.subscribedNodeBitSet.size());
                EngineEntry::SubscribedNodeBitSet::reference subscribedNodeBit =
                    engineEntry.subscribedNodeBitSet[isubscriber->nodeId];
                if (subscribedNodeBit == true)
                {
                    const char *engineName = 0;
                    const char *engineSuffix = 0;
                    e2econnector::RouteEntry *firstRouteEntry = 0;
                    e2econnector::ServiceEntry *firstServiceEntry = 0;
                    if (!engineEntry.routeEntryVector.empty())
                    {
                        Event::AllocatorBase eventAllocator = isubscriber->pipe.getAllocator();
                        engineName = Event::newCString(eventAllocator, engineEntry.engineName);
                        engineSuffix = Event::newCString(eventAllocator, engineEntry.engineSuffix);

                        e2econnector::RouteEntry *lastRouteEntry = 0;
                        for (EngineEntry::RouteEntryVector::iterator iroute = engineEntry.routeEntryVector.begin(),
                                                                     endiroute = engineEntry.routeEntryVector.end();
                             iroute != endiroute; ++iroute)
                        {
                            e2econnector::RouteEntry *routeEntry =
                                isubscriber->pipe.allocate<e2econnector::RouteEntry>(1);
                            routeEntry->nextRouteEntry = 0;
                            routeEntry->routeId = iroute->routeId;
                            routeEntry->isSharedMemory = iroute->isSharedMemory;
                            routeEntry->serialConnectionDistance = iroute->serialConnectionDistance;
                            if (lastRouteEntry == 0)
                            {
                                assert(firstRouteEntry == 0);
                                firstRouteEntry = lastRouteEntry = routeEntry;
                            }
                            else
                            {
                                assert(lastRouteEntry->nextRouteEntry == 0);
                                lastRouteEntry->nextRouteEntry = routeEntry;
                                lastRouteEntry = routeEntry;
                            }
                            assert(lastRouteEntry->nextRouteEntry == 0);
                        }

                        e2econnector::ServiceEntry *lastServiceEntry = 0;
                        for (ServiceEntryVector::iterator iservice = engineEntry.serviceEntryVector.begin(),
                                                          endiservice = engineEntry.serviceEntryVector.end();
                             iservice != endiservice; ++iservice)
                        {
                            e2econnector::ServiceEntry *serviceEntry =
                                isubscriber->pipe.allocate<e2econnector::ServiceEntry>(1);
                            serviceEntry->nextServiceEntry = 0;
                            serviceEntry->serviceName = Event::newCString(eventAllocator, iservice->serviceName);
                            serviceEntry->serviceInProcessActorId = iservice->serviceInProcessActorId;
                            if (lastServiceEntry == 0)
                            {
                                assert(firstServiceEntry == 0);
                                firstServiceEntry = lastServiceEntry = serviceEntry;
                            }
                            else
                            {
                                assert(lastServiceEntry->nextServiceEntry == 0);
                                lastServiceEntry->nextServiceEntry = serviceEntry;
                                lastServiceEntry = serviceEntry;
                            }
                            assert(lastServiceEntry->nextServiceEntry == 0);
                        }
                    }
                    e2econnector::SingletonClientEngineEvent &event =
                        isubscriber->pipe.push<e2econnector::SingletonClientEngineEvent>();
                    // below: no exception can be thrown
                    event.undeliveredBadAllocCauseFlag = false;
                    event.engineId = engineEntry.engineId;
                    event.engineName = engineName;
                    event.engineSuffix = engineSuffix;
                    event.firstRouteEntry = firstRouteEntry;
                    event.firstServiceEntry = firstServiceEntry;
                    subscribedNodeBit = false;
                }
            }
            assert(engineEntry.subscribedNodeBitSet.none());
            subscribersNotificationEngineEntryChain.pop_front();
            if (engineEntry.routeEntryVector.empty())
            {
                EngineEntryList::iterator i = engineEntryList.begin(), endi = engineEntryList.end();
                for (; i != endi && i->engineId != engineEntry.engineId; ++i)
                {
                }
                assert(i != endi);
                assert(&*i == &engineEntry);
                engineEntryList.erase(i);
            }
        }
    }
    catch (const std::bad_alloc &)
    {
        serviceActor.registerCallback(*this);
    }
}

Actor::NodeId
EngineToEngineConnector::ServiceActor::EventHandler::singletonClientNodeId(const ActorId &clientActorId) const
    noexcept
{
    SubscriberNodeEntryVector::const_iterator i = subscriberNodeEntryVector.begin(),
                                              endi = subscriberNodeEntryVector.end();
    for (; i != endi && (i->nodeId == MAX_NODE_COUNT || i->pipe.getDestinationActorId() != clientActorId); ++i)
    {
    }
    return i != endi ? i->nodeId : (NodeId)MAX_NODE_COUNT;
}

void EngineToEngineConnector::ServiceActor::EventHandler::singletonClientUnsubscribe(NodeId nodeId) noexcept
{
    assert(nodeId < MAX_NODE_COUNT);
    SubscriberNodeEntryVector::iterator isubscriber = subscriberNodeEntryVector.begin(),
                                        endisubscriber = subscriberNodeEntryVector.end();
    for (; isubscriber != endisubscriber && isubscriber->nodeId != nodeId; ++isubscriber)
    {
    }
    if (isubscriber != endisubscriber)
    {
        assert(nodeId < subscriberNodeEntryVector.size());
        for (SubscriberNodeEntryVector::iterator inext = isubscriber;
             ++inext != endisubscriber && inext->nodeId != MAX_NODE_COUNT; isubscriber = inext)
        {
            isubscriber->nodeId = inext->nodeId;
            isubscriber->pipe.setDestinationActorId(inext->pipe.getDestinationActorId());
        }
        assert(isubscriber != endisubscriber);
        isubscriber->nodeId = MAX_NODE_COUNT;
        for (EngineEntryList::iterator iengine = engineEntryList.begin(), endiengine = engineEntryList.end();
             iengine != endiengine; ++iengine)
        {
            if (iengine->subscribedNodeBitSet[nodeId])
            {
                iengine->subscribedNodeBitSet.set(nodeId, false);
                if (iengine->subscribedNodeBitSet.none())
                {
                    subscribersNotificationEngineEntryChain.remove(&*iengine);
                }
            }
        }
    }
}

EngineToEngineConnector::ServiceActor::Proxy::Proxy(Actor &actor)
    : singletonInnerActor(actor.newReferencedSingletonActor<SingletonInnerActor>()),
      proxyWeakReference((singletonInnerActor->proxyWeakReferenceList.push_back(ProxyWeakReference(this)),
                          singletonInnerActor->proxyWeakReferenceList.back()))
{
}

EngineToEngineConnector::ServiceActor::Proxy::~Proxy() noexcept { proxyWeakReference.proxy = 0; }

EngineToEngineConnector::ServiceActor::Proxy::SingletonInnerActor::SingletonInnerActor()
    : proxyWeakReferenceList(getAllocator()), engineInfoByIdMap(EngineInfoByIdMap::key_compare(), getAllocator())
{
    registerEventHandler<e2econnector::SingletonClientEngineEvent>(*this);
    e2econnector::SingletonClientSubscriptionEvent &singletonClientSubscriptionEvent =
        Event::Pipe(*this, Engine::getEngine().getServiceIndex().getServiceActorId<service::E2ERoute>())
            .push<e2econnector::SingletonClientSubscriptionEvent>();
    singletonClientSubscriptionEvent.clientNodeId = getActorId().getNodeId();
    singletonClientSubscriptionEvent.unsubscribeFlag = false;
}

EngineToEngineConnector::ServiceActor::Proxy::SingletonInnerActor::~SingletonInnerActor() noexcept
{
    try
    {
        e2econnector::SingletonClientSubscriptionEvent &singletonClientSubscriptionEvent =
            Event::Pipe(*this, Engine::getEngine().getServiceIndex().getServiceActorId<service::E2ERoute>())
                .push<e2econnector::SingletonClientSubscriptionEvent>();
        singletonClientSubscriptionEvent.clientNodeId = getActorId().getNodeId();
        singletonClientSubscriptionEvent.unsubscribeFlag = true;
    }
    catch (const std::bad_alloc &)
    {
        // nothing to do, service will detect unregistration next time it tries to send an update to this late singleton
    }
}

void EngineToEngineConnector::ServiceActor::Proxy::SingletonInnerActor::onEvent(
    const e2econnector::SingletonClientEngineEvent &event)
{
    try
    {
        EngineInfoByIdMap::iterator iengine = engineInfoByIdMap.find(event.engineId);
        if (event.firstRouteEntry == 0)
        {
            // engine removed
            assert(event.engineName == 0);
            assert(event.engineSuffix == 0);
            assert(event.firstServiceEntry == 0);
            if (iengine != engineInfoByIdMap.end())
            {
                notifyRemoveEngineRoute(iengine->first, iengine->second.routeInfoList);
            }
            engineInfoByIdMap.erase(iengine);
        }
        else if (iengine == engineInfoByIdMap.end())
        {
            // engine added
            iengine =
                engineInfoByIdMap.insert(EngineInfoByIdMap::value_type(event.engineId, EngineInfo(getAllocator())))
                    .first;
            try
            {
                iengine->second.engineName = event.engineName;
                iengine->second.engineSuffix = event.engineSuffix;
                iengine->second.serviceInfoVector.reserve(
                    e2econnector::ServiceEntry::getServiceEntryCount(event.firstServiceEntry));
                for (e2econnector::ServiceEntry *serviceEntry = event.firstServiceEntry; serviceEntry != 0;
                     serviceEntry = serviceEntry->nextServiceEntry)
                {
                    iengine->second.serviceInfoVector.push_back(
                        ServiceInfo(serviceEntry->serviceName, serviceEntry->serviceInProcessActorId, getAllocator()));
                }
                for (e2econnector::RouteEntry *routeEntry = event.firstRouteEntry; routeEntry != 0;
                     routeEntry = routeEntry->nextRouteEntry)
                {
                    iengine->second.routeInfoList.push_back(*routeEntry);
                }
            }
            catch (const std::bad_alloc &)
            {
                engineInfoByIdMap.erase(iengine);
                throw;
            }
            notifyAddEngineRoute(iengine->first, iengine->second.routeInfoList);
        }
        else
        {
            // engine route list updated
            assert(event.engineName != 0);
            assert(iengine->second.engineName == event.engineName);
            assert(event.engineSuffix != 0);
            assert(iengine->second.engineSuffix == event.engineSuffix);
#ifndef NDEBUG
            {
                ServiceInfoVector::iterator iservice = iengine->second.serviceInfoVector.begin(),
                                            endiservice = iengine->second.serviceInfoVector.end();
                e2econnector::ServiceEntry *serviceEntry = event.firstServiceEntry;
                for (; iservice != endiservice && serviceEntry != 0 &&
                       iservice->serviceName == serviceEntry->serviceName &&
                       iservice->serviceInProcessActorId == serviceEntry->serviceInProcessActorId;
                     ++iservice, serviceEntry = serviceEntry->nextServiceEntry)
                {
                }
                assert(iservice == endiservice && serviceEntry == 0);
            }
#endif
            RouteInfoList addedRouteInfoList(getAllocator());
            RouteInfoList removedRouteInfoList(getAllocator());
            try
            {
                for (RouteInfoList::iterator irouteInfo = iengine->second.routeInfoList.begin();
                     irouteInfo != iengine->second.routeInfoList.end();)
                {
                    e2econnector::RouteEntry *routeEntry = findRouteEntry(event.firstRouteEntry, irouteInfo->routeId);
                    if (routeEntry == 0)
                    {
                        removedRouteInfoList.push_back(*irouteInfo);
                        irouteInfo = iengine->second.routeInfoList.erase(irouteInfo);
                    }
                    else
                    {
                        assert(routeEntry->isSharedMemory == irouteInfo->isSharedMemory);
                        assert(routeEntry->serialConnectionDistance == irouteInfo->serialConnectionDistance);
                        ++irouteInfo;
                    }
                }
                for (e2econnector::RouteEntry *routeEntry = event.firstRouteEntry; routeEntry != 0;
                     routeEntry = routeEntry->nextRouteEntry)
                {
                    RouteInfoList::iterator irouteInfo =
                        findRouteInfo(iengine->second.routeInfoList, routeEntry->routeId);
                    if (irouteInfo == iengine->second.routeInfoList.end())
                    {
                        iengine->second.routeInfoList.push_back(*routeEntry);
                        try
                        {
                            addedRouteInfoList.push_back(iengine->second.routeInfoList.back());
                        }
                        catch (const std::bad_alloc &)
                        {
                            iengine->second.routeInfoList.pop_back();
                            throw;
                        }
                    }
                }
            }
            catch (const std::bad_alloc &)
            {
                if (!removedRouteInfoList.empty())
                {
                    notifyRemoveEngineRoute(iengine->first, removedRouteInfoList);
                }
                if (!addedRouteInfoList.empty())
                {
                    notifyAddEngineRoute(iengine->first, addedRouteInfoList);
                }
                throw;
            }
            if (!removedRouteInfoList.empty())
            {
                notifyRemoveEngineRoute(iengine->first, removedRouteInfoList);
            }
            if (!addedRouteInfoList.empty())
            {
                notifyAddEngineRoute(iengine->first, addedRouteInfoList);
            }
        }
    }
    catch (const std::bad_alloc &)
    {
        assert(event.undeliveredBadAllocCauseFlag == false);
        event.undeliveredBadAllocCauseFlag = true;
        throw ReturnToSenderException();
    }
}

//---- Notify Add Engine Route -------------------------------------------------

void EngineToEngineConnector::ServiceActor::Proxy::SingletonInnerActor::notifyAddEngineRoute(engine_id_type engineId, const RouteInfoList &routeInfoList) noexcept
{
    for (ProxyWeakReferenceList::iterator iproxy = proxyWeakReferenceList.begin(); iproxy != proxyWeakReferenceList.end();)
    {
        if (iproxy->proxy == 0)
        {
            iproxy = proxyWeakReferenceList.erase(iproxy);
        }
        else
        {
            iproxy->proxy->onAddEngineRoute(engineId, routeInfoList);
            ++iproxy;
        }
    }
}

void EngineToEngineConnector::ServiceActor::Proxy::SingletonInnerActor::notifyRemoveEngineRoute(
    engine_id_type engineId, const RouteInfoList &routeInfoList) noexcept
{
    for (ProxyWeakReferenceList::iterator iproxy = proxyWeakReferenceList.begin();
         iproxy != proxyWeakReferenceList.end();)
    {
        if (iproxy->proxy == 0)
        {
            iproxy = proxyWeakReferenceList.erase(iproxy);
        }
        else
        {
            iproxy->proxy->onRemoveEngineRoute(engineId, routeInfoList);
            ++iproxy;
        }
    }
}

EngineToEngineConnector::ServiceActor::Proxy::RouteInfoList::iterator
EngineToEngineConnector::ServiceActor::Proxy::SingletonInnerActor::findRouteInfo(
    RouteInfoList &routeInfoList, const Actor::ActorId::RouteId &routeId) noexcept
{
    RouteInfoList::iterator irouteInfo = routeInfoList.begin(), endirouteInfo = routeInfoList.end();
    for (; irouteInfo != endirouteInfo && irouteInfo->routeId != routeId; ++irouteInfo)
    {
    }
    return irouteInfo;
}

e2econnector::RouteEntry *EngineToEngineConnector::ServiceActor::Proxy::SingletonInnerActor::findRouteEntry(
    e2econnector::RouteEntry *firstRouteEntry, const Actor::ActorId::RouteId &routeId) noexcept
{
    e2econnector::RouteEntry *routeEntry = firstRouteEntry;
    for (; routeEntry != 0 && routeEntry->routeId != routeId; routeEntry = routeEntry->nextRouteEntry)
    {
    }
    return routeEntry;
}

//---- CTOR --------------------------------------------------------------------

EngineToEngineSerialConnector::EngineToEngineSerialConnector(Actor &actor, size_t serialBufferSize)
    : EngineToEngineConnector(actor), eventIdSerializeFnArray(0), eventIdDeserializeArray(0),
      readSerialBuffer(actor, serialBufferSize), writeSerialBuffer(actor, serialBufferSize), serialBufferCopy(0),
      distance(0)
{

    try
    {
        // both arrays are initialized on call to registerConnection()
        eventIdSerializeFnArray = Actor::Allocator<Actor::Event::EventE2ESerializeFunction>(getAllocator())
                                      .allocate(Actor::MAX_EVENT_ID_COUNT);
        eventIdDeserializeArray =
            Actor::Allocator<EventE2EDeserialize>(getAllocator()).allocate(Actor::MAX_EVENT_ID_COUNT);
        serialBufferCopy = Actor::Allocator<char>(getAllocator()).allocate(serialBufferCopySize);
    }
    catch (const std::bad_alloc &)
    {
        if (eventIdSerializeFnArray != 0)
        {
            Actor::Allocator<Actor::Event::EventE2ESerializeFunction>(getAllocator())
                .deallocate(eventIdSerializeFnArray, Actor::MAX_EVENT_ID_COUNT);
        }
        if (eventIdDeserializeArray != 0)
        {
            Actor::Allocator<EventE2EDeserialize>(getAllocator())
                .deallocate(eventIdDeserializeArray, Actor::MAX_EVENT_ID_COUNT);
        }
        if (serialBufferCopy != 0)
        {
            Actor::Allocator<char>(getAllocator()).deallocate(serialBufferCopy, serialBufferCopySize);
        }
        throw;
    }
}

EngineToEngineSerialConnector::~EngineToEngineSerialConnector() noexcept
{
    Actor::Allocator<Actor::Event::EventE2ESerializeFunction>(getAllocator())
        .deallocate(eventIdSerializeFnArray, Actor::MAX_EVENT_ID_COUNT);
    Actor::Allocator<EventE2EDeserialize>(getAllocator())
        .deallocate(eventIdDeserializeArray, Actor::MAX_EVENT_ID_COUNT);
    Actor::Allocator<char>(getAllocator()).deallocate(serialBufferCopy, serialBufferCopySize);
}

void EngineToEngineSerialConnector::writeSerialAbsoluteEventId(Actor::EventId eventId,
                                                                    const char *absoluteEventId)
{
    size_t absoluteEventIdSize = strlen(absoluteEventId);
    assert(absoluteEventIdSize <= std::numeric_limits<uint32_t>::max());
    {
        Actor::Event::SerialBuffer::WriteMark absoluteEventIdHeaderWriteMark =
            writeSerialBuffer.getCurrentWriteMark();
        writeSerialBuffer.increaseCurrentWriteBufferSize(sizeof(SerialAbsoluteEventIdHeader));
        if (writeSerialBuffer.getWriteMarkBufferSize(absoluteEventIdHeaderWriteMark) <
            sizeof(SerialAbsoluteEventIdHeader))
        {
            SerialAbsoluteEventIdHeader serialAbsoluteEventIdHeader(eventId, (uint32_t)absoluteEventIdSize);
            writeSerialBuffer.copyWriteBuffer(&serialAbsoluteEventIdHeader, sizeof(serialAbsoluteEventIdHeader),
                                              absoluteEventIdHeaderWriteMark);
        }
        else
        {
            new (writeSerialBuffer.getWriteMarkBuffer(absoluteEventIdHeaderWriteMark))
                SerialAbsoluteEventIdHeader(eventId, (uint32_t)absoluteEventIdSize);
        }
    }
    Actor::Event::SerialBuffer::WriteMark absoluteEventIdWriteMark = writeSerialBuffer.getCurrentWriteMark();
    writeSerialBuffer.increaseCurrentWriteBufferSize(absoluteEventIdSize);
    if (writeSerialBuffer.getWriteMarkBufferSize(absoluteEventIdWriteMark) < absoluteEventIdSize)
    {
        writeSerialBuffer.copyWriteBuffer(absoluteEventId, absoluteEventIdSize, absoluteEventIdWriteMark);
    }
    else
    {
        memcpy(writeSerialBuffer.getWriteMarkBuffer(absoluteEventIdWriteMark), absoluteEventId, absoluteEventIdSize);
    }
}

bool EngineToEngineSerialConnector::readSerialRouteEvent()
{
    assert(readSerialBuffer.size() >= sizeof(uint8_t));
    assert(readSerialBuffer.getCurrentReadBufferSize() >= sizeof(uint8_t));
    assert(*static_cast<const uint8_t *>(readSerialBuffer.getCurrentReadBuffer()) == RouteEventType);
    if (readSerialBuffer.size() < sizeof(SerialRouteEventHeader))
    {
        return false;
    }
    char serialRouteEventHeaderBufferChar[sizeof(SerialRouteEventHeader)];
    void *serialRouteEventHeaderBuffer = serialRouteEventHeaderBufferChar;
    readSerialBuffer.copyReadBuffer(serialRouteEventHeaderBuffer, sizeof(SerialRouteEventHeader));
    size_t serialEventSize = static_cast<SerialRouteEventHeader *>(serialRouteEventHeaderBuffer)->getEventSize();
    if (readSerialBuffer.size() < sizeof(SerialRouteEventHeader) + serialEventSize)
    {
        return false;
    }
    if ((int)serialEventSize > serialBufferCopySize)
    {
        char *localSerialBufferCopy = Actor::Allocator<char>(getAllocator()).allocate(serialEventSize);
        try
        {
            readSerialBuffer.decreaseCurrentReadBufferSize(sizeof(SerialRouteEventHeader));
            assert(readSerialBuffer.size() >= serialEventSize);
            readSerialBuffer.copyReadBuffer(localSerialBufferCopy, serialEventSize);
            readSerialBuffer.decreaseCurrentReadBufferSize(serialEventSize);
            readSerialEvent(*static_cast<SerialRouteEventHeader *>(serialRouteEventHeaderBuffer), localSerialBufferCopy,
                            serialEventSize);
        }
        catch (...)
        {
            Actor::Allocator<char>(getAllocator()).deallocate(localSerialBufferCopy, serialEventSize);
            throw;
        }
        Actor::Allocator<char>(getAllocator()).deallocate(localSerialBufferCopy, serialEventSize);
    }
    else
    {
        readSerialBuffer.decreaseCurrentReadBufferSize(sizeof(SerialRouteEventHeader));
        assert(readSerialBuffer.size() >= serialEventSize);
        readSerialBuffer.copyReadBuffer(serialBufferCopy, serialEventSize);
        readSerialBuffer.decreaseCurrentReadBufferSize(serialEventSize);
        readSerialEvent(*static_cast<SerialRouteEventHeader *>(serialRouteEventHeaderBuffer), serialBufferCopy,
                        serialEventSize);
    }
    return true;
}

bool EngineToEngineSerialConnector::readSerialReturnEvent()
{
    assert(readSerialBuffer.size() >= sizeof(uint8_t));
    assert(readSerialBuffer.getCurrentReadBufferSize() >= sizeof(uint8_t));
    assert(*static_cast<const uint8_t *>(readSerialBuffer.getCurrentReadBuffer()) == ReturnEventType);
    if (readSerialBuffer.size() < sizeof(SerialReturnEventHeader))
    {
        return false;
    }
    char serialReturnEventHeaderBufferChar[sizeof(SerialReturnEventHeader)];
    void *serialReturnEventHeaderBuffer = serialReturnEventHeaderBufferChar;
    readSerialBuffer.copyReadBuffer(serialReturnEventHeaderBuffer, sizeof(SerialReturnEventHeader));
    size_t serialEventSize = static_cast<SerialReturnEventHeader *>(serialReturnEventHeaderBuffer)->getEventSize();
    if (readSerialBuffer.size() < sizeof(SerialReturnEventHeader) + serialEventSize)
    {
        return false;
    }
    if ((int)serialEventSize > serialBufferCopySize)
    {
        char *localSerialBufferCopy = Actor::Allocator<char>(getAllocator()).allocate(serialEventSize);
        try
        {
            readSerialBuffer.decreaseCurrentReadBufferSize(sizeof(SerialReturnEventHeader));
            assert(readSerialBuffer.size() >= serialEventSize);
            readSerialBuffer.copyReadBuffer(localSerialBufferCopy, serialEventSize);
            readSerialBuffer.decreaseCurrentReadBufferSize(serialEventSize);
            readSerialUndeliveredEvent(*static_cast<SerialReturnEventHeader *>(serialReturnEventHeaderBuffer),
                                       localSerialBufferCopy, serialEventSize);
        }
        catch (...)
        {
            Actor::Allocator<char>(getAllocator()).deallocate(localSerialBufferCopy, serialEventSize);
            throw;
        }
        Actor::Allocator<char>(getAllocator()).deallocate(localSerialBufferCopy, serialEventSize);
    }
    else
    {
        readSerialBuffer.decreaseCurrentReadBufferSize(sizeof(SerialReturnEventHeader));
        assert(readSerialBuffer.size() >= serialEventSize);
        readSerialBuffer.copyReadBuffer(serialBufferCopy, serialEventSize);
        readSerialBuffer.decreaseCurrentReadBufferSize(serialEventSize);
        readSerialUndeliveredEvent(*static_cast<SerialReturnEventHeader *>(serialReturnEventHeaderBuffer),
                                   serialBufferCopy, serialEventSize);
    }
    return true;
}

void EngineToEngineSerialConnector::readSerialUndeliveredEvent(const SerialReturnEventHeader &header,
                                                                    const void *eventSerialBuffer,
                                                                    size_t eventSerialBufferSize)
{
    Actor::EventId peerEventId = header.getEventId();
    EventE2EDeserialize *eventE2EDeserialize;
    if (peerEventId >= Actor::MAX_EVENT_ID_COUNT ||
        *(eventE2EDeserialize = eventIdDeserializeArray + peerEventId) == tredzone::null)
    {
        throw SerialException(peerEventId >= Actor::MAX_EVENT_ID_COUNT ? SerialException::OutOfRangeEventIdReason
                                                                            : SerialException::UndefinedEventIdReason);
    }
    assert(eventE2EDeserialize->deserializeFunction != 0);
    SerialEventHeader returnHeader = header.swapSourceDestination();
    EngineToEngineConnectorEventFactory eventFactory(*this, returnHeader
#ifndef NDEBUG
                                                          ,
                                                          eventE2EDeserialize->localEventId
#endif
                                                          );
    assert(header.getEventSize() == eventSerialBufferSize);
    (*eventE2EDeserialize->deserializeFunction)(eventFactory, eventSerialBuffer, eventSerialBufferSize);
    eventFactory.toBeUndeliveredRoutedEvent();
}

void EngineToEngineSerialConnector::readSerialAbsoluteIdEvent(const SerialAbsoluteEventIdHeader &header,
                                                                   const char *absoluteEventId)
{
    Actor::EventId peerEventId = header.getEventId();
    if (peerEventId >= Actor::MAX_EVENT_ID_COUNT)
    {
        throw SerialException(SerialException::OutOfRangeEventIdReason);
    }
    Actor::EventId localEventId;
    Actor::Event::EventE2ESerializeFunction eventSerializeFn;
    Actor::Event::EventE2EDeserializeFunction eventDeserializeFn;
    if (isEventE2ECapable(absoluteEventId, localEventId, eventSerializeFn, eventDeserializeFn) == false)
    {
        return;
    }
    assert(localEventId < Actor::MAX_EVENT_ID_COUNT);
    eventIdDeserializeArray[peerEventId].localEventId = localEventId;
    eventIdDeserializeArray[peerEventId].deserializeFunction = eventDeserializeFn;
}

bool EngineToEngineSerialConnector::readSerialAbsoluteIdEvent()
{
    assert(readSerialBuffer.size() >= sizeof(uint8_t));
    assert(readSerialBuffer.getCurrentReadBufferSize() >= sizeof(uint8_t));
    assert(*static_cast<const uint8_t *>(readSerialBuffer.getCurrentReadBuffer()) == AbsoluteIdEventType);
    if (readSerialBuffer.size() < sizeof(SerialAbsoluteEventIdHeader))
    {
        return false;
    }
    char serialAbsoluteEventIdHeaderBufferChar[sizeof(SerialAbsoluteEventIdHeader)];
    void *serialAbsoluteEventIdHeaderBuffer = serialAbsoluteEventIdHeaderBufferChar;
    readSerialBuffer.copyReadBuffer(serialAbsoluteEventIdHeaderBuffer, sizeof(SerialAbsoluteEventIdHeader));
    size_t serialAbsoluteEventIdSize =
        static_cast<SerialAbsoluteEventIdHeader *>(serialAbsoluteEventIdHeaderBuffer)->getAbsoluteEventIdSize();
    if (readSerialBuffer.size() < sizeof(SerialAbsoluteEventIdHeader) + serialAbsoluteEventIdSize)
    {
        return false;
    }
    if ((int)serialAbsoluteEventIdSize + 1 > serialBufferCopySize)                  // appends zero to serialized buff?
    { // +1 to include null char
        char *localSerialBufferCopy =
            Actor::Allocator<char>(getAllocator()).allocate(serialAbsoluteEventIdSize + 1);
        try
        {
            readSerialBuffer.decreaseCurrentReadBufferSize(sizeof(SerialAbsoluteEventIdHeader));
            assert(readSerialBuffer.size() >= serialAbsoluteEventIdSize);
            readSerialBuffer.copyReadBuffer(localSerialBufferCopy, serialAbsoluteEventIdSize);
            readSerialBuffer.decreaseCurrentReadBufferSize(serialAbsoluteEventIdSize);
            localSerialBufferCopy[serialAbsoluteEventIdSize] = '\0';
            readSerialAbsoluteIdEvent(*static_cast<SerialAbsoluteEventIdHeader *>(serialAbsoluteEventIdHeaderBuffer),
                                      localSerialBufferCopy);
        }
        catch (...)
        {
            Actor::Allocator<char>(getAllocator())
                .deallocate(localSerialBufferCopy, serialAbsoluteEventIdSize + 1);
            throw;
        }
        Actor::Allocator<char>(getAllocator()).deallocate(localSerialBufferCopy, serialAbsoluteEventIdSize + 1);
    }
    else
    {
        readSerialBuffer.decreaseCurrentReadBufferSize(sizeof(SerialAbsoluteEventIdHeader));
        assert(readSerialBuffer.size() >= serialAbsoluteEventIdSize);
        readSerialBuffer.copyReadBuffer(serialBufferCopy, serialAbsoluteEventIdSize);
        readSerialBuffer.decreaseCurrentReadBufferSize(serialAbsoluteEventIdSize);
        serialBufferCopy[serialAbsoluteEventIdSize] = '\0';
        readSerialAbsoluteIdEvent(*static_cast<SerialAbsoluteEventIdHeader *>(serialAbsoluteEventIdHeaderBuffer),
                                  serialBufferCopy);
    }
    return true;
}

void EngineToEngineSharedMemoryConnector::registerConnection(const char *peerEngineName,
                                                                  const char *peerEngineSuffix,
                                                                  const ServiceEntryVector &serviceEntryVector,
                                                                  SharedMemory &psharedMemory)
{
    unregisterConnection();
    if (!psharedMemory.isConnected())
    {
        throw NotConnectedSharedMemoryException();
    }
    EngineToEngineConnector::registerConnection(peerEngineName, peerEngineSuffix, serviceEntryVector,
                                                     &onWriteSerialEvent, &onWriteSerialUndeliveredEvent, true, 0);
    assert(sharedMemory == 0);
    sharedMemory = &psharedMemory;
    actor.registerCallback(preBarrierCallback);
}

void EngineToEngineSharedMemoryConnector::unregisterConnection() noexcept
{
    EngineToEngineConnector::unregisterConnection();
    sharedMemory = 0;
    preBarrierCallback.unregister();
    postBarrierCallback.unregister();
}

EngineToEngineSharedMemoryConnector::SharedMemory::SharedMemory(const Actor &actor, void *ptr, size_t sz)
    : readControl(0), writeControl(0), eventPageSize(actor.asyncNode->getEventAllocatorPageSize()),
      lastConnectionStatus(ConnectionControl::UndefinedStatus)
{
    assert(sizeof(ConnectionControl) % CACHE_LINE_SIZE == 0);
    assert(sizeof(ReadWriteControl) % CACHE_LINE_SIZE == 0);
    uintptr_t cacheLigneAlignementOffset = ((uintptr_t)ptr) % CACHE_LINE_SIZE;
    size_t minSz = cacheLigneAlignementOffset + sizeof(ConnectionControl) + 2 * sizeof(ReadWriteControl) +
                   2 * (2 * (actor.asyncNode->getEventAllocatorPageSize() + sizeof(ReadWriteControl::Chain::Entry)) +
                        CACHE_LINE_SIZE - 1);
    if (sz < minSz)
    {
        throw UndersizedSharedMemoryException();
    }
    connectionControl = reinterpret_cast<ConnectionControl *>(cacheLigneAlignementOffset + (uintptr_t)ptr);
    eventPagesMemorySize = sz - (cacheLigneAlignementOffset + sizeof(ConnectionControl) + 2 * sizeof(ReadWriteControl));
}

EngineToEngineSharedMemoryConnector::SharedMemory::~SharedMemory() noexcept
{
    if (writeControl != 0)
    {
        assert(readControl != 0);
        connectionControl->releaseControl(writeControl, lastConnectionStatus == ConnectionControl::ConnectedStatus);
    }
}

bool EngineToEngineSharedMemoryConnector::SharedMemory::tryConnect() noexcept
{
    if ((lastConnectionStatus == ConnectionControl::ConnectedStatus &&
         connectionControl->concurrent.connectionStatusEnum != ConnectionControl::ConnectedStatus) ||
        (lastConnectionStatus == ConnectionControl::ConnectingStatus &&
         connectionControl->concurrent.connectionStatusEnum == ConnectionControl::DisconnectedStatus))
    {
        // a disconnection occured
        assert(readControl != 0);
        assert(writeControl != 0);
        connectionControl->releaseControl(writeControl, lastConnectionStatus == ConnectionControl::ConnectedStatus);
        lastConnectionStatus = ConnectionControl::UndefinedStatus;
        readControl = writeControl = 0;
    }
    if (lastConnectionStatus == ConnectionControl::UndefinedStatus)
    {
        assert(readControl == 0);
        assert(writeControl == 0);
        if (connectionControl->retainControl(readControl, writeControl) == false)
        {
            return false;
        }
        lastConnectionStatus = ConnectionControl::DisconnectedStatus; // processing will be carried on at next
                                                                      // tryConnect() call with a memory barrier
                                                                      // occurring in between (as a result of an actor
                                                                      // callback)
        return false;
    }
    bool connectionEstablishedFlag = false;
    if (lastConnectionStatus == ConnectionControl::DisconnectedStatus)
    {
        if (atomicCompareAndSwap(&connectionControl->concurrent.connectionStatusEnum,
                                 (sig_atomic_t)ConnectionControl::DisconnectedStatus,
                                 (sig_atomic_t)ConnectionControl::ConnectingStatus))
        {
            lastConnectionStatus = ConnectionControl::ConnectingStatus;
            return false;
        }
        else if (atomicCompareAndSwap(&connectionControl->concurrent.connectionStatusEnum,
                                      (sig_atomic_t)ConnectionControl::ConnectingStatus,
                                      (sig_atomic_t)ConnectionControl::ConnectedStatus))
        {
            connectionEstablishedFlag = true;
        }
        else
        {
            TRZ_DEBUG_BREAK();

            connectionControl->releaseControl(writeControl, false);
            lastConnectionStatus = ConnectionControl::UndefinedStatus;
            readControl = writeControl = 0;
            return false;
        }
    }
    if (connectionEstablishedFlag ||
        (lastConnectionStatus == ConnectionControl::ConnectingStatus &&
         connectionControl->concurrent.connectionStatusEnum == ConnectionControl::ConnectedStatus))
    {
        sig_atomic_t &oppositeReadWriteControlInUseFlag =
            (uintptr_t)(connectionControl + 1) == (uintptr_t)writeControl
                ? connectionControl->concurrent.secondReadWriteControlInUseFlag
                : connectionControl->concurrent.firstReadWriteControlInUseFlag;
        if (atomicCompareAndSwap(&oppositeReadWriteControlInUseFlag, 1, 2) == false)
        {
            // failed to lock opposite writer
            connectionControl->releaseControl(writeControl, false);
            lastConnectionStatus = ConnectionControl::UndefinedStatus;
            readControl = writeControl = 0;
            return false;
        }
        lastConnectionStatus = ConnectionControl::ConnectedStatus;
        nextConcurrentReadCount = 0;
        nextConcurrentWriteCount = 1;
        toBeDeliveredEventChain.clear();
        availableEventPageChain.clear();
        inUseEventPageChain.clear();
        size_t eventPageCount = (((eventPagesMemorySize / 2) / CACHE_LINE_SIZE) * CACHE_LINE_SIZE) /
                                (eventPageSize + sizeof(ReadWriteControl::Chain::Entry));
        assert(eventPageCount >= 2);
        void *eventPageStartPtr =
            (uintptr_t)writeControl == (uintptr_t)(connectionControl + 1)
                ? readControl + 1
                : reinterpret_cast<void *>((uintptr_t)(writeControl + 1) + eventPagesMemorySize / 2);
        for (size_t i = 0; i < eventPageCount; ++i)
        {
            availableEventPageChain.push_front(
                *connectionControl,
                *reinterpret_cast<ReadWriteControl::Chain::Entry *>(
                    ((uintptr_t)eventPageStartPtr) + i * (eventPageSize + sizeof(ReadWriteControl::Chain::Entry))));
        }
        currentEventPagePtr = &availableEventPageChain.pop_front(*connectionControl) + 1;
        currentEventPageOffset = 0;
        return true;
    }
    return isConnected();
}

bool EngineToEngineSharedMemoryConnector::SharedMemory::isConnected() noexcept
{
    return lastConnectionStatus == ConnectionControl::ConnectedStatus &&
           connectionControl->concurrent.connectionStatusEnum == ConnectionControl::ConnectedStatus;
}

bool EngineToEngineSharedMemoryConnector::SharedMemory::ConnectionControl::retainControl(
    ReadWriteControl *&readControl, ReadWriteControl *&writeControl) noexcept
{
    assert(readControl == 0);
    assert(writeControl == 0);
    if (concurrent.connectionStatusEnum == ConnectedStatus)
    {
        return false;
    }
    void *readWriteControlPtr = this + 1;
    if (atomicCompareAndSwap(&concurrent.firstReadWriteControlInUseFlag, 0, 1))
    {
        writeControl = static_cast<ReadWriteControl *>(readWriteControlPtr);
        readControl = writeControl + 1;
    }
    else if (atomicCompareAndSwap(&concurrent.secondReadWriteControlInUseFlag, 0, 1))
    {
        readControl = static_cast<ReadWriteControl *>(readWriteControlPtr);
        writeControl = readControl + 1;
    }
    else
    {
        return false;
    }
    memset(writeControl, 0, sizeof(*writeControl));
    return true;
}

void EngineToEngineSharedMemoryConnector::SharedMemory::ConnectionControl::releaseControl(
    ReadWriteControl *writeControl, bool releaseConnectionFlag) noexcept
{
    while (!atomicCompareAndSwap(&concurrent.connectionStatusEnum, (sig_atomic_t)DisconnectedStatus,
                                 concurrent.connectionStatusEnum))
    {
    }
    assert(concurrent.connectionStatusEnum == DisconnectedStatus);
    if ((uintptr_t)writeControl == (uintptr_t)(this + 1))
    {
        assert(concurrent.firstReadWriteControlInUseFlag > 0);
        atomicSubAndFetch(&concurrent.firstReadWriteControlInUseFlag, 1);
        if (releaseConnectionFlag)
        {
            assert(concurrent.secondReadWriteControlInUseFlag > 0);
            atomicSubAndFetch(&concurrent.secondReadWriteControlInUseFlag, 1);
        }
    }
    else
    {
        assert((uintptr_t)writeControl == (uintptr_t)(this + 1) + sizeof(ReadWriteControl));
        assert(concurrent.secondReadWriteControlInUseFlag > 1);
        atomicSubAndFetch(&concurrent.secondReadWriteControlInUseFlag, 1);
        if (releaseConnectionFlag)
        {
            assert(concurrent.firstReadWriteControlInUseFlag > 0);
            atomicSubAndFetch(&concurrent.firstReadWriteControlInUseFlag, 1);
        }
    }
}

EngineToEngineSharedMemoryConnector::SharedMemory::ReadWriteControl::offset_type
EngineToEngineSharedMemoryConnector::SharedMemory::ReadWriteControl::toOffset(ConnectionControl &connectionControl,
                                                                                   void *pointer) noexcept
{
    assert(sizeof(offset_type) >= sizeof(uintptr_t));
    assert((uintptr_t)pointer > (uintptr_t)&connectionControl);
    return (offset_type)(((uintptr_t)pointer) - ((uintptr_t)&connectionControl));
}

void *EngineToEngineSharedMemoryConnector::SharedMemory::ReadWriteControl::fromOffset(
    ConnectionControl &connectionControl, offset_type offset) noexcept
{
    assert(offset != 0);
    return reinterpret_cast<void *>(((uintptr_t)&connectionControl) + offset);
}

void EngineToEngineSharedMemoryConnector::SharedMemory::ReadWriteControl::Chain::push_back(
    ConnectionControl &connectionControl, Entry &entry) noexcept
{
    entry.next = 0;
    if (tail == 0)
    {
        assert(head == 0);
        head = tail = toOffset(connectionControl, &entry);
    }
    else
    {
        offset_type newTail = toOffset(connectionControl, &entry);
        static_cast<Entry *>(fromOffset(connectionControl, tail))->next = newTail;
        tail = newTail;
    }
}

void EngineToEngineSharedMemoryConnector::SharedMemory::ReadWriteControl::Chain::push_front(
    ConnectionControl &connectionControl, Entry &entry) noexcept
{
    entry.next = head;
    if (head == 0)
    {
        assert(tail == 0);
        head = tail = toOffset(connectionControl, &entry);
    }
    else
    {
        head = toOffset(connectionControl, &entry);
    }
}

void EngineToEngineSharedMemoryConnector::SharedMemory::ReadWriteControl::Chain::push_front(
    ConnectionControl &connectionControl, Chain &chain) noexcept
{
    if (head != 0 && chain.head != 0)
    {
        static_cast<Entry *>(fromOffset(connectionControl, chain.tail))->next = head;
        head = chain.head;
    }
    else
    {
        if (head == 0)
        {
            assert(tail == 0);
            head = chain.head;
            tail = chain.tail;
        }
        else
        {
            assert(chain.tail == 0);
        }
    }
    chain.head = chain.tail = 0;
}

EngineToEngineSharedMemoryConnector::SharedMemory::ReadWriteControl::Chain::Entry &
EngineToEngineSharedMemoryConnector::SharedMemory::ReadWriteControl::Chain::pop_front(
    ConnectionControl &connectionControl) noexcept
{
    assert(head != 0);
    Entry &ret = *static_cast<Entry *>(fromOffset(connectionControl, head));
    head = ret.next;
    if (head == 0)
    {
        tail = 0;
    }
    return ret;
}

void EngineToEngineSharedMemoryConnector::SharedMemory::ReadWriteControl::Chain::clear() noexcept
{
    head = tail = 0;
}

Actor::Event::AllocatorBase EngineToEngineConnectorEventFactory::getAllocator() noexcept
{
    eventAllocatorFactory.context =
        &serialConnector.asyncNode
             .getReferenceToWriterShared(serialEventHeader.getDestinationInProcessActorId().getNodeId())
             .writeCache;
    eventAllocatorFactory.allocateFn = &Actor::Event::Pipe::allocateInProcessEvent;
    eventAllocatorFactory.allocateAndGetIndexFn = &Actor::Event::Pipe::allocateInProcessEvent;
#ifndef NDEBUG
    return Actor::Event::AllocatorBase(eventAllocatorFactory, &Actor::Event::Pipe::batchInProcessEvent);
#else
    return Actor::Event::AllocatorBase(eventAllocatorFactory);
#endif
}

void *EngineToEngineConnectorEventFactory::allocate(size_t sz)
{
    return serialConnector.asyncNode
        .getReferenceToWriterShared(serialEventHeader.getDestinationInProcessActorId().getNodeId())
        .writeCache.allocateEvent(sz);
}

void *EngineToEngineConnectorEventFactory::allocateEvent(size_t sz,
                                                              Actor::Event::Chain *&destinationEventChain)
{
    Actor::NodeId destinationNodeId = serialEventHeader.getDestinationInProcessActorId().getNodeId();
    AsyncNodesHandle::Shared::WriteCache &writeCache =
        serialConnector.asyncNode.getReferenceToWriterShared(destinationNodeId).writeCache;
    destinationEventChain = &writeCache.toBeDeliveredEventChain;
    Actor::ActorId::RouteId *ret = new (writeCache.allocateEvent(sz + sizeof(Actor::ActorId::RouteId)))
        Actor::ActorId::RouteId(serialConnector.getRouteId());
    serialConnector.asyncNode.setWriteSignal(destinationNodeId);
    return ret + 1;
}

void EngineToEngineConnectorEventFactory::toBeUndeliveredRoutedEvent() noexcept
{
    assert(event != 0);
    assert(destinationEventChain ==
           &serialConnector.asyncNode
                .getReferenceToWriterShared(serialEventHeader.getDestinationInProcessActorId().getNodeId())
                .writeCache.toBeDeliveredEventChain);
    destinationEventChain =
        &serialConnector.asyncNode
             .getReferenceToWriterShared(serialEventHeader.getDestinationInProcessActorId().getNodeId())
             .writeCache.toBeUndeliveredRoutedEventChain;
    assert(event->isRouteToSource());
    ++(event->routeOffset);
    assert(event->isRouteToDestination());
    std::swap(event->sourceActorId, event->destinationActorId);
}

void EngineToEngineSharedMemoryConnector::onWriteSerialEvent(EngineToEngineConnector *,
                                                                  const Actor::Event &)
{
    breakThrow(FeatureNotImplementedException());
}

void EngineToEngineSharedMemoryConnector::onWriteSerialUndeliveredEvent(EngineToEngineConnector *,
                                                                             const Actor::Event &)
{
    breakThrow(FeatureNotImplementedException());
}

void EngineToEngineSharedMemoryConnector::PreBarrierCallback::onCallback() noexcept
{
    breakThrow(FeatureNotImplementedException());
}

void EngineToEngineSharedMemoryConnector::PostBarrierCallback::onCallback() noexcept
{
    breakThrow(FeatureNotImplementedException());
}

} // namespace