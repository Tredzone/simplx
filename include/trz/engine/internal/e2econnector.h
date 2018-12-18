/**
 * @file e2econnector.h
 * @brief Simplx Engine-To-Engine (cluster) connector
 * @copyright 2013-2018 Tredzone (www.tredzone.com). All rights reserved.
 * Please see accompanying LICENSE file for licensing terms.
 */

#pragma once

#include <bitset>
#include <csignal>
#include <map>
#include <vector>

#include "trz/engine/actor.h"
#include "trz/engine/platform.h"

namespace tredzone
{

/**
 * @brief Helper class for Service tags.
 * @note Service tags must have a name() method.
 */
struct Service
{
    inline static const char *name() noexcept { return ""; }
};

namespace service
{
    struct E2ERoute : Service
    {
    };

} // namespace service

namespace e2econnector
{

typedef uint8_t connection_distance_type;
typedef uint64_t engine_id_type;

#pragma pack(push)
#pragma pack(1)
/**
 * @brief Route entry base information.
 */
struct RouteEntryBase
{
    Actor::ActorId::RouteId routeId;
    bool isSharedMemory;
    connection_distance_type serialConnectionDistance;
};

struct RouteEntry : RouteEntryBase
{
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

/**
 * @brief ServiceEntry class
 */
struct ServiceEntry
{
    ServiceEntry *nextServiceEntry;
    const char *serviceName;
    Actor::InProcessActorId serviceInProcessActorId;
    /**
     * @brief Get the number of Service entries for the given ServiceEntry
     * @param serviceEntry service entry to check
     * @return number of service entries for the given serviceEntry
     */
    inline static size_t getServiceEntryCount(ServiceEntry *serviceEntry) noexcept
    {
        size_t ret = 0;
        for (; serviceEntry != 0; ++ret, serviceEntry = serviceEntry->nextServiceEntry)
        {
        }
        return ret;
    }
};
/**
 * @brief Connector registration event
 */
struct ConnectorRegisterEvent : public Actor::Event
{
    Actor::ActorId::RouteId routeId;
    bool isSharedMemory;
    connection_distance_type serialConnectionDistance;
    const char *engineName;
    const char *engineSuffix;
    ServiceEntry *firstServiceEntry;
};
/**
 * @brief Connector unregistration event
 */
struct ConnectorUnregisterEvent : Actor::Event
{
    Actor::ActorId::RouteId routeId;
};
/**
 * @brief Singleton client subscription event
 */
struct SingletonClientSubscriptionEvent : Actor::Event
{
    Actor::NodeId clientNodeId;
    bool unsubscribeFlag;
};
/**
 * @brief Singleton client engine event
 */
struct SingletonClientEngineEvent : Actor::Event
{
    mutable bool undeliveredBadAllocCauseFlag;
    engine_id_type engineId;
    const char *engineName;
    const char *engineSuffix;
    RouteEntry *firstRouteEntry;
    ServiceEntry *firstServiceEntry;
};
#pragma pack(pop)
}
}

namespace tredzone
{
/**
 * @brief EngineToEngine connector class
 */
class EngineToEngineConnector
{
  public:
    class ServiceActor;
    /** @brief NotE2ECapableException is thrown when an unroutable Event is pushed to a cluster Actor */
    struct NotE2ECapableException : std::exception
    {
        virtual const char *what() const noexcept
        {
            return "tredzone::EngineToEngineConnector::NotE2ECapableException";
        }
    };
    /** @brief E2ERouteServiceMissingException is thrown when the E2ERouteService is missing from the StartSequence */
    struct E2ERouteServiceMissingException : std::exception
    {
        virtual const char *what() const noexcept
        {
            return "tredzone::EngineToEngineConnector::E2ERouteServiceMissingException";
        }
    };
    /**
     * @brief cross-machine service index entry
     */
    struct ServiceEntry
    {
        Actor::string_type serviceName;
        Actor::Actor::InProcessActorId serviceInProcessActorId;
        /** @brief Basic constructor */
        inline ServiceEntry(const char *serviceName, const Actor::InProcessActorId &serviceInProcessActorId,
                            const Actor::AllocatorBase &allocator)
            : serviceName(serviceName, allocator), serviceInProcessActorId(serviceInProcessActorId)
        {
        }
        /** @brief Basic constructor */
        inline ServiceEntry(const Actor::AllocatorBase &allocator)
            : serviceName(0, allocator), serviceInProcessActorId()
        {
        }
    };
    typedef std::vector<ServiceEntry, Actor::Allocator<ServiceEntry>> ServiceEntryVector;

    /** @brief Basic constructor */
    EngineToEngineConnector(Actor &); // throw(std::bad_alloc, E2ERouteServiceMissingException)
    /** @brief Basic destructor */
    virtual ~EngineToEngineConnector() noexcept;
    /**
     * @brief Get a comparable RouteId
     * @return Comparable RouteId
     */
    inline Actor::ActorId::RouteIdComparable getRouteIdComparable() const noexcept
    {
        return Actor::ActorId::RouteIdComparable(nodeId, nodeConnectionId);
    }

  protected:
    typedef Actor::ActorId::RouteId::NodeConnectionId NodeConnectionId;
    typedef void (*OnOutboundEventFn)(EngineToEngineConnector *, const Actor::Event &);
    typedef OnOutboundEventFn OnInboundUndeliveredEventFn;

    AsyncNode &asyncNode;

    /**
     * @brief Get the ServiceVector containing the Engine's ServiceIndex values.
     * Current Engine's Services will be contained in the result ServiceVector
     * @return ServiceVector
     */
    ServiceEntryVector getServiceVector() const;
    /**
     * @brief Register a new connection to a remote Engine
     * @note Will trigger EngineToEngineConnector::ServiceActor::Proxy::onAddEngineRoute()
     * @param peerEngineName remote connection's Engine name
     * @param peerEngineSuffix remote connection's Engine suffix
     * @param serviceEntryVector remote connection's Engine ServiceEntryVector
     * @param onOutboundEventFn function to be called on outbound Event
     * @param onInboundUndeliveredEventFn function to be called on inbound Event
     * @param isEngineToEngineSharedMemoryConnectorFlag true if connector is shared memory
     * @param serialConnectionDistance connection distance to remote connection
     */
    void registerConnection(const char *, const char *, const ServiceEntryVector &, OnOutboundEventFn,
                            OnInboundUndeliveredEventFn, bool,
                            e2econnector::connection_distance_type); // throw(std::bad_alloc)
                                                                     /**
                                                                      * @brief Unregister a connection to remote Engine
                                                                      * @note Will trigger EngineToEngineConnector::ServiceActor::Proxy::onRemoveEngineRoute()
                                                                      */
    void unregisterConnection() noexcept;
    /**
     * @brief hook for connection failure
     */
    virtual void onConnectionServiceFailure();
    /**
     * @brief Get the RouteId of this connector
     * @return
     */
    inline Actor::ActorId::RouteId getRouteId() const noexcept
    {
        return Actor::ActorId::RouteId(nodeId, nodeConnectionId, nodeConnection);
    }
    /**
     * @brief Set RouteId from ActorId
     * @param externalActorId values to be set from
     */
    inline void setRouteId(Actor::ActorId &externalActorId) const noexcept
    {
        assert(nodeConnectionId != 0);
        externalActorId.routeId.nodeId = nodeId;
        externalActorId.routeId.nodeConnectionId = nodeConnectionId;
        externalActorId.routeId.nodeConnection = nodeConnection;
    }
    /** Basic getter */
    inline NodeConnectionId getNodeConnectionId() const noexcept { return nodeConnectionId; }
    /**
     * @brief Check if given event is EngineToEngine capable
     * @param eventId event id
     * @param absoluteEventId event name
     * @param serializeFn event serialization function
     * @param deserializeFn event deserilization function
     * @return
     */
    inline static bool isEventE2ECapable(Actor::EventId eventId, const char *&absoluteEventId,
                                         Actor::Event::EventE2ESerializeFunction &serializeFn,
                                         Actor::Event::EventE2EDeserializeFunction &deserializeFn)
    {
        return Actor::Event::isE2ECapable(eventId, absoluteEventId, serializeFn, deserializeFn);
    }
    /**
     * @brief Check if given event is EngineToEngine capable
     * @param absoluteEventId event name
     * @param eventId event id
     * @param serializeFn event serialization function
     * @param deserializeFn event deserilization function
     * @return
     */
    inline static bool isEventE2ECapable(const char *absoluteEventId, Actor::EventId &eventId,
                                         Actor::Event::EventE2ESerializeFunction &serializeFn,
                                         Actor::Event::EventE2EDeserializeFunction &deserializeFn)
    {
        return Actor::Event::isE2ECapable(absoluteEventId, eventId, serializeFn, deserializeFn);
    }
    /**
     * @brief Get EventId from Event name
     * @param absoluteEventId event name
     * @return event id
     */
    inline static std::pair<bool, Actor::EventId> findEventId(const char *absoluteEventId) noexcept
    {
        return Actor::Event::findEventId(absoluteEventId);
    }
    /** @brief Basic getter */
    inline const Actor::AllocatorBase &getAllocator() const noexcept { return allocator; }

  private:
    struct InnerActor : Actor, Actor::Callback
    {
        EngineToEngineConnector *connector;
        ActorId::RouteId connectionRouteId;
        InnerActor(EngineToEngineConnector *);
        void onUndeliveredEvent(const e2econnector::ConnectorRegisterEvent &);
        void onCallback() noexcept;
        void registerConnectionService(const Actor::ActorId::RouteId &, const char *, const char *, bool,
                                       e2econnector::connection_distance_type,
                                       const ServiceEntryVector &); // throw(std::bad_alloc)
        void unregisterConnectionService() noexcept;
    };

    class Singleton;
    const Actor::NodeId nodeId;
    NodeConnectionId nodeConnectionId;
    Actor::NodeConnection *nodeConnection;
    Actor::string_type peerEngineName;
    Actor::string_type peerEngineSuffix;
    Actor::ActorReference<Singleton> singleton;
    Actor::ActorReference<InnerActor> innerActor;
    Actor::AllocatorBase allocator;
};

/**
 * @brief EngineToEngine serial connector class
 * Network is little endian encoded.
 * Specialization through Connection class must implement (possibly inlined):
 * void onWriteSerialEvent(const Actor::Event&)
 * void onWriteSerialUndeliveredEvent(const Actor::Event&)
 */
class EngineToEngineSerialConnector : protected EngineToEngineConnector
{
  public:
    /**
     * @brief Serial Exception
     */
    struct SerialException : std::exception
    {
        enum ReasonEnum
        {
            OutOfRangeEventIdReason, /**< EventId is out of range */
            UndefinedEventIdReason,  /**< Undefined EventId */
            InvalidEventTypeReason   /**< Invalid Event type */
        };
        const ReasonEnum reason;

        /** @brief Basic constructor */
        inline SerialException(ReasonEnum reason) noexcept : reason(reason) {}
        virtual const char *what() const noexcept
        {
            switch (reason)
            {
            case OutOfRangeEventIdReason:
                return "tredzone::EngineToEngineSerialConnector::SerialException [OutOfRangeEventId]";
            case UndefinedEventIdReason:
                return "tredzone::EngineToEngineSerialConnector::SerialException [UndefinedEventId]";
            case InvalidEventTypeReason:
                return "tredzone::EngineToEngineSerialConnector::SerialException [InvalidEventType]";
            default:
                return "tredzone::EngineToEngineSerialConnector::SerialException [unknown type]"; // unknown serial
                                                                                                       // exception
            }
        }
    };

#pragma pack(push)
#pragma pack(1)
    /**
     * @brief In process serial ActorId
     */
    class SerialInProcessActorId
    {
      public:
        /** @brief Constructor */
        inline SerialInProcessActorId(const Actor::InProcessActorId &inProcessActorId) noexcept :
#ifdef TREDZONE_LITTLE_ENDIAN
            nodeId(inProcessActorId.nodeId),
            nodeActorId(inProcessActorId.nodeActorId),
            eventTable(serializePointer(inProcessActorId.eventTable))
#else
            nodeId(nativeToLittleEndian(inProcessActorId.nodeId)),
            nodeActorId(nativeToLittleEndian(inProcessActorId.nodeActorId)),
            eventTable(nativeToLittleEndian(serializePointer(inProcessActorId.eventTable)))
#endif
        {
        }
        /** @brief Get NodeId */
        inline Actor::NodeId getNodeId() const noexcept
        {
#ifdef TREDZONE_LITTLE_ENDIAN
            return nodeId;
#else
            return littleToNativeEndian(nodeId);
#endif
        }
        inline operator Actor::InProcessActorId() const noexcept
        {
#ifdef TREDZONE_LITTLE_ENDIAN
            return Actor::InProcessActorId(nodeId, nodeActorId,
                                                static_cast<Actor::EventTable *>(deserializePointer(eventTable)));
#else
            return Actor::InProcessActorId(
                littleToNativeEndian(nodeId), littleToNativeEndian(nodeActorId),
                static_cast<Actor::EventTable *>(deserializePointer(littleToNativeEndian(eventTable))));
#endif
        }

      private:
        Actor::NodeId nodeId;
        Actor::NodeActorId nodeActorId;
        uint64_t eventTable;
    };
#pragma pack(pop)

    typedef e2econnector::connection_distance_type connection_distance_type;

    /** @brief Constructor */
    EngineToEngineSerialConnector(Actor &, size_t serialBufferSize = 4096 - sizeof(void *)); // hardcoded
    /** @brief Destructor */
    virtual ~EngineToEngineSerialConnector() noexcept;

    inline static uint64_t serializePointer(const void *) noexcept;
    inline static void *deserializePointer(uint64_t) noexcept;

    // (not implemented)
    inline static int8_t nativeToLittleEndian(int8_t) noexcept;
    inline static uint8_t nativeToLittleEndian(uint8_t) noexcept;
    inline static int16_t nativeToLittleEndian(int16_t) noexcept;
    inline static uint16_t nativeToLittleEndian(uint16_t) noexcept;
    inline static int32_t nativeToLittleEndian(int32_t) noexcept;
    inline static uint32_t nativeToLittleEndian(uint32_t) noexcept;
    inline static int64_t nativeToLittleEndian(int64_t) noexcept;
    inline static uint64_t nativeToLittleEndian(uint64_t) noexcept;

    // (not implemented)
    inline static int8_t littleToNativeEndian(int8_t) noexcept;
    inline static uint8_t littleToNativeEndian(uint8_t) noexcept;
    inline static int16_t littleToNativeEndian(int16_t) noexcept;
    inline static uint16_t littleToNativeEndian(uint16_t) noexcept;
    inline static int32_t littleToNativeEndian(int32_t) noexcept;
    inline static uint32_t littleToNativeEndian(uint32_t) noexcept;
    inline static int64_t littleToNativeEndian(int64_t) noexcept;
    inline static uint64_t littleToNativeEndian(uint64_t) noexcept;

    /**
     * @brief Set RouteId to externalActorId
     * This is used when an ActorId is transfered in an Event to a peer Engine.
     * This method must be called in the destination Engine or the ActorId will be unroutable.
     * @param externalActorId
     * @return ActorId
     */
    inline Actor::ActorId setRouteId(const SerialInProcessActorId &externalActorId) const noexcept
    {
        Actor::ActorId ret(externalActorId);
        EngineToEngineConnector::setRouteId(ret);
        return ret;
    }
    /**
     * @see setRouteId(const SerialInProcessActorId& externalActorId)
     * @param externalActorId
     */
    inline void setRouteId(Actor::ActorId &externalActorId) const noexcept
    {
        return EngineToEngineConnector::setRouteId(externalActorId);
    }

  private:
    struct SerialRouteEventHeader;

  protected:
    /**
     * @brief Register a new connection to remote peer
     * @param peerEngineName peer engine name
     * @param peerEngineSuffix peer engine suffix
     * @param serviceEntryVector peer service entry vector
     * @param distance distance to peer
     * @param connector used to get to peer
     */
    template <class _Connector>
    inline static void registerConnection(const char *peerEngineName, const char *peerEngineSuffix,
                                          const ServiceEntryVector &serviceEntryVector,
                                          connection_distance_type distance, _Connector &connector)
    { // throw(std::bad_alloc)
        connector.EngineToEngineConnector::registerConnection(
            peerEngineName, peerEngineSuffix, serviceEntryVector, &StaticOnEvent<_Connector>::onWriteSerialEvent,
            &StaticOnEvent<_Connector>::onWriteSerialUndeliveredEvent, false, distance);
        for (int i = 0; i < Actor::MAX_EVENT_ID_COUNT; ++i)
        {
            connector.eventIdSerializeFnArray[i] = 0;
            connector.eventIdDeserializeArray[i] = tredzone::null;
        }
        connector.distance = distance;
    }
    /**
     * @brief Unregister a peer connection
     * This also clears read and write biffers
     */
    void unregisterConnection() noexcept
    {
        EngineToEngineConnector::unregisterConnection();
        readSerialBuffer.clear();
        writeSerialBuffer.clear();
    }
    /** @brief Get serial read buffer */
    inline Actor::Event::SerialBuffer &getReadSerialBuffer() noexcept { return readSerialBuffer; }
    /** @brief Get serial write buffer */
    inline Actor::Event::SerialBuffer &getWriteSerialBuffer() noexcept { return writeSerialBuffer; }
    /** @brief Get connection distance */
    inline connection_distance_type getDistance() const noexcept { return distance; }
    /**
     * @brief Read readSerialBuffer for SerialEvent before calling Event's deserializeFunction
     * @return true if readSerialBuffer still contains an Event
     * @throws std::bad_alloc
     * @throws SerialException is thrown if EventType is invalid
     */
    inline bool readSerial();
    /**
     * @brief Write a SerialEvent into the writeSerialBuffer
     * @param event to write into serial buffer
     * @throws std::bad_alloc
     * @throws NotE2ECapableException is thrown if event is not EngineToEngine capable. ei: needed methods are not
     * implemented by event
     */
    inline void writeSerialEvent(const Actor::Event &event) { writeSerialEvent2<SerialRouteEventHeader>(event); }
    /**
     * @brief Write a SerialUndeliveredEvent into the writeSerialBuffer
     * @param undelivered event to write into serial buffer
     * @throws std::bad_alloc
     * @throws NotE2ECapableException is thrown if event is not EngineToEngine capable. ei: needed methods are not
     * implemented by event
     */
    inline void writeSerialUndeliveredEvent(const Actor::Event &event)
    { // throw(std::bad_alloc, NotE2ECapableException)
        writeSerialEvent2<SerialReturnEventHeader>(event);
    }

  private:
    friend class EngineToEngineConnectorEventFactory;
    
public:

    enum EVENT_T : uint8_t
    {
        RouteEventType          = 0,
        ReturnEventType         = 1,
        AbsoluteIdEventType     = 2,
        HandshakeEventType      = 3,
        InvalidEventType        = 4,
    };
    
private:

#pragma pack(push)
#pragma pack(1)
    // AbsoluteEvent
    class SerialAbsoluteEventIdHeader
    {
      public:
        inline SerialAbsoluteEventIdHeader(Actor::EventId eventId, uint32_t absoluteEventIdSize) noexcept :
#ifdef TREDZONE_LITTLE_ENDIAN
            eventType((uint8_t)AbsoluteIdEventType),
            eventId(eventId),
            absoluteEventIdSize(absoluteEventIdSize)
#else
            eventType((uint8_t)AbsoluteIdEventType),
            eventId(nativeToLittleEndian(eventId)),
            absoluteEventIdSize(nativeToLittleEndian(absoluteEventIdSize))
#endif
        {
        }
        inline EVENT_T getEventType() const noexcept
        {
            assert(eventType == AbsoluteIdEventType);
            return (EVENT_T)eventType;
        }
        inline Actor::EventId getEventId() const noexcept
        {
#ifdef TREDZONE_LITTLE_ENDIAN
            return eventId;
#else
            return littleToNativeEndian(eventId);
#endif
        }
        inline uint32_t getAbsoluteEventIdSize() const noexcept
        {
#ifdef TREDZONE_LITTLE_ENDIAN
            return absoluteEventIdSize;
#else
            return littleToNativeEndian(absoluteEventIdSize);
#endif
        }

      private:
        uint8_t eventType; // EVENT_T
        Actor::EventId eventId;
        uint32_t absoluteEventIdSize;
    };
    class SerialEventHeader
    {
      public:
        inline Actor::EventId getEventId() const noexcept
        {
#ifdef TREDZONE_LITTLE_ENDIAN
            return eventId;
#else
            return littleToNativeEndian(eventId);
#endif
        }
        inline const SerialInProcessActorId &getSourceInProcessActorId() const noexcept
        {
            return sourceInProcessActorId;
        }
        inline const SerialInProcessActorId &getDestinationInProcessActorId() const noexcept
        {
            return destinationInProcessActorId;
        }
        inline uint32_t getEventSize() const noexcept
        {
#ifdef TREDZONE_LITTLE_ENDIAN
            return eventSize;
#else
            return littleToNativeEndian(eventSize);
#endif
        }
        inline SerialEventHeader swapSourceDestination() const noexcept
        {
            return SerialEventHeader((EVENT_T)eventType, eventId, destinationInProcessActorId,
                                     sourceInProcessActorId, eventSize);
        }

      protected:
        uint8_t eventType; // EVENT_T

        // CTOR
        inline SerialEventHeader(EVENT_T eventType, Actor::EventId eventId,
                                 const Actor::InProcessActorId &sourceInProcessActorId,
                                 const Actor::InProcessActorId &destinationInProcessActorId,
                                 uint32_t eventSize) noexcept :
#ifdef TREDZONE_LITTLE_ENDIAN
            eventType((uint8_t)eventType),
            eventId(eventId),
            sourceInProcessActorId(sourceInProcessActorId),
            destinationInProcessActorId(destinationInProcessActorId),
            eventSize(eventSize)
#else
            eventType((uint8_t)eventType),
            eventId(nativeToLittleEndian(eventId)),
            sourceInProcessActorId(sourceInProcessActorId),
            destinationInProcessActorId(destinationInProcessActorId),
            eventSize(nativeToLittleEndian(eventSize))
#endif
        {
            assert(eventType == RouteEventType || eventType == ReturnEventType);
        }

      private:
        Actor::EventId eventId;
        SerialInProcessActorId sourceInProcessActorId;
        SerialInProcessActorId destinationInProcessActorId;
        uint32_t eventSize;
    };
    
    // RouteEvent
    struct SerialRouteEventHeader : SerialEventHeader
    {
        inline SerialRouteEventHeader(Actor::EventId eventId,
                                      const Actor::InProcessActorId &sourceInProcessActorId,
                                      const Actor::InProcessActorId &destinationInProcessActorId,
                                      uint32_t eventSize) noexcept
            : SerialEventHeader(RouteEventType, eventId, sourceInProcessActorId, destinationInProcessActorId, eventSize)
        {
        }
        inline EVENT_T getEventType() const noexcept
        {
            assert(eventType == RouteEventType);
            return (EVENT_T)eventType;
        }
    };
    
    // ReturnEvent
    struct SerialReturnEventHeader : SerialEventHeader
    {
        inline SerialReturnEventHeader(Actor::EventId eventId,
                                       const Actor::InProcessActorId &sourceInProcessActorId,
                                       const Actor::InProcessActorId &destinationInProcessActorId,
                                       uint32_t eventSize) noexcept
            : SerialEventHeader(ReturnEventType, eventId, sourceInProcessActorId, destinationInProcessActorId,
                                eventSize)
        {
        }
        inline EVENT_T getEventType() const noexcept
        {
            assert(eventType == ReturnEventType);
            return (EVENT_T)eventType;
        }
    };
#pragma pack(pop)
    template <class _Connector> struct StaticOnEvent
    {
        inline static void onWriteSerialEvent(EngineToEngineConnector *connector, const Actor::Event &event)
        {
            static_cast<_Connector *>(connector)->onWriteSerialEvent(event);
        }
        inline static void onWriteSerialUndeliveredEvent(EngineToEngineConnector *connector,
                                                         const Actor::Event &event)
        {
            static_cast<_Connector *>(connector)->onWriteSerialUndeliveredEvent(event);
        }
    };
    struct EventE2EDeserialize
    {
        Actor::EventId localEventId;
        Actor::Event::EventE2EDeserializeFunction deserializeFunction;
        inline void operator=(const Null &) noexcept
        {
            localEventId = Actor::MAX_EVENT_ID_COUNT;
            deserializeFunction = 0;
        }
        inline bool operator==(const Null &) const noexcept
        {
            assert((localEventId == Actor::MAX_EVENT_ID_COUNT && deserializeFunction == 0) ||
                   (localEventId > 0 && localEventId < Actor::MAX_EVENT_ID_COUNT && deserializeFunction != 0));
            return localEventId == Actor::MAX_EVENT_ID_COUNT;
        }
        inline bool operator!=(const Null &) const noexcept { return !(*this == tredzone::null); }
    };

    Actor::Event::EventE2ESerializeFunction *eventIdSerializeFnArray;
    EventE2EDeserialize *eventIdDeserializeArray;
    Actor::Event::SerialBuffer readSerialBuffer;
    Actor::Event::SerialBuffer writeSerialBuffer;
    char *serialBufferCopy;
    static const int serialBufferCopySize = 65536;
    connection_distance_type distance;

    bool readSerialRouteEvent(); // throw(std::bad_alloc)
    bool readSerialReturnEvent();
    bool readSerialAbsoluteIdEvent();                                                  // throw(std::bad_alloc)
    inline void readSerialEvent(const SerialRouteEventHeader &, const void *, size_t); // throw(SerialException)
    inline void readSerialUndeliveredEvent(const SerialReturnEventHeader &, const void *,
                                           size_t);                                    // throw(SerialException)
    void readSerialAbsoluteIdEvent(const SerialAbsoluteEventIdHeader &, const char *); // throw(SerialException)
    void writeSerialAbsoluteEventId(Actor::EventId, const char *);                // throw(std::bad_alloc)
    template <class _Header>
    inline void writeSerialEvent2(const Actor::Event &); // throw(std::bad_alloc, NotE2ECapableException)
};

/**
 * @brief Factory method used in the Event's deserialization function to deliver recieved and deserialized event
 */
class EngineToEngineConnectorEventFactory
{
  public:
    /**	@brief Basic destructor */
    inline ~EngineToEngineConnectorEventFactory() noexcept
    {
        assert(destinationEventChain != 0);
        assert(event != 0);
        destinationEventChain->push_back(event);
    }
    /** @brief Get allocator */
    Actor::Event::AllocatorBase getAllocator() noexcept;

    /**
     * @brief Allocate n * sizeof(T) bytes using the Tredzone allocator
     * @param n * sizeof(T) to be allocated
     * @return Allocated buffer
     * @throws std::bad_alloc
     */
    template <class T> inline T *allocate(size_t n) { return static_cast<T *>(allocate(n * sizeof(T))); }
    /**
     * @brief Create a new event that will inserted into Engine
     * @return Event reference
     * @throws std::bad_alloc
     */
    template <class _Event> inline _Event &newEvent() { return newEvent<_Event>(NewEventVoidParameter()); }
    /**
     * @brief Create a new event that will inserted into Engine
     * @param eventInit Event parameter
     * @return Event reference
     * @throws std::bad_alloc
     */
    template <class _Event, class _EventInit> inline _Event &newEvent(const _EventInit &eventInit)
    {
        assert(destinationEventChain == 0);
        assert(event == 0);
        _Event *ret = new (allocateEvent(sizeof(EventWrapper<_Event, _EventInit>), destinationEventChain))
            EventWrapper<_Event, _EventInit>(serialEventHeader, eventInit);
        assert(ret->getClassId() == debugLocalEventId);
        event = ret;
        return *ret;
    }
    /**
     * @brief Set serial connector's route id from given ActorId
     * @param externalActorId external ActorId
     */
    inline void setRouteId(Actor::ActorId &externalActorId) const noexcept
    {
        serialConnector.setRouteId(externalActorId);
    }
    /**
     * @brief Set serial connector's route id from given ActorId
     * @param externalInProcessActorId in process ActorId
     * @return ActorId
     */
    inline Actor::ActorId setRouteId(const Actor::InProcessActorId &externalInProcessActorId) const noexcept
    {
        Actor::ActorId ret(externalInProcessActorId);
        setRouteId(ret);
        return ret;
    }

  private:
    friend class EngineToEngineSerialConnector;

    struct NewEventVoidParameter
    {
    };

    template <class _Event, class _EventInit> struct EventWrapper : virtual private Actor::EventBase, _Event
    {
        inline EventWrapper(const EngineToEngineSerialConnector::SerialEventHeader &serialEventHeader,
                            const _EventInit &eventInit)
            : Actor::EventBase(Actor::Event::getClassId<_Event>(),
                                    serialEventHeader.getSourceInProcessActorId(),
                                    serialEventHeader.getDestinationInProcessActorId(),
                                    (Actor::Event::route_offset_type)(
                                        ((uintptr_t) static_cast<Actor::Event *>((EventWrapper *)0) +
                                         sizeof(Actor::ActorId::RouteId))
                                        << 1)),
              _Event(eventInit)
        {
            assert(
                ((uintptr_t) static_cast<Actor::Event *>((EventWrapper *)0) + sizeof(Actor::ActorId::RouteId))
                    << 1 <=
                std::numeric_limits<Actor::Event::route_offset_type>::max());
        }
    };
    template <class _Event>
    struct EventWrapper<_Event, NewEventVoidParameter> : virtual private Actor::EventBase, _Event
    {
        inline EventWrapper(const EngineToEngineSerialConnector::SerialEventHeader &serialEventHeader,
                            const NewEventVoidParameter &)
            : Actor::EventBase(Actor::Event::getClassId<_Event>(),
                                    serialEventHeader.getSourceInProcessActorId(),
                                    serialEventHeader.getDestinationInProcessActorId(),
                                    (Actor::Event::route_offset_type)(
                                        ((uintptr_t) static_cast<Actor::Event *>((EventWrapper *)0) +
                                         sizeof(Actor::ActorId::RouteId))
                                        << 1))
        {
            assert(
                ((uintptr_t) static_cast<Actor::Event *>((EventWrapper *)0) + sizeof(Actor::ActorId::RouteId))
                    << 1 <=
                std::numeric_limits<Actor::Event::route_offset_type>::max());
        }
    };

    const EngineToEngineSerialConnector &serialConnector;
    const EngineToEngineSerialConnector::SerialEventHeader &serialEventHeader;
    Actor::Event::Chain *destinationEventChain;
    Actor::Event *event;
    Actor::Event::AllocatorBase::Factory eventAllocatorFactory;
#ifndef NDEBUG
    Actor::EventId debugLocalEventId;
#endif

#ifndef NDEBUG
    inline EngineToEngineConnectorEventFactory(
        const EngineToEngineSerialConnector &serialConnector,
        const EngineToEngineSerialConnector::SerialEventHeader &serialEventHeader,
        Actor::EventId debugLocalEventId) noexcept : serialConnector(serialConnector),
                                                          serialEventHeader(serialEventHeader),
                                                          destinationEventChain(0),
                                                          event(0),
                                                          debugLocalEventId(debugLocalEventId)
    {
    }
#else
    inline EngineToEngineConnectorEventFactory(
        const EngineToEngineSerialConnector &serialConnector,
        const EngineToEngineSerialConnector::SerialEventHeader &serialEventHeader) noexcept
        : serialConnector(serialConnector),
          serialEventHeader(serialEventHeader)
    {
    }
#endif
    void *allocate(size_t);
    void *allocateEvent(size_t, Actor::Event::Chain *&);
    inline void toBeUndeliveredRoutedEvent() noexcept;
};

/**
 * @brief EngineToEngine shared memory connector class
 * @note: available in upcoming version
 */
class EngineToEngineSharedMemoryConnector : protected EngineToEngineConnector
{
  public:
    class SharedMemory;
    /**
     * @brief NotConnectedSharedMemoryException
     */
    struct NotConnectedSharedMemoryException : std::exception
    {
        virtual const char *what() const noexcept
        {
            return "tredzone::EngineToEngineSharedMemoryConnector::NotConnectedSharedMemoryException";
        }
    };

    /** @brief Constructor */
    inline EngineToEngineSharedMemoryConnector(Actor &actor)
        : EngineToEngineConnector(actor), actor(actor), sharedMemory(0)
    {
    }

  protected:
    void registerConnection(const char *peerEngineName, const char *peerEngineSuffix,
                            const ServiceEntryVector &serviceEntryVector,
                            SharedMemory &); // throw(std::bad_alloc, NotConnectedSharedMemoryException)
    void unregisterConnection() noexcept;
    virtual void onDisconnected() noexcept = 0;

  private:
    friend class Actor;
    struct PreBarrierCallback : Actor::Callback
    {
        inline void onCallback() noexcept;
    };
    struct PostBarrierCallback : Actor::Callback
    {
        inline void onCallback() noexcept;
    };

    Actor &actor;
    SharedMemory *sharedMemory;
    PreBarrierCallback preBarrierCallback;
    PostBarrierCallback postBarrierCallback;

    static void onWriteSerialEvent(EngineToEngineConnector *connector, const Actor::Event &event);
    static void onWriteSerialUndeliveredEvent(EngineToEngineConnector *connector, const Actor::Event &event);
    inline void *allocateEvent(size_t sz); // throw(std::bad_alloc)
};

/**
 * @brief SharedMemory class that is used with EngineToEngineSharedMemoryConnector
 * @note: available in upcoming version
 */
class EngineToEngineSharedMemoryConnector::SharedMemory
{
  public:
    struct UndersizedSharedMemoryException : std::exception
    {
        virtual const char *what() const noexcept
        {
            return "tredzone::EngineToEngineSharedMemoryConnector::SharedMemory::UndersizedSharedMemoryException";
        }
    };

    SharedMemory(const Actor &, void *, size_t); // throw(UndersizedSharedMemoryException)
    ~SharedMemory() noexcept;
    bool tryConnect() noexcept; // no more than one attempt by actor callback (as it may require a memory barrier
                                // between 2 subsequent calls)
    bool isConnected() noexcept;

  private:
    friend class Actor;
    friend class EngineToEngineSharedMemoryConnector;
    struct ConnectionControl;
    struct ReadWriteControl
    {
        typedef uintptr_t offset_type;
        struct Chain
        {
            struct Entry
            {
                offset_type next;
            };
            offset_type head;
            offset_type tail;
            inline Chain() noexcept : head(0), tail(0) {}
            inline void push_back(ConnectionControl &, Entry &) noexcept;
            inline void push_front(ConnectionControl &, Entry &) noexcept;
            inline void push_front(ConnectionControl &, Chain &) noexcept;
            inline Entry &pop_front(ConnectionControl &) noexcept;
            inline void clear() noexcept;
        };
        struct Concurrent
        {
            sig_atomic_t count;
        };

        Concurrent write; // producer is the writer, consumer is the reader
        char cacheLinePadding1[TREDZONE_CACHE_LINE_PADDING(sizeof(Concurrent))];
        Concurrent read; // producer is the reader, consumer is the writer
        char cacheLinePadding2[TREDZONE_CACHE_LINE_PADDING(sizeof(Concurrent))];
        struct Shared
        { // mutual exclusive access by reader and writer protected using Concurrent operations and a barrier
            Chain toBeDeliveredChain;
            Chain toBeUndeliveredChain;
            Chain inUsePageChain;
        } shared;
        char cacheLinePadding3[TREDZONE_CACHE_LINE_PADDING(sizeof(Shared))];

        inline static offset_type toOffset(ConnectionControl &, void *) noexcept;
        inline static void *fromOffset(ConnectionControl &, offset_type) noexcept;
    };
    struct ConnectionControl
    {
        enum ConnectionStatusEnum
        {
            DisconnectedStatus = 0,
            ConnectingStatus,
            ConnectedStatus,
            UndefinedStatus
        };
        struct Concurrent
        {
            sig_atomic_t connectionStatusEnum;            // requires a barrier to reflect change
            sig_atomic_t firstReadWriteControlInUseFlag;  // retained (CAS 0->1)/released (atomic -1) by writer and upon
                                                          // ConnectedStatus locked (CAS 1->2)/unlocked (atomic -1) by
                                                          // reader
            sig_atomic_t secondReadWriteControlInUseFlag; // retained (CAS 0->1)/released (atomic -1) by writer and upon
                                                          // ConnectedStatus locked (CAS 1->2)/unlocked (atomic -1) by
                                                          // reader
        };

        Concurrent concurrent; // CAS & decrement atomic operations
        char cacheLinePadding[TREDZONE_CACHE_LINE_PADDING(sizeof(Concurrent))];

        bool retainControl(ReadWriteControl *&, ReadWriteControl *&) noexcept;
        void releaseControl(ReadWriteControl *, bool) noexcept;
    };

    ConnectionControl *connectionControl;
    ReadWriteControl *readControl;
    ReadWriteControl *writeControl;
    const size_t eventPageSize;
    size_t eventPagesMemorySize;
    ConnectionControl::ConnectionStatusEnum lastConnectionStatus;
    sig_atomic_t
        nextConcurrentReadCount; // writeControl->read.count (+1 change triggers next write from this engines's end)
    sig_atomic_t
        nextConcurrentWriteCount; // readControl->write.count (+1 change triggers next read from this engines's end)
    void *currentEventPagePtr;
    size_t currentEventPageOffset;
    ReadWriteControl::Chain toBeDeliveredEventChain;
    ReadWriteControl::Chain availableEventPageChain;
    ReadWriteControl::Chain inUseEventPageChain;

    inline void *allocateEvent(size_t)
    { // throw(std::bad_alloc)
        breakThrow(std::bad_alloc());
        return nullptr; // (silence compiler warning)
    }
};

/**
 * @brief EngineToEngineConnector ServiceActor
 */
class EngineToEngineConnector::ServiceActor : public Actor
{
  public:
    class Proxy;

    /** @brief Constructor */
    ServiceActor();
    /** @brief Destructor */
    virtual ~ServiceActor() noexcept;

  private:
    typedef e2econnector::engine_id_type engine_id_type;
    struct EventHandler;
    struct RouteEntry
    {
        ActorId::RouteId routeId;
        bool isSharedMemory;
        e2econnector::connection_distance_type serialConnectionDistance;
        inline RouteEntry(const ActorId::RouteId &routeId, bool isSharedMemory,
                          e2econnector::connection_distance_type serialConnectionDistance) noexcept
            : routeId(routeId),
              isSharedMemory(isSharedMemory),
              serialConnectionDistance(serialConnectionDistance)
        {
        }
    };
    struct EngineEntry : MultiDoubleChainLink<EngineEntry>
    {
        static const int ROUTE_ENTRY_VECTOR_RESERVE = 16;
        typedef std::bitset<MAX_NODE_COUNT> SubscribedNodeBitSet;
        typedef std::vector<RouteEntry, Allocator<RouteEntry>> RouteEntryVector;
        const engine_id_type engineId;
        const string_type engineName;
        const string_type engineSuffix;
        ServiceEntryVector serviceEntryVector;
        RouteEntryVector routeEntryVector;
        SubscribedNodeBitSet subscribedNodeBitSet;
        inline EngineEntry(engine_id_type engineId, const char *engineName, const char *engineSuffix,
                           const AllocatorBase &allocator)
            : engineId(engineId), engineName(engineName, allocator), engineSuffix(engineSuffix, allocator),
              serviceEntryVector(allocator), routeEntryVector(allocator)
        {
        }
        inline RouteEntryVector::iterator findRouteEntry(const ActorId::RouteId &routeId) noexcept
        {
            RouteEntryVector::iterator i = routeEntryVector.begin();
            for (RouteEntryVector::iterator endi = routeEntryVector.end(); i != endi && i->routeId != routeId; ++i)
            {
            }
            return i;
        }
    };
    struct SubscriberNodeEntry
    {
        NodeId nodeId;
        Event::Pipe pipe;
        inline SubscriberNodeEntry(Actor &actor) noexcept : nodeId(MAX_NODE_COUNT), pipe(actor) {}
    };
    struct EventHandler : Actor::Callback
    {
        typedef EngineEntry::DoubleChain<> EngineEntryChain;
        typedef std::list<EngineEntry, Allocator<EngineEntry>> EngineEntryList;
        typedef std::vector<SubscriberNodeEntry, Allocator<SubscriberNodeEntry>> SubscriberNodeEntryVector;
        ServiceActor &serviceActor;
        engine_id_type lastEngineId;
        EngineEntryList engineEntryList;
        EngineEntryChain subscribersNotificationEngineEntryChain;
        SubscriberNodeEntryVector subscriberNodeEntryVector;

        EventHandler(ServiceActor &);
        void onEvent(const e2econnector::ConnectorRegisterEvent &);
        void onEvent(const e2econnector::ConnectorUnregisterEvent &);
        void onEvent(const e2econnector::SingletonClientSubscriptionEvent &);
        void onUndeliveredEvent(const e2econnector::SingletonClientEngineEvent &);
        void onCallback() noexcept;
        inline EngineEntryList::iterator findEngineEntry(const char *engineName, const char *engineSuffix) noexcept
        {
            EngineEntryList::iterator i = engineEntryList.begin();
            for (EngineEntryList::iterator endi = engineEntryList.end();
                 i != endi && (i->engineName != engineName || i->engineSuffix != engineSuffix); ++i)
            {
            }
            return i;
        }
        inline void notifySubscribers(EngineEntry &engineEntry) noexcept
        {
            bool wasSubscribedNodeBitSetNone = engineEntry.subscribedNodeBitSet.none();
            for (SubscriberNodeEntryVector::iterator i = subscriberNodeEntryVector.begin(),
                                                     endi = subscriberNodeEntryVector.end();
                 i != endi && i->nodeId < MAX_NODE_COUNT; ++i)
            {
                assert(i->nodeId < engineEntry.subscribedNodeBitSet.size());
                engineEntry.subscribedNodeBitSet.set(i->nodeId, true);
            }
            if (wasSubscribedNodeBitSetNone && engineEntry.subscribedNodeBitSet.any())
            {
                subscribersNotificationEngineEntryChain.push_back(&engineEntry);
                if (!isRegistered())
                {
                    serviceActor.registerCallback(*this);
                }
            }
        }
        inline NodeId singletonClientNodeId(const ActorId &) const noexcept;
        inline void singletonClientUnsubscribe(NodeId) noexcept;
    };

    EventHandler eventHandler;
};

/**
 * @brief EngineToEngineConnector ServiceActor proxy
 */
class EngineToEngineConnector::ServiceActor::Proxy
{
  private:
    struct SingletonInnerActor;

  public:
    typedef e2econnector::engine_id_type engine_id_type; /**< Tredzone engine id type */
    typedef e2econnector::RouteEntryBase RouteInfo;      /**< Tredzone Route information */

    /**
     * @brief ServiceInfo class
     */
    class ServiceInfo
    {
      public:
        Actor::string_type serviceName;
        /** @brief Constructor */
        inline ServiceInfo(const char *serviceName, const Actor::InProcessActorId &serviceInProcessActorId,
                           const Actor::AllocatorBase &allocator)
            : serviceName(serviceName, allocator), serviceInProcessActorId(serviceInProcessActorId)
        {
        }

      private:
        friend struct SingletonInnerActor;
        Actor::InProcessActorId serviceInProcessActorId;
    };
    typedef std::vector<ServiceInfo, Actor::Allocator<ServiceInfo>> ServiceInfoVector; /**< Service info vector */
    typedef std::list<RouteInfo, Actor::Allocator<RouteInfo>> RouteInfoList;           /**< Route info vector */

    /**
     * @brief EngineInfo class
     */
    struct EngineInfo
    {
        Actor::string_type engineName;
        Actor::string_type engineSuffix;
        ServiceInfoVector serviceInfoVector;
        RouteInfoList routeInfoList;
        inline EngineInfo(const Actor::AllocatorBase &allocator)
            : engineName(allocator), engineSuffix(allocator), serviceInfoVector(allocator), routeInfoList(allocator)
        {
        }
    };
    typedef std::map<engine_id_type, EngineInfo, std::less<engine_id_type>,
                     Actor::Allocator<std::pair<const engine_id_type, EngineInfo>>>
        EngineInfoByIdMap;

    /** @brief Constructor */
    Proxy(Actor &);
    /** @brief Destructor */
    virtual ~Proxy() noexcept;

    /**
     * @brief Get service ActorId for given engineId and routeId
     * A service can be available in multiple engines and through multiple routes
     * @param engineId Engine id
     * @param routeId Route id
     * @return ActorId
     */
    template <class _Service>
    inline Actor::ActorId getServiceActorId(engine_id_type engineId,
                                                 const Actor::ActorId::RouteId &routeId) const noexcept
    {
        return singletonInnerActor->getServiceActorId<_Service>(engineId, routeId);
    }
    /**
     * @brief Get engineInfo map (indexed by engine id)
     * @return engineInfo map
     */
    inline const EngineInfoByIdMap &getEngineInfoByIdMap() const noexcept
    {
        return singletonInnerActor->engineInfoByIdMap;
    }

  protected:
    /**
     * @brief Callback method that is triggered after engineInfoByIdMap is updated
     * @param engine id
     * @param route info list
     */
    virtual void onAddEngineRoute(engine_id_type, const RouteInfoList &) noexcept {}
    /**
     * @brief Callback method that is triggered before engineInfoByIdMap is updated
     * @param engine id
     * @param route info list
     */
    virtual void onRemoveEngineRoute(engine_id_type, const RouteInfoList &) noexcept {}

  private:
    struct ProxyWeakReference
    {
        Proxy *proxy;
        inline ProxyWeakReference(Proxy *proxy) noexcept : proxy(proxy) {}
    };
    struct SingletonInnerActor : Actor
    {
        typedef std::list<ProxyWeakReference, Allocator<ProxyWeakReference>> ProxyWeakReferenceList;
        ProxyWeakReferenceList proxyWeakReferenceList;
        EngineInfoByIdMap engineInfoByIdMap;
        SingletonInnerActor();
        virtual ~SingletonInnerActor() noexcept;
        void onEvent(const e2econnector::SingletonClientEngineEvent &);
        inline void notifyAddEngineRoute(engine_id_type, const RouteInfoList &) noexcept;
        inline void notifyRemoveEngineRoute(engine_id_type, const RouteInfoList &) noexcept;
        inline static RouteInfoList::iterator findRouteInfo(RouteInfoList &,
                                                            const Actor::ActorId::RouteId &) noexcept;
        inline static e2econnector::RouteEntry *findRouteEntry(e2econnector::RouteEntry *,
                                                               const Actor::ActorId::RouteId &) noexcept;
        template <class _Service>
        inline ActorId getServiceActorId(engine_id_type engineId, const Actor::ActorId::RouteId &routeId) const
            noexcept
        {
            EngineInfoByIdMap::const_iterator iengineInfo = engineInfoByIdMap.find(engineId);
            if (iengineInfo != engineInfoByIdMap.end())
            {
                RouteInfoList::const_iterator irouteInfo = iengineInfo->second.routeInfoList.begin(),
                                              endirouteInfo = iengineInfo->second.routeInfoList.end();
                for (; irouteInfo != endirouteInfo && irouteInfo->routeId != routeId; ++irouteInfo)
                {
                }
                if (irouteInfo != endirouteInfo)
                {
                    ServiceInfoVector::const_iterator iserviceInfo = iengineInfo->second.serviceInfoVector.begin(),
                                                      endiserviceInfo = iengineInfo->second.serviceInfoVector.end();
                    for (const char *serviceName = _Service::name();
                         iserviceInfo != endiserviceInfo && iserviceInfo->serviceName != serviceName; ++iserviceInfo)
                    {
                    }
                    if (iserviceInfo != endiserviceInfo)
                    {
                        assert(iserviceInfo->serviceName.empty() == false);
                        return ActorId(iserviceInfo->serviceInProcessActorId, routeId);
                    }
                }
            }
            return ActorId();
        }
    };
    Actor::ActorReference<SingletonInnerActor> singletonInnerActor;
    ProxyWeakReference &proxyWeakReference;
};

uint64_t EngineToEngineSerialConnector::serializePointer(const void *p) noexcept
{
    assert(sizeof(p) <= sizeof(uint64_t));
    return (uint64_t)((uintptr_t)p);
}

void *EngineToEngineSerialConnector::deserializePointer(uint64_t p) noexcept
{
    assert(sizeof(p) >= sizeof(void *));
    return reinterpret_cast<void *>((uintptr_t)p);
}

bool EngineToEngineSerialConnector::readSerial()
{
    size_t readSize, serialEventSize;
    const size_t currentReadBufferSize = readSerialBuffer.getCurrentReadBufferSize();
    const SerialRouteEventHeader *serialRouteEventHeader;
    if (readSerialBuffer.size() >= sizeof(SerialRouteEventHeader) &&
        currentReadBufferSize >= sizeof(SerialRouteEventHeader) &&
        *static_cast<const uint8_t *>(readSerialBuffer.getCurrentReadBuffer()) == RouteEventType &&
        readSerialBuffer.size() >=
            (readSize = sizeof(SerialRouteEventHeader) +
                        (serialEventSize = (serialRouteEventHeader = static_cast<const SerialRouteEventHeader *>(
                                                readSerialBuffer.getCurrentReadBuffer()))
                                               ->getEventSize())) &&
        currentReadBufferSize >= readSize)
    {
        try
        {
            readSerialEvent(*serialRouteEventHeader, serialRouteEventHeader + 1, serialEventSize);
        }
        catch (...)
        {
            readSerialBuffer.decreaseCurrentReadBufferSize(readSize);
            throw;
        }
        readSerialBuffer.decreaseCurrentReadBufferSize(readSize);
        return true;
    }
    else if (readSerialBuffer.size() >= sizeof(uint8_t))
    {
        assert(currentReadBufferSize >= sizeof(uint8_t));
        switch (*static_cast<const uint8_t *>(readSerialBuffer.getCurrentReadBuffer()))
        {
        case RouteEventType:
            return readSerialRouteEvent();
        case ReturnEventType:
            return readSerialReturnEvent();
        case AbsoluteIdEventType:
            return readSerialAbsoluteIdEvent();
        default:
        case InvalidEventType:
            throw SerialException(SerialException::InvalidEventTypeReason);
        }
    }
    else
    {
        assert(readSerialBuffer.empty());
        return false;
    }
}

void EngineToEngineSerialConnector::readSerialEvent(const SerialRouteEventHeader &header,
                                                         const void *eventSerialBuffer, size_t eventSerialBufferSize)
{
    Actor::EventId peerEventId = header.getEventId();
    EventE2EDeserialize *eventE2EDeserialize;
    if (peerEventId >= Actor::MAX_EVENT_ID_COUNT ||
        *(eventE2EDeserialize = eventIdDeserializeArray + peerEventId) == tredzone::null)
    {
        Actor::Event::SerialBuffer::WriteMark eventHeaderWriteMark = writeSerialBuffer.getCurrentWriteMark();
        writeSerialBuffer.increaseCurrentWriteBufferSize(eventSerialBufferSize);
        writeSerialBuffer.copyWriteBuffer(eventSerialBuffer, eventSerialBufferSize, eventHeaderWriteMark);
        writeSerialBuffer.increaseCurrentWriteBufferSize(sizeof(SerialReturnEventHeader));
        new (writeSerialBuffer.getWriteMarkBuffer(eventHeaderWriteMark))
            SerialReturnEventHeader(header.getEventId(), header.getDestinationInProcessActorId(),
                                    header.getSourceInProcessActorId(), (uint32_t)eventSerialBufferSize);
        return;
    }
    assert(eventE2EDeserialize->deserializeFunction != 0);
    EngineToEngineConnectorEventFactory eventFactory(*this, header
#ifndef NDEBUG
                                                          ,
                                                          eventE2EDeserialize->localEventId
#endif
                                                          );
    assert(header.getEventSize() == eventSerialBufferSize);
    (*eventE2EDeserialize->deserializeFunction)(eventFactory, eventSerialBuffer, eventSerialBufferSize);
}

template<class _Header>
void EngineToEngineSerialConnector::writeSerialEvent2(const Actor::Event &event)
{
    const Actor::EventId    eventId = event.getClassId();
    assert(eventId < Actor::MAX_EVENT_ID_COUNT);
    
    Actor::Event::EventE2ESerializeFunction eventSerializeFn = eventIdSerializeFnArray[eventId];
    Actor::Event::EventE2EDeserializeFunction eventDeserializeFn;
    if (eventSerializeFn == nullptr)
    {
        const char *absoluteEventId;
        if (isEventE2ECapable(eventId, absoluteEventId, eventSerializeFn, eventDeserializeFn) == false)
        {
            throw NotE2ECapableException();
        }
        writeSerialAbsoluteEventId(eventId, absoluteEventId);
        eventIdSerializeFnArray[eventId] = eventSerializeFn;
    }
    Actor::Event::SerialBuffer::WriteMark eventHeaderWriteMark = writeSerialBuffer.getCurrentWriteMark();
    writeSerialBuffer.increaseCurrentWriteBufferSize(sizeof(_Header));
    size_t serialEventSize = writeSerialBuffer.size();
    assert(eventSerializeFn != nullptr);
    eventSerializeFn(writeSerialBuffer, event);
    assert(writeSerialBuffer.size() >= serialEventSize);
    serialEventSize = writeSerialBuffer.size() - serialEventSize;
    assert(serialEventSize <= std::numeric_limits<uint32_t>::max());
    if (writeSerialBuffer.getWriteMarkBufferSize(eventHeaderWriteMark) < sizeof(_Header))
    {
        _Header serialRouteEventHeader(eventId, event.getSourceInProcessActorId(),
                                       event.getDestinationInProcessActorId(), (uint32_t)serialEventSize);
        writeSerialBuffer.copyWriteBuffer(&serialRouteEventHeader, sizeof(serialRouteEventHeader),
                                          eventHeaderWriteMark);
    }
    else
    {
        new (writeSerialBuffer.getWriteMarkBuffer(eventHeaderWriteMark))
            _Header(eventId, event.getSourceInProcessActorId(), event.getDestinationInProcessActorId(),
                    (uint32_t)serialEventSize);
    }
}

void *EngineToEngineSharedMemoryConnector::allocateEvent(size_t sz)
{
    assert(sharedMemory != 0);
    return sharedMemory->allocateEvent(sz);
}

} // namespace