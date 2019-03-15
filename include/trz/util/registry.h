/**
 * @file registry.h
 * @brief utility registry service
 * @copyright 2013-2019 Tredzone (www.tredzone.com). All rights reserved.
 * Please see accompanying LICENSE file for licensing terms.
 */
 
#pragma once

// Q: may leak event string if is too large? [PL]

#include <unordered_map>
#include <unordered_set>
#include <string>
#include <utility>

#include "simplx.h"

#include "trz/e2e/e2e.h"

namespace tredzone
{
// import into namespace
using std::string;
using std::unordered_map;
using std::unordered_set;
using std::make_pair;
using tredzone::EngineToEngineConnectorEventFactory;

//---- Register key/id pair event ----------------------------------------------

struct RegisterEvent : public Actor::Event
{
public:
    RegisterEvent(const string &key, const Actor::ActorId &id)
        : m_Key(key), m_Id(id)
    {
    }
    
    /*
    static
    bool isE2ECapable(const char*& absoluteEventId, e2econnector::EventE2ESerializeFunction& serializeFn, e2econnector::EventE2EDeserializeFunction& deserializeFn)
    {
        absoluteEventId = "RegisterEvent";

        // return de/serialization pointers
        serializeFn = e2eSerializeFunction;
        deserializeFn = e2eDeserializeFunction;

        return true;
    }

    static
    void e2eSerializeFunction(e2econnector::SerialBuffer& serialBuffer, const RegisterEvent& e)
    {
        serialBuffer << e;
    }

    static
    void e2eDeserializeFunction(EngineToEngineConnectorEventFactory& eventFactory, const void* buffer, size_t sz)
    {
        const RegisterEvent *myEventDataPtr = static_cast<const RegisterEvent*>(buffer);
        eventFactory.newEvent<RegisterEvent>(RegisterEvent->m_Key, RegisterEvent->m_Id));
    }
    */

#pragma pack(push)
#pragma pack(1)

    const string            m_Key;
    const Actor::ActorId    m_Id;

#pragma pack(pop)
};

//---- Actor Id Snapshot Request event -----------------------------------------

struct ActorIdSnapshotRequestEvent : public Actor::Event
{
public:
    ActorIdSnapshotRequestEvent(const string &key)
        : m_Key(key)
    {
    }

    const string    m_Key;
};

//---- Actor Id Subscription Request event -------------------------------------

struct ActorIdSubscribeRequestEvent : public Actor::Event
{
public:
    ActorIdSubscribeRequestEvent(const string &key)
        : m_Key(key)
    {
    }

    const string m_Key;
};

//---- Actor Id Response event -------------------------------------------------

struct ActorIdResponseEvent : public Actor::Event
{
public:
    ActorIdResponseEvent(const string &key, const Actor::ActorId &id)
        : m_Key(key), m_Id(id)
    {
    }

    const string            m_Key;
    const Actor::ActorId    m_Id;
};

//---- Not Registered Actor event (REALLY NEEDED?) -----------------------------

struct NotRegisteredActorEvent : public Actor::Event
{
public:

    NotRegisteredActorEvent(const string &key)
        : m_Key(key)
    {
    }
    
    const string m_Key;
};

//---- Registry SERVICE --------------------------------------------------------

class Registry : public Actor
{
public:

    struct Tag
    {
        static const char *name() noexcept { return "Registry"; }
    };

    // ctor
    Registry()
    {
        registerEventHandler<RegisterEvent>(*this);
        registerEventHandler<ActorIdSnapshotRequestEvent>(*this);
        registerEventHandler<ActorIdSubscribeRequestEvent>(*this);
    }

    // on register event
    void onEvent(const RegisterEvent &e)
    {
        m_RegisteredActors.emplace(make_pair(e.m_Key, e.m_Id));
        
        // notify all subscribers
        auto subscriber_set = m_Subscribers.at(e.m_Key);
        for (auto subId : subscriber_set)
        {
            Event::Pipe pipe(*this, subId);
            pipe.push<ActorIdResponseEvent>(e.m_Key, e.m_Id);
        }
    }

    // snapshot request
    void onEvent(const ActorIdSnapshotRequestEvent &e)
    {
        processSnapshot(e.m_Key, e.getSourceActorId());
    }

    // subscribe request
    void onEvent(const ActorIdSubscribeRequestEvent &e)
    {
        addSubscriber(e.getSourceActorId(), e.m_Key);
        processSnapshot(e.m_Key, e.getSourceActorId());
    }

private:

    // add subscriber
    void addSubscriber(const ActorId &subscriber, const string &key)
    {
        // a key can have multiple subscribers
        // assert(!m_Subscribers.count(key));
        
        m_Subscribers[key].insert(subscriber);
    }

    // remove susbcriber
    void removeSubscriber(const ActorId &subscriber_id, const string &key)
    {
        assert(m_Subscribers.count(key));                       // key exists?
        assert(m_Subscribers.at(key).count(subscriber_id));     // subscriber exists
        
        m_Subscribers.at(key).erase(subscriber_id);
    }

    void processSnapshot(const string &key, const ActorId &sourceActorId)
    {
        Event::Pipe pipe(*this, sourceActorId);

        auto it_id = m_RegisteredActors.find(key);
        if (it_id == m_RegisteredActors.end())
        {
            // not found
            pipe.push<NotRegisteredActorEvent>(key);
        }
        else
        {   // found
            pipe.push<ActorIdResponseEvent>(key, it_id->second);
        }
    }

    unordered_map<string, ActorId>                  m_RegisteredActors;
    unordered_map<string, unordered_set<ActorId>>   m_Subscribers;
};

//---- Registry callback -------------------------------------------------------

class RegistryCallback
{
public:

    virtual void onRegistryActorIdResponseSuccess(const string &key, const Actor::ActorId &id)
    {   (void)key;
        (void)id;
    };
    virtual void onRegistryActorIdResponseFailure(const string &key)
    {   (void)key;
    };
};

//---- Registry PROXY ----------------------------------------------------------

class RegistryProxy
{
public:
    RegistryProxy(Actor &source, RegistryCallback &callback)
        : m_Pipe(source), m_Callback(callback)
    {
        const Actor::ActorId id = source.getEngine().getServiceIndex().getServiceActorId<Registry::Tag>();
        // check service exists
        assert(id != Actor::ActorId());
        m_Pipe.setDestinationActorId(id);

        source.registerEventHandler<ActorIdResponseEvent>(*this);
        source.registerEventHandler<NotRegisteredActorEvent>(*this);
    }

    void registerName(const string &key)
    {
        m_Pipe.push<RegisterEvent>(key, m_Pipe.getSourceActorId());
    }

    void requestRegistry(const string &key)
    {
        m_Pipe.push<ActorIdSnapshotRequestEvent>(key);
    }

    void subscribeRegistry(const string &key)
    {
        m_Pipe.push<ActorIdSubscribeRequestEvent>(key);
    }

    void unsubscribe(const string &key)
    {
        // missing code?
        (void)key;
    }

    void onEvent(const NotRegisteredActorEvent &e)
    {
        m_Callback.onRegistryActorIdResponseFailure(e.m_Key);
    }

    void onEvent(const ActorIdResponseEvent &e)
    {
        m_Callback.onRegistryActorIdResponseSuccess(e.m_Key, e.m_Id);
    }

private:

    Actor::Event::Pipe  m_Pipe;
    RegistryCallback    &m_Callback;
};

} // namespace tredzone
