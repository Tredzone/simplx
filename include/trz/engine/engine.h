/**
 * @file engine.h
 * @brief Simplx engine class
 * @copyright 2013-2018 Tredzone (www.tredzone.com). All rights reserved.
 * Please see accompanying LICENSE file for licensing terms.
 */

#pragma once

#include <cstring>
#include <memory>

#include "trz/engine/actor.h"
#include "trz/engine/engineversion.h"
#include "trz/engine/internal/e2econnector.h"
#include "trz/engine/internal/property.h"

extern "C" {
/**
 * @brief Check if the linked Tredzone library type (debug/release) is compatible with the user's binary
 * @param isNDEBUG is true if the user's library is in debug mode, false otherwise
 * @return true if the library is compatible.
 */
bool TREDZONE_SDK_IS_DEBUG_RELEASE_COMPATIBLE(bool isNDEBUG);
/**
 * @brief Check if the linked Tredzone library is compiler compatible with the user's binary
 * This compared compiler major and minor version.
 * eg: g++ 4.9: major if 4 and minor is 9
 * @param compilerId Tredzone library's version
 * @param compilerVersion current compiler version
 * @return true if the library is compiler compatible.
 */
bool TREDZONE_SDK_IS_COMPILER_COMPATIBLE(int compilerId, const void *compilerVersion);

#ifdef TREDZONE_COMPILER_ID
    #error TREDZONE_COMPILER_ID macro already defined
#endif
    #pragma pack(push, 1)
#if defined(__GNUG__)
    /**
     * @def TREDZONE_COMPILER_ID
     * @brief Compiler specific identifier used for library compatibility check
     */
    #define TREDZONE_COMPILER_ID 1
    struct TREDZONE_SDK_COMPILER_VERSION_type
    {
        int vmajor;
        int vminor;
        int vpatch;
        inline TREDZONE_SDK_COMPILER_VERSION_type() : vmajor(__GNUC__), vminor(__GNUC_MINOR__), vpatch(__GNUC_PATCHLEVEL__)
        {
        }
    };
#else
    #error C++ compiler not supported!
#endif
#pragma pack(pop)
}

namespace tredzone
{

class Engine;
class AsyncNodeManager;
class EngineCustomEventLoopFactory;
class EngineToEngineConnector;

/**
 * @brief Asynchronous exception handler that contains a Mutex that is locked for screen output.
 * onEventException() and onUnreachableException() can be specialized by inheritance. These
 * methods are called by the engine under mutex-lock.
 * In case of specialization, use Engine::StartSequence::setExceptionHandler() method
 * to set the reference of an external exception-handler.
 * <br><b>The external exception-handler instance must outlive the engine instance.</b>
 */
class AsyncExceptionHandler
{
  public:
    /** Destructor */
    virtual ~AsyncExceptionHandler() {}
    void onEventExceptionSynchronized(Actor *, const std::type_info &asyncActorTypeInfo,
                                      const char *onXXX_FunctionName, const Actor::Event &,
                                      const char *whatException) noexcept;
    void onUnreachableExceptionSynchronized(Actor &, const std::type_info &asyncActorTypeInfo,
                                            const Actor::ActorId::RouteIdComparable &,
                                            const char *whatException) noexcept;
    virtual void onEventException(Actor *, const std::type_info &asyncActorTypeInfo,
                                  const char *onXXX_FunctionName, const Actor::Event &,
                                  const char *whatException) noexcept;
    virtual void onUnreachableException(Actor &, const std::type_info &asyncActorTypeInfo,
                                        const Actor::ActorId::RouteIdComparable &,
                                        const char *whatException) noexcept;

  private:
    Mutex mutex;
};

/**
 * @brief Base-class to event-loop.
 */
class EngineEventLoop
{
  public:
    /** @brief Default constructor */
    EngineEventLoop() noexcept;
    /** @brief Default destructor */
    virtual ~EngineEventLoop() noexcept;
    /** @brief Forces runWhileNotInterrupted() method to return */
    inline void interrupt() noexcept
    {
        assert(interruptFlag != 0);
        assert(debugIsInNodeThread());
        *interruptFlag = true;
    }
    /**
     * @brief Getter to the current engine instance.
     * @return the current engine instance.
     */
    Engine &getEngine() noexcept;
    /**
     * @brief Getter to the current engine instance.
     * @return the current engine const instance.
     */
    const Engine &getEngine() const noexcept;

  protected:
    virtual void run() noexcept = 0;
    virtual void preRun() noexcept;
    virtual void postRun() noexcept;
    void runWhileNotInterrupted() noexcept;
    void synchronize() noexcept;
    bool isRunning() noexcept;
    void returnToSender(const Actor::Event &) noexcept;
    Actor::NodeId getNodeId() const noexcept;
    Actor::CoreId getCoreId() const noexcept;
    AsyncExceptionHandler &getAsyncExceptionHandler() const noexcept;
    inline bool isInterrupted() const noexcept
    {
        assert(interruptFlag != 0);
        return *interruptFlag;
    }
#ifndef NDEBUG
    inline unsigned debugGetSynchronizeCount() const noexcept { return debugSynchronizeCount; }
#endif

  private:
    friend class AsyncNode;
    friend class EngineCustomEventLoopFactory;

    AsyncNode *asyncNode;
    bool *interruptFlag;
    bool isBeingDestroyedFlag;
#ifndef NDEBUG
    bool debugPreRunCalledFlag;
    unsigned debugSynchronizeCount;
#endif

#ifndef NDEBUG
    bool debugIsInNodeThread() const noexcept;
#endif
    void init(AsyncNode &) noexcept;
};

/**
 * @brief Base-class to specialize the engine's event-loop.
 * Specialization is achieved by overriding virtual newEventLoop() method.
 * In case of specialization, use Engine::StartSequence::setEngineCustomEventLoopFactory()
 * method to set the reference of an external event-loop-factory.
 * <br><b>The external event-loop-factory instance must outlive the engine instance.</b>
 */
class EngineCustomEventLoopFactory
{
  public:
    class DefaultEventLoop;
    template <class _EventLoop> struct EventLoopAutoPointerNew;

    /**
     * @brief Default engine's event-loop
     */
    class DefaultEventLoop : public EngineEventLoop
    {
      protected:
        virtual void run() noexcept;
    };

    /**
     * @brief Smart-pointer to an event-loop instance reference.
     */
    class EventLoopAutoPointer
    {
      public:
        /** @brief Default constructor */
        inline EventLoopAutoPointer() noexcept : eventLoop(0) {}
        /** @brief Default copy constructor */
        inline EventLoopAutoPointer(const EventLoopAutoPointer &other) noexcept : eventLoop(other.eventLoop)
        {
            other.eventLoop = 0;
        }
        /** @brief Default destructor */
        inline ~EventLoopAutoPointer() noexcept { reset(); }
        /** @brief event-loop factory method with no initialization argument */
        template <class _EventLoop> inline static EventLoopAutoPointer newEventLoop()
        {
            return EventLoopAutoPointer(new _EventLoop);
        }
        /** @brief event-loop factory method with an initialization argument */
        template <class _EventLoop, class _EventLoopInit>
        inline static EventLoopAutoPointer newEventLoop(const _EventLoopInit &eventLoopInit)
        {
            return EventLoopAutoPointer(new _EventLoop(eventLoopInit));
        }
        /** @brief Default equal operator */
        inline EventLoopAutoPointer &operator=(const EventLoopAutoPointer &other) noexcept
        {
            reset();
            eventLoop = other.eventLoop;
            other.eventLoop = 0;
            return *this;
        }
        inline EngineEventLoop *operator->() noexcept
        {
            assert(eventLoop != 0);
            return eventLoop;
        }
        inline const EngineEventLoop *operator->() const noexcept
        {
            assert(eventLoop != 0);
            return eventLoop;
        }
        inline EngineEventLoop &operator*() noexcept
        {
            assert(eventLoop != 0);
            return *eventLoop;
        }
        inline const EngineEventLoop &operator*() const noexcept
        {
            assert(eventLoop != 0);
            return *eventLoop;
        }
        inline void reset() noexcept
        {
            if (eventLoop != 0)
            {
                delete eventLoop;
                eventLoop = 0;
            }
        }

      private:
        mutable EngineEventLoop *eventLoop;

        EventLoopAutoPointer(EngineEventLoop *eventLoop) noexcept : eventLoop(eventLoop) {}
    };

    /** @brief Default destructor */
    virtual ~EngineCustomEventLoopFactory() {}
    /**
     * @brief Instantiates a new event-loop.
     * @return Smart-pointer to a new event-loop instance reference.
     * @throw std::bad_alloc
     * @throw ? Any other exception possibly thrown depending on specialized event-loop
     * constructor call.
     */
    virtual EventLoopAutoPointer newEventLoop();
};

/**
 * @brief Base-class to specialize CoreActor (core-actor).
 * A core-actor is run on every event-loop (cpu-core).
 * By default DefaultCoreActor is used.
 * Specialization is achieved by overriding virtual newCustomCoreActor() method.
 * In case of specialization, use Engine::StartSequence::setEngineCustomCoreActorFactory()
 * method to set the reference of an external core-actor-factory.
 * <br><b>The external core-actor-factory instance must outlive the engine instance.</b>
 */
class EngineCustomCoreActorFactory
{
  public:
    /** @brief Default destructor */
    virtual ~EngineCustomCoreActorFactory() noexcept {}
    /**
     * @brief Instantiates a new core-actor.
     * @param engine reference to current engine's instance.
     * @param coreId cpu core-id of the event-loop running the core-actor.
     * @param isRedZone true if the event-loop running the core-actor is in the red-zone.
     * @param parent parent actor which will hold the reference to the custom core-actor.
     * @return Smart-pointer to a new core-actor instance reference.
     * @throw std::bad_alloc
     * @throw ? Any other exception possibly thrown depending on specialized core-actor
     * constructor call.
     */
    virtual Actor::ActorReference<Actor> newCustomCoreActor(Engine &, Actor::CoreId, bool,
                                                                      Actor &parent)
    {
        return parent.newReferencedActor<DefaultCoreActor>();
    }

  protected:
    /**
     * @brief DefaultCoreActor calls threadYield() on every callback
     */
    struct DefaultCoreActor : Actor, Actor::Callback
    {
        inline DefaultCoreActor() { registerPerformanceNeutralCallback(*this); }
        inline void onCallback() noexcept
        {
            registerPerformanceNeutralCallback(*this);
            threadYield();
        }
    };
};

class IEngine
{
public:
    virtual ~IEngine() = default;
};

/**
 * @brief Tredzone runtime engine.
 *
 * The Engine manages the Actor's life cycle, event distribution, callback...
 */
class Engine: public virtual IEngine
{
  public:
    static const int SDK_ARCHITECTURE = 1;                                      /**< Hardware specific identifier */
    static const int SDK_COMPATIBILITY_VERSION = TREDZONE_ENGINE_VERSION_MINOR; /**< SDK minor version */
    static const int SDK_PATCH_VERSION = TREDZONE_ENGINE_VERSION_PATCH;         /**< SDK patch version */
    static const int DEFAULT_EVENT_ALLOCATOR_PAGE_SIZE = 64 * 1024; /**< Default event allocator page size */

    typedef Actor::CoreId CoreId;
    /**
     * @brief Runtime exception that can be thrown when checking the user's binary compatibility with the Tredzone
     * library
     */
    struct RuntimeCompatibilityException : std::exception
    {
        /** @enum Exception cause */
        enum Cause
        {
            E_DEBUG_RELEASE = 1, /**< Error with debug/release compatibility. Non-compatible target type */
            E_COMPILER_VERSION,  /**< Error with compiler version. Non-compatible compiler version */
            E_ENGINE_VERSION     /**< Error with engine version. Non-compatible engine version */
        };

        const Cause cause;
        /** Constructor */
        inline RuntimeCompatibilityException(const Cause cause) noexcept : cause(cause) {}
        /** what() override to show exception cause */
        virtual const char *what() const noexcept
        {
            switch (cause)
            {
            case E_DEBUG_RELEASE:
                return "tredzone::Engine::RuntimeCompatibilityException [debug/release mode used to build current "
                       "binary mismatches TREDZONE RUNTIME]";
            case E_COMPILER_VERSION:
                return "tredzone::Engine::RuntimeCompatibilityException [inconsistent version of C++ compiler "
                       "used to build TREDZONE SDK and the current binary]";
            case E_ENGINE_VERSION:
                return "tredzone::Engine::RuntimeCompatibilityException [the TREDZONE SDK version used to build "
                       "the current binary is incompatible with the TREDZONE RUNTIME used to run the current binary]";
            default:
                return "tredzone::Engine::RuntimeCompatibilityException [?]";
            }
        }
    };
    /**
     * @brief CoreInUseException is thrown if Engine::newCore() method was attempted
     * using a cpu core-id with an event-loop already running.
     */
    struct CoreInUseException : std::exception
    {
        virtual const char *what() const noexcept { return "tredzone::Engine::CoreInUseException"; }
    };
    /**
     * @brief UndersizedException is thrown if the ServiceIndex is full.
     */
    struct UndersizedException : std::bad_alloc
    {
        virtual const char *what() const noexcept { return "tredzone::Engine::UndersizedException"; }
    };
    typedef Property<Actor::Allocator<char>> AsyncProperty;

    /**
     * @brief Vector like container for cpu core-ids.
     */
    class CoreSet
    {
      public:
        typedef Actor::NodeId NodeId;
        /** @brief Thrown when the number of cpu-cores exceeds Actor::MAX_NODE_COUNT. */
        struct TooManyCoresException : std::exception
        {
            virtual const char *what() const noexcept
            {
                return "tredzone::Engine::CoreSet::TooManyCoresException";
            }
        };
        /** @brief Thrown when the provided cpu-core is invalid. */
        struct UndefinedCoreException : std::exception
        {
            virtual const char *what() const noexcept
            {
                return "tredzone::Engine::CoreSet::UndefinedCoreException";
            }
        };

        /** @brief Default constructor */
        CoreSet() noexcept;
        /**
         * @brief If not already present, adds the provided core-id to the core-set.
         * @param coreId core-id to be included in the core-set.
         * @throw TooManyCoresException
         */
        void set(CoreId coreId);
        /**
         * @brief Getter.
         * @param coreId core-id present in the core-set.
         * @return the index (starting 0) of the core-id in the core-set.
         * @throw UndefinedCoreException If the core-id is not present in the core-set.
         */
        NodeId index(CoreId coreId) const;
        /**
         * @brief Getter.
         * @param nodeId index in the core-set.
         * @return The core-id at index nodeId.
         * @throw UndefinedCoreException If nodeId is greater or equal to the core-set current size.
         */
        CoreId at(NodeId nodeId) const;
        /** @brief Return the number of available core-ids. */
        inline size_t size() const noexcept { return coreCount; }

      private:
        size_t coreCount;
        CoreId cores[Actor::MAX_NODE_COUNT];
    };

    /**
     * @brief Specialization of CoreSet, which, at construction, adds all available cpu-cores.
     */
    struct FullCoreSet : CoreSet
    {
        /**
         * @brief Constructor
         * @throw TooManyCoresException If the available number of cpu-cores
         * exceeds Actor::MAX_NODE_COUNT.
         */
        FullCoreSet();
    };

    /**
     * @brief Tredzone Service Actor registry.
     * Any Actor can be declared as a Service from the StartSequence using the addService() method.
     * This Actor is identified by a service-tag.
     * Any Actor during runtime can ask the ServiceIndex for the ActorId corresponding to a given service-tag.
     * @note service-tag must implement <code>static const char* name() throw()</code> public method,
     * or inherit from Service which contains a default name method.
     */
    class ServiceIndex
    {
      public:
        static const int MAX_SIZE = 1024; ///< Maximum number of service-index entries.
                                          /**
                                           * @brief Get the next available ServiceIndex index value.
                                           * @note This method is thread-safe
                                           */
        template <class _Service> static unsigned getIndexValue() noexcept
        {
            static unsigned index = nextStaticIndexValue();
            return index;
        }
        /**
         * @brief Get the ActorId associated to the given service-tag (_Service)
         * @return Found ActorId or null if not found
         */
        template <class _Service>
        const Actor::ActorId &getServiceActorId() const noexcept
        {
            int i = 0;
            for (const unsigned index = getIndexValue<_Service>();
                 i < MAX_SIZE && table[i].actorId != tredzone::null && table[i].index != index; ++i)
            {
            }
            if (i < MAX_SIZE)
            {
                return table[i].actorId;
            }
            return null;
        }
        /**
         * @brief Set Registry value for given service-tag (_Service)
         * @throws UndersizedException is thrown if ServiceIndex has reached the MAX_SIZE value. ie: registry is full.
         */
        template <class _Service>
        void setServiceActorId(const Actor::ActorId &actorId)
        {
            int i = 0;
            unsigned index = getIndexValue<_Service>();
            for (; i < MAX_SIZE && table[i].actorId != tredzone::null && table[i].index != index; ++i)
            {
            }
            if (i == MAX_SIZE)
            {
                throw UndersizedException();
            }
            table[i].index = index;
            table[i].actorId = actorId;
            table[i].name = _Service::name();
        }

      private:
        friend class EngineToEngineConnector;
        struct Table
        {
            unsigned index;
            Actor::ActorId actorId;
            std::string name;
            inline Table() noexcept : index(0) {}
        };

        static Mutex mutex;
        static unsigned staticIndexValue;
        Actor::ActorId null;
        Table table[MAX_SIZE];

        /**
         * @brief Get the next available ServiceIndex index value.
         * @note This method is thread-safe
         */
        inline static unsigned nextStaticIndexValue() noexcept
        {
            Mutex::Lock lock(mutex);
            assert(staticIndexValue != std::numeric_limits<unsigned>::max());
            return staticIndexValue++;
        }
    };

    /**
     * @brief Configuration class used to start the engine.
     *
     * All actors will start in the strict order of their declaration using
     * addActor() and addServiceActor() methods.
     * Actors declared as service (see addServiceActor()) can only be
     * destroyed (called on their Actor::onDestroyRequest() method) after
     * all non-service-actors destruction was completed.
     * This behavior is seamless to the user. Destruction sequence is
     * managed by the engine. Once service-actors starts to be destroyed,
     * their destruction is guaranteed to happen in the strict reverse-order
     * of their declaration.
     * As for non-service-actors, there is no guarantee on their destruction order.
     * <br>A library compatibility check is executed in StartSequence constructor.
     */
    class StartSequence
    {
      public:
        /**
         * @brief Thrown if two Services share the same service-tag. The service-tag must be unique,
         * to the exception of AnonymousService service-tag.
         */
        struct DuplicateServiceException : std::exception
        {
            virtual const char *what() const noexcept
            {
                return "tredzone::Engine::StartSequence::DuplicateServiceException";
            }
        };
        /**
         * @brief Declaring an actor as a service using this tag will exclude that actor from
         * the service-index. However the actor will still be part of the service-workflow.
         * Multiple actors can be declared as service using this service-tag.
         */
        struct AnonymousService : Service
        {
        };

        /**
         * @brief Constructor
         * @param coreSet to be used by the Engine. By default FullCoreSet() is used.
         * @param compatibility check runs by default checkRuntimeCompatibility(true), which runs all checks + compiler
         * version.
         * @throws std::bad_alloc
         */
        StartSequence(const CoreSet & = FullCoreSet(), int = (checkRuntimeCompatibility(true), 0));
        /** @brief Destructor */
        ~StartSequence() noexcept;
        /**
         * @brief Getter.
         * @return A allocator of same type as event-loop local allocator (see Actor::Allocator).
         * @note This is useful, at actor's construction, to pass parameters
         * that can be assignable when using event-loop local allocator.
         * <br>Example:
         * \code
         * class MyActor : public tredzone::Actor {
         * public:
         *     typedef std::vector<int, tredzone::Actor::Allocator<int> > IntVector;
         *     MyActor(const IntVector& intVector) :
         *         intVector(intVector, this->getAllocator()) { // this is possible as the parameter uses the same
         * allocator as the member
         *     }
         *
         * private:
         *     IntVector intVector;
         * };
         *
         * int main() {
         *     tredzone::Engine::StartSequence startSequence;
         *     MyActor::IntVector intVector(startSequence.getAllocator());
         *     intVector.push_back(1);
         *     startSequence.addActor<MyActor>(0, intVector); // a copy of intVector will be passed as parameter to
         * MyActor constructor
         *     tredzone::Engine engine(startSequence);
         *     waitForUserBreak(); // some signal function to wait for the program to be explicitly terminated
         *     return 0;
         * }
         * \endcode
         */
        Actor::AllocatorBase getAllocator() const noexcept;
        /**
         * @brief Set the red-zone properties
         * @param Properties to be set
         */
        void setRedZoneParam(const ThreadRealTimeParam &) noexcept;
        /**
         * @brief Get the red-zone properties
         * @return red-zone properties
         */
        const ThreadRealTimeParam &getRedZoneParam() const noexcept;
        /**
         * @brief Set given core to be in the red-zone
         * @param Core to be set to the red-zone
         * @throws std::bad_alloc
         */
        void setRedZoneCore(CoreId);
        /**
         * @brief Set given core to be in the blue-zone
         * @param Core to be set to the blue-zone
         */
        void setBlueZoneCore(CoreId) noexcept;
        /**
         * @brief Check if the given core is set to be in the red-zone
         * @param Core to check
         * @return true if the core is set in the red-zone. false otherwise.
         */
        bool isRedZoneCore(CoreId) const noexcept;
        /**
         * @brief Add _Actor to the StartSequence. This Actor will be instantiated on Engine start.
         * @param coreId on which the Actor is to be started.
         * @throws std::bad_alloc
         */
        template <class _Actor> void addActor(CoreId);
        /**
         * @brief Add _Actor to the StartSequence. This Actor will be instantiated on Engine start with the given
         * constructor parameter.
         * @param coreId on which the Actor is to be started.
         * @param Actor constructor parameter.
         * @throws std::bad_alloc
         */
        template <class _Actor, class _ActorInit> void addActor(CoreId, const _ActorInit &);
        /**
         * @brief Add _Actor to the StartSequence, identified by the service-tag (_Service).
         * @param coreId on which the Actor is to be started.
         * @throws DuplicateServiceException if the service-tag (_Service) has previously been used to identify an other
         * _Actor
         * @throws std::bad_alloc
         */
        template <class _Service, class _Actor> void addServiceActor(CoreId);
        /**
         * @brief Add _Actor to the StartSequence, identified by the service-tag (_Service).
         * This Actor will be instantiated on Engine start with the given constructor parameter.
         * @param coreId on which the Actor is to be started.
         * @param Actor constructor parameter.
         * @throws DuplicateServiceException if the service-tag (_Service) has previously
         * been used to identify an other _Actor
         * @throws std::bad_alloc
         */
        template <class _Service, class _Actor, class _ActorInit> void addServiceActor(CoreId, const _ActorInit &);
        /**
         * @brief Define a exception handler that is called when an exception in thrown in an unexpected manner.
         * By default AsyncExceptionHandler is used.
         * AsyncExceptionHandler reference is saved, thus it must survive Engine's life-cycle.
         * @param AsyncExceptionHandler specialization
         * @note Important: Any error that end up in this ExceptionHandler should be considered extremely important
         * (fatal)
         * @see AsyncExceptionHandler
         */
        void setExceptionHandler(AsyncExceptionHandler &) noexcept;
        /**
         * @brief Customize CoreActor that is run on every core.
         * EngineCustomCoreActorFactory reference is saved, thus it must survive Engine's life-cycle.
         * @param EngineCustomCoreActorFactory specialization
         */
        void setEngineCustomCoreActorFactory(EngineCustomCoreActorFactory &) noexcept;
        /**
         * @brief Customize EventLoop that is run on every core.
         * EngineCustomEventLoopFactory reference is saved, thus it must survive Engine's life-cycle.
         * @param EngineCustomEventLoopFactory specialization
         */
        void setEngineCustomEventLoopFactory(EngineCustomEventLoopFactory &) noexcept;
        /**
         * @brief Set the default maximum Event page size that is used by the Event Allocator.
         * By default DEFAULT_EVENT_ALLOCATOR_PAGE_SIZE is used.
         * @param size in bytes
         */
        void setEventAllocatorPageSizeByte(size_t) noexcept;
        /**
         * @brief Set the default thread stack size that will be used by the treads created by Tredzone.
         * @param size in bytes
         */
        void setThreadStackSizeByte(size_t) noexcept;
        /**
         * @brief Get a pointer to the previously set exception-handler,
         * using setExceptionHandler().
         * @return pointer to the previously set exception-handler.
         * 0 if no exception-handler was set.
         */
        AsyncExceptionHandler *getAsyncExceptionHandler() const noexcept;
        /**
         * @brief Get a pointer to the previously set core-actor-factory,
         * using setEngineCustomCoreActorFactory().
         * @return pointer to the previously set core-actor-factory.
         * 0 if no core-actor-factory was set.
         */
        EngineCustomCoreActorFactory *getEngineCustomCoreActorFactory() const noexcept;
        /**
         * @brief Get a pointer to the previously set event-loop-factory,
         * using setEngineCustomEventLoopFactory().
         * @return pointer to the previously set event-loop-factory.
         * 0 if no event-loop-factory was set.
         */
        EngineCustomEventLoopFactory *getEngineCustomEventLoopFactory() const noexcept;
        /**
         * @brief Get the current EventAllocatorPageSize in bytes.
         * @return size in bytes
         */
        size_t getEventAllocatorPageSizeByte() const noexcept;
        /**
         * @brief Get the current thread stack size in bytes.
         * @return size in bytes.
         */
        size_t getThreadStackSizeByte() const noexcept;
        /**
         * @brief Get the currently used CoreSet
         * @return currently used CoreSet
         * @throws CoreSet::TooManyCoresException
         */
        CoreSet getCoreSet() const;

        /**
         * @brief Set the Engine's name.
         * The Engine's name is used to identify in a networked cluster.
         * @param Engine name
         */
        void setEngineName(const char *);
        /**
         * @brief Get the Engine name.
         * @return Engine name
         */
        const char *getEngineName() const noexcept;

        template <typename _T>
        void setUserData(_T *data);
        
        /**
         * @brief Set the Engine's suffix.
         * The Engine's suffix is used to identify in a networked cluster. This must be unique.
         * @param Engine suffix
         */
        void setEngineSuffix(const char *);
        /**
         * @brief Get the Engine's suffix.
         * @return Engine suffix
         */
        const char *getEngineSuffix() const noexcept;

      private:
        friend class Engine;
        struct Starter : MultiForwardChainLink<Starter>
        {
            const CoreId coreId;
            const bool isServiceFlag;
            const unsigned serviceIndex;
            Starter(CoreId pcoreId, bool pisServiceFlag, unsigned pserviceIndex = 0)
                : // throw(std::bad_alloc, ...)
                  coreId(pcoreId),
                  isServiceFlag(pisServiceFlag), serviceIndex(pserviceIndex)
            {
                if (pcoreId >= cpuGetCount())
                {
                    std::stringstream sstm;
                    sstm << "Invalid core : " << (int16_t)pcoreId << ". Maximum on this platform is "
                         << cpuGetCount() - 1;
                    throw std::runtime_error(sstm.str());
                }
            }
            virtual ~Starter() noexcept {}
            virtual Actor::ActorId onStart(Engine &, AsyncNode &, const Actor::ActorId &,
                                                bool) const = 0;
        };
        template <class _ActorInit> struct ActorStarter : Starter
        {
            const _ActorInit actorInit;
            ActorStarter(CoreId pcoreId, const _ActorInit &pactorInit, bool pisServiceFlag, unsigned pserviceIndex = 0)
                : // throw(std::bad_alloc, ...)
                  Starter(pcoreId, pisServiceFlag, pserviceIndex),
                  actorInit(pactorInit)
            {
            }
            virtual ~ActorStarter() noexcept {}
        };
        template <class _Actor> struct AddServiceActorWrapper : _Actor
        {
            AddServiceActorWrapper(int) {}
        };
        typedef Starter::ForwardChain<> StarterChain;
        typedef std::list<CoreId> RedZoneCoreIdList;

        const CoreSet coreSet;
        std::unique_ptr<AsyncNodeAllocator> asyncNodeAllocator;
        StarterChain starterChain;
        RedZoneCoreIdList redZoneCoreIdList;
        ThreadRealTimeParam redZoneParam;
        AsyncExceptionHandler *asyncExceptionHandler;
        EngineCustomCoreActorFactory *engineCustomCoreActorFactory;
        EngineCustomEventLoopFactory *engineCustomEventLoopFactory;
        size_t eventAllocatorPageSizeByte;
        size_t threadStackSizeByte;
        std::string engineName;
        std::string engineSuffix;
        
        void    *m_UserData;
    };

    /**
     * @brief Default constructor
     * @param startSequence start-sequence containing actors to be started.
     * @throw std::bad_alloc
     * @throw ? Any exception thrown by actors' construction during start-sequence execution.
     */
    Engine(const StartSequence &startSequence);
    /**
     * @brief Default destructor
     * @attention When engine's destruction is called, the engine enters shutdown-mode.
     * All existing actors are marked for destruction, and no new actor can be created.
     */
    ~Engine() noexcept;

    /**
     * @brief Check if the linked Tredzone library is compatible
     * This checks debug/release, compiler version and engine version
     *
     * @param checkCompiler default is true to check compiler version
     * @throws RuntimeCompatibilityException if the linked library is not compatible
     */
    inline static void checkRuntimeCompatibility(bool checkCompiler = true)
    {
#ifndef NDEBUG
        bool isNDEBUG = false;
#else
        bool isNDEBUG = true;
#endif

        // check compiler version compatibility
        if (checkCompiler)
        {
            TREDZONE_SDK_COMPILER_VERSION_type compilerVersion;
            if (!TREDZONE_SDK_IS_COMPILER_COMPATIBLE(TREDZONE_COMPILER_ID, &compilerVersion))
            {
                throw RuntimeCompatibilityException(RuntimeCompatibilityException::E_COMPILER_VERSION);
            }
        }
        // check sdk target (debug/release) compatibility
        if (!TREDZONE_SDK_IS_DEBUG_RELEASE_COMPATIBLE(isNDEBUG))
        {
            throw RuntimeCompatibilityException(RuntimeCompatibilityException::E_DEBUG_RELEASE);
        }
    }
    /**
     * @brief Get a string representing the Engine's version.
     * This includes:
     * - debug/release
     * - Linux/Apple/Windows
     * - compiler version
     * - engine version
     * - hardware architecture
     *
     * @return string
     */
    static std::string getVersion();
    /**
     * @brief Get the Engine's name
     * @return Engine name
     */
    inline const char *getEngineName() const noexcept { return engineName.c_str(); }
    /**
     * @brief Get the Engine's suffix
     * @return Engine suffix
     */
    inline const char *getEngineSuffix() const noexcept { return engineSuffix.c_str(); }
    /**
     * @brief Get a reference to the ServiceIndex.
     * @return ServiceIndex reference.
     */
    inline const ServiceIndex &getServiceIndex() const noexcept { return serviceIndex; }
    /**
     * @brief Get the CoreSet.
     * @return CoreSet
     */
    const CoreSet &getCoreSet() const noexcept;
    /**
     * @brief Get the EventAllocatorPageSize in bytes
     * @return EventAllocatorPageSize
     */
    size_t getEventAllocatorPageSizeByte() const noexcept;
    /**
     * @brief Start a new Actor on a given CoreId
     * @attention This method is used to start a new event-loop after engine's start.
     * Therefore no event-loop must already be running on the targeted cpu core-id.
     * @param coreId coreId on which to
     * @param isRedZone if true, new thread will be started in red-zone.
     * @return ActorId Created _Actor's ActorId
     * @throw CoreInUseException if the given CoreId is already in use.
     * @throw std::bad_alloc
     * @throw ? Any other exception possibly thrown depending on _Actor (the template generic type)
     * constructor call.
     */
    template <class _Actor> inline Actor::ActorId newCore(CoreId, bool isRedZone);
    /**
     * @see newCore(CoreId, bool)
     */
    template <class _Actor, class _ActorInit>
    inline Actor::ActorId newCore(CoreId, bool isRedZone, const _ActorInit &);

#ifndef NDEBUG
    /**
     * @brief If called, this will activate the memory leak backtrace. By default this is disabled.
     * @note Method only available in debug mode
     * @note This method has an important impact on performance.
     * @attention On Linux back-trace is resolved using backtrace_symbols() (see man page)
     * which requires -rdynamic GNU linker option to properly resolve symbols.
     */
    static void debugActivateMemoryLeakBacktrace() noexcept;
#endif

    inline static Engine &getEngine() noexcept
    {
        assert(currentEngineTLS.get() != 0);
        return *currentEngineTLS.get();
    }
    
    void *getUserData(void) const noexcept;

  private:
    friend class Actor;
    friend class EngineToEngineConnector;
    friend class EngineEventLoop;
    struct NewCoreStarter
    {
        virtual ~NewCoreStarter() noexcept {}
        virtual Actor::ActorId start(AsyncNode &) = 0;
    };
    template <class _Actor, class _ActorInit> class ServiceActorWrapper;
    struct ServiceDestroyEvent : Actor::Event
    {
    };
    class ServiceSingletonActor;
    class CoreActor;

    char cacheLineHeaderPadding[CACHE_LINE_SIZE - 1];
    const std::string engineName;
    const std::string engineSuffix;
    const size_t threadStackSizeByte;
    const ThreadRealTimeParam redZoneParam;
    CacheLineAlignedObject<unsigned> regularActorsCoreCount;
    std::unique_ptr<EngineCustomCoreActorFactory> defaultCoreActorFactory;
    std::unique_ptr<EngineCustomEventLoopFactory> defaultEventLoopFactory;
    std::unique_ptr<AsyncNodeManager> nodeManager;
    ServiceIndex serviceIndex;
    EngineCustomCoreActorFactory &customCoreActorFactory;
    EngineCustomEventLoopFactory &customEventLoopFactory;
    static ThreadLocalStorage<Engine *> currentEngineTLS;
    char cacheLineTrailerPadding[CACHE_LINE_SIZE - 1];

    void start(const StartSequence &); // throw (std::bad_alloc, ...)
    void finish() noexcept;
    static void threadStartHook(void *);
    Actor::ActorId newCore(CoreId, bool isRedZone, NewCoreStarter &); // throw (std::bad_alloc, CoreInUseException, ...)
    void    *m_UserData;
};

class Engine::ServiceSingletonActor : public Actor
{
  public:
    typedef std::list<Actor *, Allocator<Actor *>> ServiceActorList;

    inline ServiceSingletonActor() noexcept : serviceActorList(getAllocator()), isServiceDestroyTimeFlag(true) {}
    virtual ~ServiceSingletonActor() noexcept { assert(serviceActorList.empty()); }
    inline void registerServiceActor(Actor &actor)
    {
        serviceActorList.remove(&actor);
        serviceActorList.push_back(&actor);
    }
    inline void unregisterServiceActor(Actor &actor) noexcept { serviceActorList.remove(&actor); }
    inline bool isServiceDestroyTime() noexcept { return isServiceDestroyTimeFlag; }
    const ServiceActorList &getServiceActorList() const noexcept { return serviceActorList; }

  private:
    friend class CoreActor;
    ServiceActorList serviceActorList;
    bool isServiceDestroyTimeFlag;

    void destroyServiceActors() noexcept
    {
        assert(!isServiceDestroyTimeFlag);
        isServiceDestroyTimeFlag = true;
        for (ServiceActorList::iterator i = serviceActorList.begin(), endi = serviceActorList.end(); i != endi; ++i)
        {
            (*i)->requestDestroy();
        }
    }
};

template <class _Actor, class _ActorInit>
class Engine::ServiceActorWrapper : virtual public ActorBase, public _Actor
{
public:

    inline ServiceActorWrapper(AsyncNode &pasyncNode, const _ActorInit &actorInit,
                               const Actor::ActorId &ppreviousServiceDestroyActorId, bool plastServiceFlag)
        : ActorBase(&pasyncNode), _Actor(actorInit),
          serviceSingletonActor(static_cast<Actor *>(this)->newReferencedSingletonActor<ServiceSingletonActor>()),
          destroyEventActor(static_cast<Actor *>(this)->newReferencedActor<DestroyEventActor>(
              std::make_pair(this, &ppreviousServiceDestroyActorId))),
          lastServiceFlag(plastServiceFlag)
    {
        serviceSingletonActor->registerServiceActor(*this);
    }
        
    virtual ~ServiceActorWrapper() noexcept
    {
        serviceSingletonActor->unregisterServiceActor(*this);
        destroyEventActor->serviceActor = 0;
    }
    inline void operator delete(void *p) noexcept
    {
        Actor::Allocator<ServiceActorWrapper>(
            Actor::AllocatorBase(*static_cast<ServiceActorWrapper *>(p)->Actor::asyncNode))
            .deallocate(static_cast<ServiceActorWrapper *>(p), 1);
    }
    inline void operator delete(void *, void *p) noexcept
    {
        Actor::Allocator<ServiceActorWrapper>(
            Actor::AllocatorBase(*static_cast<ServiceActorWrapper *>(p)->Actor::asyncNode))
            .deallocate(static_cast<ServiceActorWrapper *>(p), 1);
    }

private:
    
    friend class StartSequence;
    
    // 
    struct DestroyEventActor : public Actor
    {
    public:
        ServiceActorWrapper *serviceActor;
        const Actor::ActorId previousServiceDestroyActorId;
        /**
         * throw (std::bad_alloc)
         */
        DestroyEventActor(const std::pair<ServiceActorWrapper *, const Actor::ActorId *> &init)
            : serviceActor(init.first), previousServiceDestroyActorId(*init.second)
        {
            registerEventHandler<ServiceDestroyEvent>(*this);
        }
        void onEvent(const ServiceDestroyEvent &) noexcept
        {
            assert(serviceActor != 0);
            if (serviceActor != 0)
            {
                assert(!serviceActor->lastServiceFlag);
                serviceActor->lastServiceFlag = true;
                serviceActor->requestDestroy();
            }
        }
        
        virtual void onDestroyRequest() noexcept
        {
            if (previousServiceDestroyActorId != tredzone::null)
            {
                try
                {
                    Event::Pipe(*this, previousServiceDestroyActorId).push<ServiceDestroyEvent>();
                    Actor::onDestroyRequest();
                }
                catch (std::bad_alloc &)
                {
                    requestDestroy();
                }
            }
            else
            {
                Actor::onDestroyRequest();
            }
        }
    };

    Actor::ActorReference<ServiceSingletonActor> serviceSingletonActor;
    Actor::ActorReference<DestroyEventActor> destroyEventActor;
    bool lastServiceFlag;

    const Actor::ActorId &getDestroyEventActorId() const noexcept { return destroyEventActor->getActorId(); }
    virtual void onDestroyRequest() noexcept
    {
        if (serviceSingletonActor->isServiceDestroyTime() && lastServiceFlag)
        {
            _Actor::onDestroyRequest();
        }
    }
};

template <class _Service> struct EngineStartSequenceServiceTraits
{
    static inline bool isAnonymous() noexcept { return false; }
};

template <> struct EngineStartSequenceServiceTraits<Engine::StartSequence::AnonymousService>
{
    static inline bool isAnonymous() noexcept { return true; }
};

template <class _Actor> Actor::ActorId Engine::newCore(CoreId coreId, bool isRedZone)
{
    struct NewActorCoreStarter : NewCoreStarter
    {
		virtual Actor::ActorId start(AsyncNode& node)
        {
            _Actor& actor = Actor::newActor<_Actor>(node);

            TraceREF(&node, __func__, "-1.-1", cppDemangledTypeInfoName(typeid(*this)), actor.actorId, cppDemangledTypeInfoName(typeid(actor)))

			return actor.getActorId();
		}
	};
    NewActorCoreStarter newActorCoreStarter;
    return newCore(coreId, isRedZone, newActorCoreStarter);
}

template <class _Actor, class _ActorInit>
Actor::ActorId Engine::newCore(CoreId coreId, bool isRedZone, const _ActorInit &actorInit)
{
    struct NewActorCoreStarter : NewCoreStarter
    {
        const _ActorInit &actorInit;
        inline NewActorCoreStarter(const _ActorInit &pactorInit) noexcept : actorInit(pactorInit) {}
        virtual Actor::ActorId start(AsyncNode &node)
        {
			_Actor& actor = Actor::newActor<_Actor>(node, actorInit);
            
            TraceREF(&node, __func__, "-1.-1", cppDemangledTypeInfoName(typeid(*this)), actor.actorId, cppDemangledTypeInfoName(typeid(actor)))
            
			return actor.getActorId();
		}
    };
    NewActorCoreStarter newActorCoreStarter(actorInit);
    return newCore(coreId, isRedZone, newActorCoreStarter);
}

template <class _Actor> void Engine::StartSequence::addActor(CoreId coreId)
{
    struct AddActorStarter : Starter
    {
        AddActorStarter(CoreId coreId) : Starter(coreId, false) {}
        virtual ~AddActorStarter() noexcept {}
        virtual Actor::ActorId onStart(Engine &, AsyncNode &node, const Actor::ActorId &, bool) const
        {
			_Actor  &actor = Actor::newActor<_Actor>(node);
            (void)actor;
            
            TraceREF(&node, __func__, "-1.-1", cppDemangledTypeInfoName(typeid(*this)), actor.actorId, cppDemangledTypeInfoName(typeid(actor)))
            
            return Actor::ActorId();
		}
    };
    starterChain.push_back(new AddActorStarter(coreId));
}

template <class _Actor, class _ActorInit>
void Engine::StartSequence::addActor(CoreId coreId, const _ActorInit &actorInit)
{
    struct AddActorStarter : ActorStarter<_ActorInit>
    {
        AddActorStarter(CoreId coreId, const _ActorInit &pactorInit)
            : ActorStarter<_ActorInit>(coreId, pactorInit, false)
        {
        }
        virtual ~AddActorStarter() noexcept {}
        
        virtual Actor::ActorId onStart(Engine&, AsyncNode& node, const Actor::ActorId&, bool) const
        {
            _Actor  &actor = Actor::newActor<_Actor, _ActorInit>(node, this->actorInit);
            (void)actor;
            
            TraceREF(&node, __func__, "-1.-1", cppDemangledTypeInfoName(typeid(*this)), actor.actorId, cppDemangledTypeInfoName(typeid(actor)))
            
			return Actor::ActorId();
		}
    };
    starterChain.push_back(new AddActorStarter(coreId, actorInit));
}

template <class _Service, class _Actor> void Engine::StartSequence::addServiceActor(CoreId coreId)
{
    addServiceActor<_Service, AddServiceActorWrapper<_Actor>>(coreId, 0);
}

template <class _Service, class _Actor, class _ActorInit>
void Engine::StartSequence::addServiceActor(CoreId coreId, const _ActorInit &actorInit)
{
    struct AddServiceActorStarter : ActorStarter<_ActorInit>
    {
        AddServiceActorStarter(CoreId coreId, const _ActorInit &actorInit, unsigned pserviceIndex)
            : ActorStarter<_ActorInit>(coreId, actorInit, true, pserviceIndex)
        {
        }
        virtual ~AddServiceActorStarter() noexcept {}
        virtual Actor::ActorId onStart(Engine &engine, AsyncNode &asyncNode,
                                            const Actor::ActorId &previousServiceDestroyActorId,
                                            bool isLastService) const
        {
            ServiceActorWrapper<_Actor, _ActorInit> &serviceActor = *new (
                Actor::Allocator<ServiceActorWrapper<_Actor, _ActorInit>>(Actor::AllocatorBase(asyncNode))
                    .allocate(1)) ServiceActorWrapper<_Actor, _ActorInit>(asyncNode, this->actorInit,
                                                                          previousServiceDestroyActorId, isLastService);
            if (EngineStartSequenceServiceTraits<_Service>::isAnonymous() == false)
            {
                assert(engine.serviceIndex.getServiceActorId<_Service>() == tredzone::null);
                engine.serviceIndex.setServiceActorId<_Service>(serviceActor.getActorId());
            }
            return serviceActor.getDestroyEventActorId();
        }
    };
    unsigned serviceIndex = ServiceIndex::getIndexValue<_Service>();
    if (EngineStartSequenceServiceTraits<_Service>::isAnonymous() == false)
    {
        serviceIndex = ServiceIndex::getIndexValue<_Service>();
        for (StarterChain::iterator i = starterChain.begin(), endi = starterChain.end(); i != endi; ++i)
        {
            if (i->isServiceFlag && i->serviceIndex == serviceIndex)
            {
                throw DuplicateServiceException();
            }
        }
    }
    starterChain.push_back(new AddServiceActorStarter(coreId, actorInit, serviceIndex));
}

} // namespace
