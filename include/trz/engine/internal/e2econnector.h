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
struct AsyncService
{
    inline static const char *name() noexcept { return ""; }
};

namespace service
{

struct E2ERoute : AsyncService
{
};
}

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
    AsyncActor::ActorId::RouteId routeId;
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
    AsyncActor::InProcessActorId serviceInProcessActorId;
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
struct ConnectorRegisterEvent : public AsyncActor::Event
{
    AsyncActor::ActorId::RouteId routeId;
    bool isSharedMemory;
    connection_distance_type serialConnectionDistance;
    const char *engineName;
    const char *engineSuffix;
    ServiceEntry *firstServiceEntry;
};
/**
 * @brief Connector unregistration event
 */
struct ConnectorUnregisterEvent : AsyncActor::Event
{
    AsyncActor::ActorId::RouteId routeId;
};
/**
 * @brief Singleton client subscription event
 */
struct SingletonClientSubscriptionEvent : AsyncActor::Event
{
    AsyncActor::NodeId clientNodeId;
    bool unsubscribeFlag;
};
/**
 * @brief Singleton client engine event
 */
struct SingletonClientEngineEvent : AsyncActor::Event
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
class AsyncEngineToEngineConnector
{
  public:
    class ServiceActor;
    /** @brief NotE2ECapableException is thrown when an unroutable Event is pushed to a cluster Actor */
    struct NotE2ECapableException : std::exception
    {
        virtual const char *what() const noexcept
        {
            return "tredzone::AsyncEngineToEngineConnector::NotE2ECapableException";
        }
    };
    /** @brief E2ERouteServiceMissingException is thrown when the E2ERouteService is missing from the StartSequence */
    struct E2ERouteServiceMissingException : std::exception
    {
        virtual const char *what() const noexcept
        {
            return "tredzone::AsyncEngineToEngineConnector::E2ERouteServiceMissingException";
        }
    };
    /**
     * @brief cross-machine service index entry
     */
    struct ServiceEntry
    {
        AsyncActor::string_type serviceName;
        AsyncActor::AsyncActor::InProcessActorId serviceInProcessActorId;
        /** @brief Basic constructor */
        inline ServiceEntry(const char *serviceName, const AsyncActor::InProcessActorId &serviceInProcessActorId,
                            const AsyncActor::AllocatorBase &allocator)
            : serviceName(serviceName, allocator), serviceInProcessActorId(serviceInProcessActorId)
        {
        }
        /** @brief Basic constructor */
        inline ServiceEntry(const AsyncActor::AllocatorBase &allocator)
            : serviceName(0, allocator), serviceInProcessActorId()
        {
        }
    };
    typedef std::vector<ServiceEntry, AsyncActor::Allocator<ServiceEntry>> ServiceEntryVector;

    /** @brief Basic constructor */
    AsyncEngineToEngineConnector(AsyncActor &); // throw(std::bad_alloc, E2ERouteServiceMissingException)
    /** @brief Basic destructor */
    virtual ~AsyncEngineToEngineConnector() noexcept;
    /**
     * @brief Get a comparable RouteId
     * @return Comparable RouteId
     */
    inline AsyncActor::ActorId::RouteIdComparable getRouteIdComparable() const noexcept
    {
        return AsyncActor::ActorId::RouteIdComparable(nodeId, nodeConnectionId);
    }

  protected:
    typedef AsyncActor::ActorId::RouteId::NodeConnectionId NodeConnectionId;
    typedef void (*OnOutboundEventFn)(AsyncEngineToEngineConnector *, const AsyncActor::Event &);
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
     * @note Will trigger AsyncEngineToEngineConnector::ServiceActor::Proxy::onAddEngineRoute()
     * @param peerEngineName remote connection's Engine name
     * @param peerEngineSuffix remote connection's Engine suffix
     * @param serviceEntryVector remote connection's Engine ServiceEntryVector
     * @param onOutboundEventFn function to be called on outbound Event
     * @param onInboundUndeliveredEventFn function to be called on inbound Event
     * @param isAsyncEngineToEngineSharedMemoryConnectorFlag true if connector is shared memory
     * @param serialConnectionDistance connection distance to remote connection
     */
    void registerConnection(const char *, const char *, const ServiceEntryVector &, OnOutboundEventFn,
                            OnInboundUndeliveredEventFn, bool,
                            e2econnector::connection_distance_type); // throw(std::bad_alloc)
                                                                     /**
                                                                      * @brief Unregister a connection to remote Engine
                                                                      * @note Will trigger AsyncEngineToEngineConnector::ServiceActor::Proxy::onRemoveEngineRoute()
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
    inline AsyncActor::ActorId::RouteId getRouteId() const noexcept
    {
        return AsyncActor::ActorId::RouteId(nodeId, nodeConnectionId, nodeConnection);
    }
    /**
     * @brief Set RouteId from ActorId
     * @param externalActorId values to be set from
     */
    inline void setRouteId(AsyncActor::ActorId &externalActorId) const noexcept
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
    inline static bool isEventE2ECapable(AsyncActor::EventId eventId, const char *&absoluteEventId,
                                         AsyncActor::Event::EventE2ESerializeFunction &serializeFn,
                                         AsyncActor::Event::EventE2EDeserializeFunction &deserializeFn)
    {
        return AsyncActor::Event::isE2ECapable(eventId, absoluteEventId, serializeFn, deserializeFn);
    }
    /**
     * @brief Check if given event is EngineToEngine capable
     * @param absoluteEventId event name
     * @param eventId event id
     * @param serializeFn event serialization function
     * @param deserializeFn event deserilization function
     * @return
     */
    inline static bool isEventE2ECapable(const char *absoluteEventId, AsyncActor::EventId &eventId,
                                         AsyncActor::Event::EventE2ESerializeFunction &serializeFn,
                                         AsyncActor::Event::EventE2EDeserializeFunction &deserializeFn)
    {
        return AsyncActor::Event::isE2ECapable(absoluteEventId, eventId, serializeFn, deserializeFn);
    }
    /**
     * @brief Get EventId from Event name
     * @param absoluteEventId event name
     * @return event id
     */
    inline static std::pair<bool, AsyncActor::EventId> findEventId(const char *absoluteEventId) noexcept
    {
        return AsyncActor::Event::findEventId(absoluteEventId);
    }
    /** @brief Basic getter */
    inline const AsyncActor::AllocatorBase &getAllocator() const noexcept { return allocator; }

  private:
    struct InnerActor : AsyncActor, AsyncActor::Callback
    {
        AsyncEngineToEngineConnector *connector;
        ActorId::RouteId connectionRouteId;
        InnerActor(AsyncEngineToEngineConnector *);
        void onUndeliveredEvent(const e2econnector::ConnectorRegisterEvent &);
        void onCallback() noexcept;
        void registerConnectionService(const AsyncActor::ActorId::RouteId &, const char *, const char *, bool,
                                       e2econnector::connection_distance_type,
                                       const ServiceEntryVector &); // throw(std::bad_alloc)
        void unregisterConnectionService() noexcept;
    };

    class Singleton;
    const AsyncActor::NodeId nodeId;
    NodeConnectionId nodeConnectionId;
    AsyncActor::NodeConnection *nodeConnection;
    AsyncActor::string_type peerEngineName;
    AsyncActor::string_type peerEngineSuffix;
    AsyncActor::ActorReference<Singleton> singleton;
    AsyncActor::ActorReference<InnerActor> innerActor;
    AsyncActor::AllocatorBase allocator;
};

/**
 * @brief EngineToEngine serial connector class
 * Network is little endian encoded.
 * Specialization through Connection class must implement (possibly inlined):
 * void onWriteSerialEvent(const AsyncActor::Event&)
 * void onWriteSerialUndeliveredEvent(const AsyncActor::Event&)
 */
class AsyncEngineToEngineSerialConnector : protected AsyncEngineToEngineConnector
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
                return "tredzone::AsyncEngineToEngineSerialConnector::SerialException [OutOfRangeEventId]";
            case UndefinedEventIdReason:
                return "tredzone::AsyncEngineToEngineSerialConnector::SerialException [UndefinedEventId]";
            case InvalidEventTypeReason:
                return "tredzone::AsyncEngineToEngineSerialConnector::SerialException [InvalidEventType]";
            default:
                return "tredzone::AsyncEngineToEngineSerialConnector::SerialException [unknown type]"; // unknown serial
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
        inline SerialInProcessActorId(const AsyncActor::InProcessActorId &inProcessActorId) noexcept :
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
        inline AsyncActor::NodeId getNodeId() const noexcept
        {
#ifdef TREDZONE_LITTLE_ENDIAN
            return nodeId;
#else
            return littleToNativeEndian(nodeId);
#endif
        }
        inline operator AsyncActor::InProcessActorId() const noexcept
        {
#ifdef TREDZONE_LITTLE_ENDIAN
            return AsyncActor::InProcessActorId(nodeId, nodeActorId,
                                                static_cast<AsyncActor::EventTable *>(deserializePointer(eventTable)));
#else
            return AsyncActor::InProcessActorId(
                littleToNativeEndian(nodeId), littleToNativeEndian(nodeActorId),
                static_cast<AsyncActor::EventTable *>(deserializePointer(littleToNativeEndian(eventTable))));
#endif
        }

      private:
        AsyncActor::NodeId nodeId;
        AsyncActor::NodeActorId nodeActorId;
        uint64_t eventTable;
    };
#pragma pack(pop)

    typedef e2econnector::connection_distance_type connection_distance_type;

    /** @brief Constructor */
    AsyncEngineToEngineSerialConnector(AsyncActor &, size_t serialBufferSize = 4096 - sizeof(void *)); // hardcoded
    /** @brief Destructor */
    virtual ~AsyncEngineToEngineSerialConnector() noexcept;

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
    inline AsyncActor::ActorId setRouteId(const SerialInProcessActorId &externalActorId) const noexcept
    {
        AsyncActor::ActorId ret(externalActorId);
        AsyncEngineToEngineConnector::setRouteId(ret);
        return ret;
    }
    /**
     * @see setRouteId(const SerialInProcessActorId& externalActorId)
     * @param externalActorId
     */
    inline void setRouteId(AsyncActor::ActorId &externalActorId) const noexcept
    {
        return AsyncEngineToEngineConnector::setRouteId(externalActorId);
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
        connector.AsyncEngineToEngineConnector::registerConnection(
            peerEngineName, peerEngineSuffix, serviceEntryVector, &StaticOnEvent<_Connector>::onWriteSerialEvent,
            &StaticOnEvent<_Connector>::onWriteSerialUndeliveredEvent, false, distance);
        for (int i = 0; i < AsyncActor::MAX_EVENT_ID_COUNT; ++i)
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
        AsyncEngineToEngineConnector::unregisterConnection();
        readSerialBuffer.clear();
        writeSerialBuffer.clear();
    }
    /** @brief Get serial read buffer */
    inline AsyncActor::Event::SerialBuffer &getReadSerialBuffer() noexcept { return readSerialBuffer; }
    /** @brief Get serial write buffer */
    inline AsyncActor::Event::SerialBuffer &getWriteSerialBuffer() noexcept { return writeSerialBuffer; }
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
    inline void writeSerialEvent(const AsyncActor::Event &event) { writeSerialEvent2<SerialRouteEventHeader>(event); }
    /**
     * @brief Write a SerialUndeliveredEvent into the writeSerialBuffer
     * @param undelivered event to write into serial buffer
     * @throws std::bad_alloc
     * @throws NotE2ECapableException is thrown if event is not EngineToEngine capable. ei: needed methods are not
     * implemented by event
     */
    inline void writeSerialUndeliveredEvent(const AsyncActor::Event &event)
    { // throw(std::bad_alloc, NotE2ECapableException)
        writeSerialEvent2<SerialReturnEventHeader>(event);
    }

  private:
    friend class AsyncEngineToEngineConnectorEventFactory;
    enum EventTypeEnum
    {
        RouteEventType = 0,
        ReturnEventType,
        AbsoluteIdEventType,
        InvalidEventType
    };

#pragma pack(push)
#pragma pack(1)
    class SerialAbsoluteEventIdHeader
    {
      public:
        inline SerialAbsoluteEventIdHeader(AsyncActor::EventId eventId, uint32_t absoluteEventIdSize) noexcept :
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
        inline EventTypeEnum getEventType() const noexcept
        {
            assert(eventType == AbsoluteIdEventType);
            return (EventTypeEnum)eventType;
        }
        inline AsyncActor::EventId getEventId() const noexcept
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
        uint8_t eventType; // EventTypeEnum
        AsyncActor::EventId eventId;
        uint32_t absoluteEventIdSize;
    };
    class SerialEventHeader
    {
      public:
        inline AsyncActor::EventId getEventId() const noexcept
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
            return SerialEventHeader((EventTypeEnum)eventType, eventId, destinationInProcessActorId,
                                     sourceInProcessActorId, eventSize);
        }

      protected:
        uint8_t eventType; // EventTypeEnum

        inline SerialEventHeader(EventTypeEnum eventType, AsyncActor::EventId eventId,
                                 const AsyncActor::InProcessActorId &sourceInProcessActorId,
                                 const AsyncActor::InProcessActorId &destinationInProcessActorId,
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
        AsyncActor::EventId eventId;
        SerialInProcessActorId sourceInProcessActorId;
        SerialInProcessActorId destinationInProcessActorId;
        uint32_t eventSize;
    };
    struct SerialRouteEventHeader : SerialEventHeader
    {
        inline SerialRouteEventHeader(AsyncActor::EventId eventId,
                                      const AsyncActor::InProcessActorId &sourceInProcessActorId,
                                      const AsyncActor::InProcessActorId &destinationInProcessActorId,
                                      uint32_t eventSize) noexcept
            : SerialEventHeader(RouteEventType, eventId, sourceInProcessActorId, destinationInProcessActorId, eventSize)
        {
        }
        inline EventTypeEnum getEventType() const noexcept
        {
            assert(eventType == RouteEventType);
            return (EventTypeEnum)eventType;
        }
    };
    struct SerialReturnEventHeader : SerialEventHeader
    {
        inline SerialReturnEventHeader(AsyncActor::EventId eventId,
                                       const AsyncActor::InProcessActorId &sourceInProcessActorId,
                                       const AsyncActor::InProcessActorId &destinationInProcessActorId,
                                       uint32_t eventSize) noexcept
            : SerialEventHeader(ReturnEventType, eventId, sourceInProcessActorId, destinationInProcessActorId,
                                eventSize)
        {
        }
        inline EventTypeEnum getEventType() const noexcept
        {
            assert(eventType == ReturnEventType);
            return (EventTypeEnum)eventType;
        }
    };
#pragma pack(pop)
    template <class _Connector> struct StaticOnEvent
    {
        inline static void onWriteSerialEvent(AsyncEngineToEngineConnector *connector, const AsyncActor::Event &event)
        {
            static_cast<_Connector *>(connector)->onWriteSerialEvent(event);
        }
        inline static void onWriteSerialUndeliveredEvent(AsyncEngineToEngineConnector *connector,
                                                         const AsyncActor::Event &event)
        {
            static_cast<_Connector *>(connector)->onWriteSerialUndeliveredEvent(event);
        }
    };
    struct EventE2EDeserialize
    {
        AsyncActor::EventId localEventId;
        AsyncActor::Event::EventE2EDeserializeFunction deserializeFunction;
        inline void operator=(const Null &) noexcept
        {
            localEventId = AsyncActor::MAX_EVENT_ID_COUNT;
            deserializeFunction = 0;
        }
        inline bool operator==(const Null &) const noexcept
        {
            assert((localEventId == AsyncActor::MAX_EVENT_ID_COUNT && deserializeFunction == 0) ||
                   (localEventId > 0 && localEventId < AsyncActor::MAX_EVENT_ID_COUNT && deserializeFunction != 0));
            return localEventId == AsyncActor::MAX_EVENT_ID_COUNT;
        }
        inline bool operator!=(const Null &) const noexcept { return !(*this == tredzone::null); }
    };

    AsyncActor::Event::EventE2ESerializeFunction *eventIdSerializeFnArray;
    EventE2EDeserialize *eventIdDeserializeArray;
    AsyncActor::Event::SerialBuffer readSerialBuffer;
    AsyncActor::Event::SerialBuffer writeSerialBuffer;
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
    void writeSerialAbsoluteEventId(AsyncActor::EventId, const char *);                // throw(std::bad_alloc)
    template <class _Header>
    inline void writeSerialEvent2(const AsyncActor::Event &); // throw(std::bad_alloc, NotE2ECapableException)
};

/**
 * @brief Factory method used in the Event's deserialization function to deliver recieved and deserialized event
 */
class AsyncEngineToEngineConnectorEventFactory
{
  public:
    /**	@brief Basic destructor */
    inline ~AsyncEngineToEngineConnectorEventFactory() noexcept
    {
        assert(destinationEventChain != 0);
        assert(event != 0);
        destinationEventChain->push_back(event);
    }
    /** @brief Get allocator */
    AsyncActor::Event::AllocatorBase getAllocator() noexcept;

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
    inline void setRouteId(AsyncActor::ActorId &externalActorId) const noexcept
    {
        serialConnector.setRouteId(externalActorId);
    }
    /**
     * @brief Set serial connector's route id from given ActorId
     * @param externalInProcessActorId in process ActorId
     * @return ActorId
     */
    inline AsyncActor::ActorId setRouteId(const AsyncActor::InProcessActorId &externalInProcessActorId) const noexcept
    {
        AsyncActor::ActorId ret(externalInProcessActorId);
        setRouteId(ret);
        return ret;
    }

  private:
    friend class AsyncEngineToEngineSerialConnector;

    struct NewEventVoidParameter
    {
    };

    template <class _Event, class _EventInit> struct EventWrapper : virtual private AsyncActor::EventBase, _Event
    {
        inline EventWrapper(const AsyncEngineToEngineSerialConnector::SerialEventHeader &serialEventHeader,
                            const _EventInit &eventInit)
            : AsyncActor::EventBase(AsyncActor::Event::getClassId<_Event>(),
                                    serialEventHeader.getSourceInProcessActorId(),
                                    serialEventHeader.getDestinationInProcessActorId(),
                                    (AsyncActor::Event::route_offset_type)(
                                        ((uintptr_t) static_cast<AsyncActor::Event *>((EventWrapper *)0) +
                                         sizeof(AsyncActor::ActorId::RouteId))
                                        << 1)),
              _Event(eventInit)
        {
            assert(
                ((uintptr_t) static_cast<AsyncActor::Event *>((EventWrapper *)0) + sizeof(AsyncActor::ActorId::RouteId))
                    << 1 <=
                std::numeric_limits<AsyncActor::Event::route_offset_type>::max());
        }
    };
    template <class _Event>
    struct EventWrapper<_Event, NewEventVoidParameter> : virtual private AsyncActor::EventBase, _Event
    {
        inline EventWrapper(const AsyncEngineToEngineSerialConnector::SerialEventHeader &serialEventHeader,
                            const NewEventVoidParameter &)
            : AsyncActor::EventBase(AsyncActor::Event::getClassId<_Event>(),
                                    serialEventHeader.getSourceInProcessActorId(),
                                    serialEventHeader.getDestinationInProcessActorId(),
                                    (AsyncActor::Event::route_offset_type)(
                                        ((uintptr_t) static_cast<AsyncActor::Event *>((EventWrapper *)0) +
                                         sizeof(AsyncActor::ActorId::RouteId))
                                        << 1))
        {
            assert(
                ((uintptr_t) static_cast<AsyncActor::Event *>((EventWrapper *)0) + sizeof(AsyncActor::ActorId::RouteId))
                    << 1 <=
                std::numeric_limits<AsyncActor::Event::route_offset_type>::max());
        }
    };

    const AsyncEngineToEngineSerialConnector &serialConnector;
    const AsyncEngineToEngineSerialConnector::SerialEventHeader &serialEventHeader;
    AsyncActor::Event::Chain *destinationEventChain;
    AsyncActor::Event *event;
    AsyncActor::Event::AllocatorBase::Factory eventAllocatorFactory;
#ifndef NDEBUG
    AsyncActor::EventId debugLocalEventId;
#endif

#ifndef NDEBUG
    inline AsyncEngineToEngineConnectorEventFactory(
        const AsyncEngineToEngineSerialConnector &serialConnector,
        const AsyncEngineToEngineSerialConnector::SerialEventHeader &serialEventHeader,
        AsyncActor::EventId debugLocalEventId) noexcept : serialConnector(serialConnector),
                                                          serialEventHeader(serialEventHeader),
                                                          destinationEventChain(0),
                                                          event(0),
                                                          debugLocalEventId(debugLocalEventId)
    {
    }
#else
    inline AsyncEngineToEngineConnectorEventFactory(
        const AsyncEngineToEngineSerialConnector &serialConnector,
        const AsyncEngineToEngineSerialConnector::SerialEventHeader &serialEventHeader) noexcept
        : serialConnector(serialConnector),
          serialEventHeader(serialEventHeader)
    {
    }
#endif
    void *allocate(size_t);
    void *allocateEvent(size_t, AsyncActor::Event::Chain *&);
    inline void toBeUndeliveredRoutedEvent() noexcept;
};

/**
 * @brief EngineToEngine shared memory connector class
 * @note: available in upcoming version
 */
class AsyncEngineToEngineSharedMemoryConnector : protected AsyncEngineToEngineConnector
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
            return "tredzone::AsyncEngineToEngineSharedMemoryConnector::NotConnectedSharedMemoryException";
        }
    };

    /** @brief Constructor */
    inline AsyncEngineToEngineSharedMemoryConnector(AsyncActor &actor)
        : AsyncEngineToEngineConnector(actor), actor(actor), sharedMemory(0)
    {
    }

  protected:
    void registerConnection(const char *peerEngineName, const char *peerEngineSuffix,
                            const ServiceEntryVector &serviceEntryVector,
                            SharedMemory &); // throw(std::bad_alloc, NotConnectedSharedMemoryException)
    void unregisterConnection() noexcept;
    virtual void onDisconnected() noexcept = 0;

  private:
    friend class AsyncActor;
    struct PreBarrierCallback : AsyncActor::Callback
    {
        inline void onCallback() noexcept;
    };
    struct PostBarrierCallback : AsyncActor::Callback
    {
        inline void onCallback() noexcept;
    };

    AsyncActor &actor;
    SharedMemory *sharedMemory;
    PreBarrierCallback preBarrierCallback;
    PostBarrierCallback postBarrierCallback;

    static void onWriteSerialEvent(AsyncEngineToEngineConnector *connector, const AsyncActor::Event &event);
    static void onWriteSerialUndeliveredEvent(AsyncEngineToEngineConnector *connector, const AsyncActor::Event &event);
    inline void *allocateEvent(size_t sz); // throw(std::bad_alloc)
};

/**
 * @brief SharedMemory class that is used with AsyncEngineToEngineSharedMemoryConnector
 * @note: available in upcoming version
 */
class AsyncEngineToEngineSharedMemoryConnector::SharedMemory
{
  public:
    struct UndersizedSharedMemoryException : std::exception
    {
        virtual const char *what() const noexcept
        {
            return "tredzone::AsyncEngineToEngineSharedMemoryConnector::SharedMemory::UndersizedSharedMemoryException";
        }
    };

    SharedMemory(const AsyncActor &, void *, size_t); // throw(UndersizedSharedMemoryException)
    ~SharedMemory() noexcept;
    bool tryConnect() noexcept; // no more than one attempt by actor callback (as it may require a memory barrier
                                // between 2 subsequent calls)
    bool isConnected() noexcept;

  private:
    friend class AsyncActor;
    friend class AsyncEngineToEngineSharedMemoryConnector;
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
class AsyncEngineToEngineConnector::ServiceActor : public AsyncActor
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
        inline SubscriberNodeEntry(AsyncActor &actor) noexcept : nodeId(MAX_NODE_COUNT), pipe(actor) {}
    };
    struct EventHandler : AsyncActor::Callback
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
class AsyncEngineToEngineConnector::ServiceActor::Proxy
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
        AsyncActor::string_type serviceName;
        /** @brief Constructor */
        inline ServiceInfo(const char *serviceName, const AsyncActor::InProcessActorId &serviceInProcessActorId,
                           const AsyncActor::AllocatorBase &allocator)
            : serviceName(serviceName, allocator), serviceInProcessActorId(serviceInProcessActorId)
        {
        }

      private:
        friend struct SingletonInnerActor;
        AsyncActor::InProcessActorId serviceInProcessActorId;
    };
    typedef std::vector<ServiceInfo, AsyncActor::Allocator<ServiceInfo>> ServiceInfoVector; /**< Service info vector */
    typedef std::list<RouteInfo, AsyncActor::Allocator<RouteInfo>> RouteInfoList;           /**< Route info vector */

    /**
     * @brief EngineInfo class
     */
    struct EngineInfo
    {
        AsyncActor::string_type engineName;
        AsyncActor::string_type engineSuffix;
        ServiceInfoVector serviceInfoVector;
        RouteInfoList routeInfoList;
        inline EngineInfo(const AsyncActor::AllocatorBase &allocator)
            : engineName(allocator), engineSuffix(allocator), serviceInfoVector(allocator), routeInfoList(allocator)
        {
        }
    };
    typedef std::map<engine_id_type, EngineInfo, std::less<engine_id_type>,
                     AsyncActor::Allocator<std::pair<const engine_id_type, EngineInfo>>>
        EngineInfoByIdMap;

    /** @brief Constructor */
    Proxy(AsyncActor &);
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
    inline AsyncActor::ActorId getServiceActorId(engine_id_type engineId,
                                                 const AsyncActor::ActorId::RouteId &routeId) const noexcept
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
    struct SingletonInnerActor : AsyncActor
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
                                                            const AsyncActor::ActorId::RouteId &) noexcept;
        inline static e2econnector::RouteEntry *findRouteEntry(e2econnector::RouteEntry *,
                                                               const AsyncActor::ActorId::RouteId &) noexcept;
        template <class _Service>
        inline ActorId getServiceActorId(engine_id_type engineId, const AsyncActor::ActorId::RouteId &routeId) const
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
    AsyncActor::ActorReference<SingletonInnerActor> singletonInnerActor;
    ProxyWeakReference &proxyWeakReference;
};

uint64_t AsyncEngineToEngineSerialConnector::serializePointer(const void *p) noexcept
{
    assert(sizeof(p) <= sizeof(uint64_t));
    return (uint64_t)((uintptr_t)p);
}

void *AsyncEngineToEngineSerialConnector::deserializePointer(uint64_t p) noexcept
{
    assert(sizeof(p) >= sizeof(void *));
    return reinterpret_cast<void *>((uintptr_t)p);
}

bool AsyncEngineToEngineSerialConnector::readSerial()
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

void AsyncEngineToEngineSerialConnector::readSerialEvent(const SerialRouteEventHeader &header,
                                                         const void *eventSerialBuffer, size_t eventSerialBufferSize)
{
    AsyncActor::EventId peerEventId = header.getEventId();
    EventE2EDeserialize *eventE2EDeserialize;
    if (peerEventId >= AsyncActor::MAX_EVENT_ID_COUNT ||
        *(eventE2EDeserialize = eventIdDeserializeArray + peerEventId) == tredzone::null)
    {
        AsyncActor::Event::SerialBuffer::WriteMark eventHeaderWriteMark = writeSerialBuffer.getCurrentWriteMark();
        writeSerialBuffer.increaseCurrentWriteBufferSize(eventSerialBufferSize);
        writeSerialBuffer.copyWriteBuffer(eventSerialBuffer, eventSerialBufferSize, eventHeaderWriteMark);
        writeSerialBuffer.increaseCurrentWriteBufferSize(sizeof(SerialReturnEventHeader));
        new (writeSerialBuffer.getWriteMarkBuffer(eventHeaderWriteMark))
            SerialReturnEventHeader(header.getEventId(), header.getDestinationInProcessActorId(),
                                    header.getSourceInProcessActorId(), (uint32_t)eventSerialBufferSize);
        return;
    }
    assert(eventE2EDeserialize->deserializeFunction != 0);
    AsyncEngineToEngineConnectorEventFactory eventFactory(*this, header
#ifndef NDEBUG
                                                          ,
                                                          eventE2EDeserialize->localEventId
#endif
                                                          );
    assert(header.getEventSize() == eventSerialBufferSize);
    (*eventE2EDeserialize->deserializeFunction)(eventFactory, eventSerialBuffer, eventSerialBufferSize);
}

template <class _Header> void AsyncEngineToEngineSerialConnector::writeSerialEvent2(const AsyncActor::Event &event)
{
    AsyncActor::EventId eventId = event.getClassId();
    assert(eventId < AsyncActor::MAX_EVENT_ID_COUNT);
    AsyncActor::Event::EventE2ESerializeFunction eventSerializeFn = eventIdSerializeFnArray[eventId];
    AsyncActor::Event::EventE2EDeserializeFunction eventDeserializeFn;
    if (eventSerializeFn == 0)
    {
        const char *absoluteEventId;
        if (isEventE2ECapable(eventId, absoluteEventId, eventSerializeFn, eventDeserializeFn) == false)
        {
            throw NotE2ECapableException();
        }
        writeSerialAbsoluteEventId(eventId, absoluteEventId);
        eventIdSerializeFnArray[eventId] = eventSerializeFn;
    }
    AsyncActor::Event::SerialBuffer::WriteMark eventHeaderWriteMark = writeSerialBuffer.getCurrentWriteMark();
    writeSerialBuffer.increaseCurrentWriteBufferSize(sizeof(_Header));
    size_t serialEventSize = writeSerialBuffer.size();
    assert(eventSerializeFn != 0);
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

void *AsyncEngineToEngineSharedMemoryConnector::allocateEvent(size_t sz)
{
    assert(sharedMemory != 0);
    return sharedMemory->allocateEvent(sz);
}

} // namespace