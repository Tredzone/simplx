/**
 * @file actor.h
 * @brief actor class
 * @copyright 2013-2018 Tredzone (www.tredzone.com). All rights reserved.
 * Please see accompanying LICENSE file for licensing terms.
 */

#pragma once

#include <vector>

#include "trz/engine/internal/cacheline.h"
#include "trz/engine/internal/intrinsics.h"
#include "trz/engine/internal/mdoublechain.h"
#include "trz/engine/internal/mforwardchain.h"
#include "trz/engine/internal/property.h"
#include "trz/engine/internal/serialbufferchain.h"
#include "trz/engine/internal/stringstream.h"
#include "trz/engine/internal/thread.h"

namespace tredzone
{

class AsyncNode;
struct AsyncNodeBase;
class AsyncNodesHandle;
class AsyncNodeAllocator;
class AsyncEngine;
class AsyncEngineEventLoop;
class AsyncExceptionHandler;
class AsyncActor;
typedef AsyncActor Actor;
class AsyncEngineToEngineConnector;
class AsyncEngineToEngineSerialConnector;
class AsyncEngineToEngineSharedMemoryConnector;
class AsyncEngineToEngineConnectorEventFactory;

struct FeatureNotImplementedException : std::exception
{
    const char *what() const noexcept override
    { // feature is not (yet) implemented!
        return "tredzone::FeaturedNotImplementedException";
    }
};

/**
 * @fn void breakThrow(const std::exception &e) throw()
 * @brief exception-throwing wrapper
 * @param e std::exception
 * @note optionally break to debugger before throwing exception
 */
inline void breakThrow(const std::exception &e) throw()
{
    TRZ_DEBUG_BREAK();

    throw e;
}

template <class> class Accessor;

struct AsynActorBase
{
  public:
    AsynActorBase() : asyncNode(0) { breakThrow(std::runtime_error("illegal direct base instantiation")); }

  private:
    friend class AsyncActor;
    friend class AsyncEngineToEngineConnector;
    friend class AsyncEngineToEngineSharedMemoryConnector;
    friend class AsyncEngine;

    AsyncNode *asyncNode;
    AsynActorBase(AsyncNode *_asyncNode) : asyncNode(_asyncNode) {}
};

/**
 * @brief Base class for actors processed by AsyncEngine.
 *
 * This class and its derived classes can only be instantiated using factory methods found
 * in this class (using newReferencedActor(), newReferencedSingletonActor(), newUnreferencedActor())
 * and in AsyncEngine::StartSequence.
 */
class AsyncActor : private MultiDoubleChainLink<AsyncActor, 2u>, virtual private AsynActorBase
{
  public:
    static const int MAX_NODE_COUNT =
        255; ///< Maximum number of parallel event-loops, with one event-loop per cpu-core.
    static const int MAX_EVENT_ID_COUNT = 4096; ///< Maximum number of classes derived from AsyncActor::Event that can
                                                ///actually be instantiated at run-time.

    /**
     * @brief This exception is thrown when actor A attempts to get a reference to actor B,
     * where B already has a direct or indirect reference to A.
     *
     * A direct reference to actor C means an access to an instance of AsyncActor::ActorReference
     * referencing actor C. Whilst an indirect reference to actor C means an access to
     * an instance of AsyncActor::ActorReference referencing actor D, where actor D has access
     * to an instance of AsyncActor::ActorReference referencing actor C.
     */
    struct CircularReferenceException : std::exception
    {
        virtual const char *what() const noexcept { return "tredzone::AsyncActor::CircularReferenceException"; }
    };

  private:
    struct EventBase;
    friend class AsyncEngineToEngineConnectorEventFactory;

    class ActorReferenceBase : public MultiDoubleChainLink<ActorReferenceBase>
    {
      public:
        typedef ActorReferenceBase::DoubleChain<> Chain;

        inline ActorReferenceBase() noexcept : referenceChain(0), referencedActor(0) {}
        inline ActorReferenceBase(const ActorReferenceBase &other) noexcept
            : MultiDoubleChainLink<ActorReferenceBase>(),
              destroyFlag(other.destroyFlag),
              referenceChain(other.referenceChain),
              referencedActor(other.referencedActor)
        {
            if (referenceChain != 0)
            {
                referenceChain->push_back(this);
            }
            if (referencedActor != 0)
            {
                assert(referencedActor->referenceFromCount > 0);
                ++(referencedActor->referenceFromCount);
            }
        }

        /**
         * throw (CircularReferenceException)
         */
        inline ActorReferenceBase(AsyncActor &referencingActor, AsyncActor &preferencedActor, bool pdestroyFlag)
            : destroyFlag(pdestroyFlag), referenceChain(&referencingActor.referenceToChain),
              referencedActor(&preferencedActor)
        {
            if (recursiveFind(referencingActor, *referencedActor))
            {
                throw CircularReferenceException();
            }
            referenceChain->push_back(this);
            ++(referencedActor->referenceFromCount);
        }
        inline ~ActorReferenceBase() noexcept { unreference(); }
        inline ActorReferenceBase &operator=(const ActorReferenceBase &other) noexcept
        {
            unreference();
            destroyFlag = other.destroyFlag;
            referenceChain = other.referenceChain;
            referencedActor = other.referencedActor;
            if (referenceChain != 0)
            {
                referenceChain->push_back(this);
            }
            if (referencedActor != 0)
            {
                assert(referencedActor->referenceFromCount > 0);
                ++(referencedActor->referenceFromCount);
            }
            return *this;
        }
        inline AsyncActor *getReferencedActor() const noexcept { return referencedActor; }
        inline void unreference() noexcept
        {
            if (referenceChain != 0)
            {
                referenceChain->remove(this);
                referenceChain = 0;
            }
            if (referencedActor != 0)
            {
                assert(referencedActor->referenceFromCount > 0);
                if (--(referencedActor->referenceFromCount) == 0 &&
                    (destroyFlag || referencedActor->onUnreferencedDestroyFlag))
                {
                    referencedActor->requestDestroy();
                }
                referencedActor = 0;
            }
        }
        inline void unchain(Chain &
#ifndef NDEBUG
                                debugChain
#endif
                            ) noexcept
        {
            assert(referenceChain == &debugChain);
            referenceChain->remove(this);
            referenceChain = 0;
        }

      private:
        bool destroyFlag;
        Chain *referenceChain;
        AsyncActor *referencedActor;

        inline static bool recursiveFind(const AsyncActor &referencedActor, const AsyncActor &referencingActor) noexcept
        {
            if (&referencedActor == &referencingActor)
            {
                return true;
            }
            Chain::const_iterator i = referencingActor.referenceToChain.begin(),
                                  endi = referencingActor.referenceToChain.end();
            for (; i != endi && !recursiveFind(referencedActor, *i->getReferencedActor()); ++i)
            {
            }
            return i != endi;
        }
    };

  public:
    typedef uint8_t CoreId; ///< Scalar type representing a physical cpu-core of a CPU, its value range is arbitrary
                            ///(hardware dependent).
    typedef uint8_t NodeId; ///< Scalar type representing a logical cpu-core of a CPU, its value range is compact (+1
                            ///increment) starting from 0. There is a one-to-one relationship between NodeId and CoreId.
    typedef uint64_t NodeActorId; ///< Scalar type representing the actor-id within an event-loop (there is one
                                  ///event-loop per cpu-core). Valid values start at 1 (0 is invalid).
    class Event;
    typedef uint16_t EventId; ///< Scalar type representing an event-class during run-time. An event-class is derived
                              ///from AsyncActor::Event.
    struct ReturnToSenderException;
    struct UndersizedException;
    struct ShutdownException;
    struct EventTable;
    struct NodeConnection;

    /**
     * @brief This exception is thrown when attempting multiple calls to
     * registerEventHandler() using the same set of arguments (both template and function type arguments).
     * The same goes with registerUndeliveredEventHandler().
     *
     * Using the same arguments is legal only if the event-handler was unregistered
     * (using unregisterEventHandler(), unregisterUndeliveredEventHandler(), unregisterAllEventHandlers())
     * before the next register attempt.
     */
    struct AlreadyRegisterdEventHandlerException : std::exception
    {
        virtual const char *what() const noexcept
        {
            return "tredzone::AsyncActor::AlreadyRegisterdEventHandlerException";
        }
    };
    /**
     * @brief This exception is thrown when attempting to instantiate an actor while the engine is being shutdown.
     *
     * The engine (instance of AsyncEngine) goes into shutdown mode when it is being destroyed (call to destructor).
     */
    struct ShutdownException : std::exception
    {
        const bool engineShutdownflag;
        inline ShutdownException(bool pengineShutdownflag) noexcept : engineShutdownflag(pengineShutdownflag) {}
        virtual const char *what() const noexcept
        {
            if (engineShutdownflag)
            {
                return "tredzone::AsyncActor::ShutdownException (the engine is terminating)";
            }
            else
            {
                return "tredzone::AsyncActor::ShutdownException (the core is terminating)";
            }
        }
    };
    /**
     * @brief This exception is thrown when attempting to instantiate a reference (AsyncActor::ActorReference)
     * to an actor using its actor-id cannot be achieved.
     *
     * This occurs in 2 cases:
     * -# The actor does not exist anymore.
     * -# The actor is processed by a difference event-loop
     * (in other words the actor we are attempting to reference is on a different cpu-core).
     */
    struct ReferenceLocalActorException : std::exception
    {
        const bool notLocalFlag;
        inline ReferenceLocalActorException(bool pnotLocalFlag) noexcept : notLocalFlag(pnotLocalFlag) {}
        virtual const char *what() const noexcept
        {
            if (notLocalFlag)
            {
                return "tredzone::AsyncActor::ReferenceLocalActorException (the actor belongs to a different core)";
            }
            else
            {
                return "tredzone::AsyncActor::ReferenceLocalActorException (the actor doesn't exist anymore)";
            }
        }
    };
    /**
     * @brief Non-template base-class of stl-compliant AsyncActor::Allocator.
     *
     * This is an event-loop (cpu-core) local allocator construct, with no global context.<br>
     * It is not thread-safe. Although a default constructor exists for stl-compliance,
     * an allocation attempt using a default-constructed instance will always throw an std::bad_alloc exception.<br>
     * A usable instance of this class can only be obtained from the AsyncActor::getAllocator() factory-method.
     */
    class AllocatorBase
    {
      public:
        /**
         * @brief Default constructor.
         * @note As explained in the class-description, this constructor should not be used.
         */
        inline AllocatorBase() noexcept : asyncNodeAllocator(0)
        { // Must never be used, allocation will always fail with bad_alloc exception
        }
        /**
         * @brief Constructor with event-loop context.
         * @param asyncNodeAllocator event-loop context.
         * @note event-loop context cannot be obtained directly.
         * Therefore, this constructor cannot be called directly.
         * Instead use, the AsyncActor::getAllocator() factory-method.
         */
        inline AllocatorBase(AsyncNodeAllocator &asyncNodeAllocator) noexcept : asyncNodeAllocator(&asyncNodeAllocator)
        {
        }
        /**
         * @brief Constructor with event-loop context.
         * @param asyncNode event-loop context.
         * @note event-loop context cannot be obtained directly.
         * Therefore, this constructor cannot be called directly.
         * Instead use, the AsyncActor::getAllocator() factory-method.
         */
        AllocatorBase(AsyncNode &asyncNode) noexcept;
        /**
         * @brief Compare 2 allocators based on there event-loop contexts.<br>
         * Two allocators with the same event-loop contexts can be used indifferently
         * with respect to allocation/deallocation operations.
         * @param other reference to another allocator instance.
         * @return true if the 2 allocators have the same event-loop context.
         */
        inline bool operator==(const AllocatorBase &other) const noexcept
        {
            return asyncNodeAllocator == other.asyncNodeAllocator;
        }
        /**
         * @brief Compare 2 allocators based on there event-loop contexts.<br>
         * Two allocators with the same event-loop contexts can be used indifferently
         * with respect to allocation/deallocation operations.
         * @param other reference to another allocator instance.
         * @return true if the 2 allocators have different event-loop contexts.
         */
        inline bool operator!=(const AllocatorBase &other) const noexcept
        {
            return asyncNodeAllocator != other.asyncNodeAllocator;
        }

#ifndef NDEBUG
        /**
         * @brief A debug-mode method to assert thread-safety.
         * This method is only available in debug (NDEBUG macro not defined).
         * Therefore calling this method must be conditioned to:
         * \code
         * #ifndef NDEBUG
         * // in here call to debugGetThreadId() is valid
         * #endif
         * \endcode
         * This condition is also valid within assert() and TRZ_DEBUG() arguments' scope.
         * For instance:<br>
         * \code
         * assert(allocator.debugGetThreadId() == ThreadId::current());
         * \endcode
         * \code
         * TRZ_DEBUG(
         *     if (allocator.debugGetThreadId() != ThreadId::current()) {
         *         doSomeThing();
         *         exit(-1);
         *     }
         * )
         * \endcode
         * @return A reference to the event-loop thread in which it is safe to use this allocator instance.
         */
        const ThreadId &debugGetThreadId() const noexcept;
#endif

      protected:
        /**
         * @brief Allocates a memory bloc in the event-loop dedicated memory.
         * @param sz size (byte count) to be allocated
         * @param hint not used (for stl-compliance)
         * @return A pointer to the allocated memory
         * @throw std::bad_alloc
         */
        void *allocate(size_t sz, const void *hint = 0);
        /**
         * @brief Deallocates a memory bloc previously allocated in the event-loop dedicated memory.
         * @param sz size (byte count) to be deallocated
         * @param p pointer to the memory bloc to be deallocated
         */
        void deallocate(size_t sz, void *p) noexcept;

      private:
        friend class AsyncActor;
        friend class AsyncEngineToEngineConnectorEventFactory;
        AsyncNodeAllocator *const asyncNodeAllocator;
    };
    /**
     * @brief STL-compliant allocator template based on AsyncActor::AllocatorBase.
     *
     * This is a nested template class. Therefore, there is no Allocator<void> specialization,
     * as nested template class specialization is forbidden by the language.
     */
    template <class T> class Allocator : public AllocatorBase
    {
      public:
        typedef T value_type;              ///< STL-compliant
        typedef size_t size_type;          ///< STL-compliant
        typedef ptrdiff_t difference_type; ///< STL-compliant
        typedef T *pointer;                ///< STL-compliant
        typedef const T *const_pointer;    ///< STL-compliant
        typedef T &reference;              ///< STL-compliant
        typedef const T &const_reference;  ///< STL-compliant

        /**
         * @brief STL-compliant.
         */
        template <class U> struct rebind
        {
            typedef Allocator<U> other; ///< STL-compliant
        };

        /**
         * @brief Default constructor which follows the same rules as parent default constructor
         * AllocatorBase::AllocatorBase(). This constructor should not be used, and is only there for stl-compliance.
         */
        inline Allocator() noexcept {}
        /**
         * @brief Default copy constructor.
         * @param other allocator to copy event-loop context from.
         */
        inline Allocator(const AllocatorBase &other) noexcept : AllocatorBase(other) {}
        /**
         * @brief STL-compliant.
         */
        inline pointer address(reference r) const { return &r; }
        /**
         * @brief STL-compliant.
         */
        inline const_pointer address(const_reference r) const { return &r; }
        /**
         * @brief Allocates an array of T entry-type in the event-loop dedicated memory.
         * @param n entry count in the array to be allocated
         * @param hint not used (for stl-compliance)
         * @return A pointer to the allocated array
         * @throw std::bad_alloc
         */
        inline pointer allocate(size_type n, const void *hint = 0)
        {
            return static_cast<pointer>(AllocatorBase::allocate(n * sizeof(T), hint));
        }
        /**
         * @brief Deallocates an array of T entry-type previously allocated in the event-loop dedicated memory.
         * @param n entry count in the array to be deallocated
         * @param p pointer to the array to be deallocated
         */
        inline void deallocate(pointer p, size_type n) noexcept { AllocatorBase::deallocate(n * sizeof(T), p); }

/**
 * @brief STL-compliant.
 */
#ifdef TREDZONE_CPP11_SUPPORT
        template <class U, class... Args> void construct(U *p, Args &&... args)
        {
            ::new ((void *)p) U(std::forward<Args>(args)...);
        }
#else
        inline void construct(pointer p, const T &init) { new (p) T(init); }
#endif

        /**
         * @brief STL-compliant.
         */
        inline void destroy(pointer p) { p->~T(); }
        /**
         * @brief STL-compliant.
         * @return The maximum memory model limit (e.g. 2^32 for 32-bit, 2^64 for 64 bit)
         */
        inline size_type max_size() const noexcept { return std::numeric_limits<size_type>::max(); }

      private:
        inline Allocator(AsyncNode &pasyncNode) noexcept : AllocatorBase(pasyncNode) {}
    };
    /**
     * @brief Smart-pointer placeholder to an actor reference.
     *
     * Any existing instance of this class with non-NULL reference to an actor will
     * prevent that actor from being destroyed.
     * An ActorReference instance retains a valid actor-reference from one of the following AsyncActor methods:
     * -# referenceLocalActor()
     * -# newReferencedActor()
     * -# newReferencedSingletonActor()
     */
    template <class _AsyncActor> class ActorReference : private ActorReferenceBase
    {
      public:
        /**
         * @brief Default constructor, which initializes the internal actor-reference to NULL.
         */
        inline ActorReference() noexcept {}
        /**
         * @brief Copy constructor, which initializes the internal actor-reference from another ActorReference instance.
         * @param other another instance to initialize the internal actor-reference from.
         */
        inline ActorReference(const ActorReference &other) noexcept : ActorReferenceBase(other) {}
        /**
         * @brief Copy constructor, which initializes the internal actor-reference from another ActorReference instance,
         * and performs a dynamic_cast<_AsyncActor>.
         * @param other another instance to initialize the internal actor-reference from.
         * @throw std::bad_cast if cast of actor-reference from _OtherAsyncActor to _AsyncActor fails.
         */
        template <class _OtherAsyncActor>
        inline ActorReference(const ActorReference<_OtherAsyncActor> &other) : ActorReferenceBase(other)
        {
            AsyncActor *referencedActor = getReferencedActor();
            if (referencedActor != 0 && dynamic_cast<_OtherAsyncActor *>(referencedActor) == 0)
            {
                throw std::bad_cast();
            }
        }
        /**
         * @brief Assigns the internal actor-reference from another ActorReference instance.
         * Any pre-existing actor-reference is first released.
         * @param other another instance to assign the internal actor-reference from.
         * @return A reference to this instance.
         */
        inline ActorReference &operator=(const ActorReference &other) noexcept
        {
            ActorReferenceBase::operator=(other);
            return *this;
        }
        /**
         * @brief Set to NULL the internal actor-reference from another ActorReference instance.
         * Any pre-existing actor-reference is first released.
         */
        inline void reset() noexcept { ActorReferenceBase::operator=(ActorReferenceBase()); }
        /**
         * @brief Dereferences the internal actor-reference.
         * No reference validity checked is performed.
         * @attention The internal actor-reference must be valid (not NULL).
         * It can be first checked using the get() method.
         * Calling this method without knowing the internal actor-reference is valid
         * can lead to unexpected behavior.
         * @return A reference to a non-NULL internal actor-reference.
         */
        inline _AsyncActor &operator*() noexcept
        {
            assert(getReferencedActor() != 0);
            return static_cast<_AsyncActor &>(*getReferencedActor());
        }
        /**
         * @brief Dereferences the internal actor-reference.
         * No reference validity checked is performed.
         * @attention The internal actor-reference must be valid (not NULL).
         * It can be first checked using the get() method.
         * Calling this method without knowing the internal actor-reference is valid
         * can lead to unexpected behavior.
         * @return A const reference to a non-NULL internal actor-reference.
         */
        inline const _AsyncActor &operator*() const noexcept
        {
            assert(getReferencedActor() != 0);
            return static_cast<const _AsyncActor &>(*getReferencedActor());
        }
        /**
         * @brief Dereferences the internal actor-reference.
         * No reference validity checked is performed.
         * @attention The internal actor-reference must be valid (not NULL).
         * It can be first checked using the get() method.
         * Calling this method without knowing the internal actor-reference is valid
         * can lead to unexpected behavior.
         * @return A pointer to a non-NULL internal actor-reference.
         */
        inline _AsyncActor *operator->() noexcept
        {
            assert(getReferencedActor() != 0);
            return static_cast<_AsyncActor *>(getReferencedActor());
        }
        /**
         * @brief Dereferences the internal actor-reference.
         * No reference validity checked is performed.
         * @attention The internal actor-reference must be valid (not NULL).
         * It can be first checked using the get() method.
         * Calling this method without knowing the internal actor-reference is valid
         * can lead to unexpected behavior.
         * @return A const pointer to a non-NULL internal actor-reference.
         */
        inline const _AsyncActor *operator->() const noexcept
        {
            assert(getReferencedActor() != 0);
            return static_cast<const _AsyncActor *>(getReferencedActor());
        }
        /**
         * @brief Getter to the internal actor-reference.
         * @return A pointer to the internal actor-reference, which can be NULL.
         */
        inline _AsyncActor *get() noexcept { return static_cast<_AsyncActor *>(getReferencedActor()); }
        /**
         * @brief Getter to the internal actor-reference.
         * @return A const pointer to the internal actor-reference, which can be NULL.
         */
        inline const _AsyncActor *get() const noexcept
        {
            return static_cast<const _AsyncActor *>(getReferencedActor());
        }

      private:
        friend class AsyncActor;
        friend class AsyncEngine;

        /**
         * throw (CircularReferenceException)
         */
        inline ActorReference(AsyncActor &referencingActor, _AsyncActor &referencedActor, bool destroyFlag = true)
            : ActorReferenceBase(referencingActor, referencedActor, destroyFlag)
        {
        }
    };
    /**
     * @brief Singleton class (one per event-loop) to access the real-time performance counters.
     * Reference to the current event-loop CorePerformanceCounters instance is through
     * getCorePerformanceCounters() method of an actor-instance.
     */
    class CorePerformanceCounters
    {
      public:
        /**
         * @brief Getter to the cumulative total event-loop count, regardless of functional activity.
         * This is a significant indicator of application throughput at event-loop (cpu-core) level.
         * Periodically sampling the variation (delta) of this indicator can inform about general application health.
         * @attention As the returned counter is strictly cumulative, there is no guarantee
         * that it will not eventually reach its max value and roll-back from zero.
         * As it is safe to say that the max value cannot be reached below a fixed time threshold
         * (hardware dependent on cpu-core clock max frequency), sampling below that threshold can deal
         * with roll-back knowing that it could only occur once after the previous sampling.
         * @return The real-time latest cumulative event-loop count.
         */
        uint64_t getLoopTotalCount() const noexcept { return loopTotalCount; }
        /**
         * @brief Getter to the cumulative usage event-loop count.
         * Each time a callback is triggered (e.g. onEvent(), onCallback(), onDestroyRequest(),...)
         * the current event-loop iteration will count as 1 usage
         * (multiple callbacks within the same event-loop iteration will count as 1 usage).
         * This is a significant indicator of application event-loop usage (cpu-core) level.
         * Periodically sampling the variation (delta) of this indicator can inform about general application health.
         * Comparing (ratio) this indicator to getLoopTotalCount() indicates the event-loop (cpu-core) saturation level.
         * @attention As the returned counter is strictly cumulative, there is no guarantee
         * that it will not eventually reach its max value and roll-back from zero.
         * As it is safe to say that the max value cannot be reached below a fixed time threshold
         * (hardware dependent on cpu-core clock max frequency), sampling below that threshold can deal
         * with roll-back knowing that it could only occur once after the previous sampling.
         * @return The real-time latest cumulative usage event-loop count.
         */
        uint64_t getLoopUsageCount() const noexcept { return loopUsageCount; }
        /**
         * @brief Getter to the cumulative count to every onEvent() method call for all actors within the event-loop.
         * @attention As the returned counter is strictly cumulative, there is no guarantee
         * that it will not eventually reach its max value and roll-back from zero.
         * @return The real-time latest cumulative count to every onEvent() method.
         */
        uint64_t getOnEventCount() const noexcept { return onEventCount; }
        /**
         * @brief Getter to the cumulative count to every onCallback() method call for all actors within the event-loop.
         * @attention onCallback() calls resulting of registerPerformanceNeutralCallback() are not counted.
         * @attention As the returned counter is strictly cumulative, there is no guarantee
         * that it will not eventually reach its max value and roll-back from zero.
         * @return The real-time latest cumulative count to every onCallback() method.
         */
        uint64_t getOnCallbackCount() const noexcept { return onCallbackCount; }
        /**
         * @brief Getter to the cumulative queue size (byte count) to another cpu-core.
         * This is a significant indicator of application load-balance between event-loops (cpu-cores).
         * Periodically sampling the variation (delta) of this indicator can inform about
         * general application load-balancing between the available cpu-cores.
         * @attention As the returned counter is strictly cumulative, there is no guarantee
         * that it will not eventually reach its max value and roll-back from zero.
         * As it is safe to say that the max value cannot be reached below a fixed time threshold
         * (hardware dependent on cpu-core clock max frequency), sampling below that threshold can deal
         * with roll-back knowing that it could only occur once after the previous sampling.
         * @param nodeId logical cpu-core. If an inconsistent value is passed
         * (e.g. greater than the actual number of cpu-cores), the returned value is zero.
         * @return The real-time latest cumulative size (byte count) of data transfered from
         * current event-loop cpu-core to the cpu-core represented by the arguemnt nodeId.
         * Or zero if nodeId is inconsistent.
         */
        uint64_t getTotalWrittenEventByteSizeTo(NodeId nodeId) const noexcept
        {
            return nodeId < writtenSizePointerVector.size() ? *writtenSizePointerVector[nodeId] : 0;
        }

      private:
        friend class AsyncNode;
        friend class AsyncNodesHandle;
        typedef std::vector<const uint64_t *, Allocator<const uint64_t *>> SizePointerVector;
        SizePointerVector writtenSizePointerVector;
        uint64_t loopTotalCount;
        uint64_t loopUsageCount;
        uint64_t onEventCount;
        uint64_t onCallbackCount;

        CorePerformanceCounters(const AllocatorBase &allocator, size_t nodeCount)
            : writtenSizePointerVector(allocator), loopTotalCount(0), loopUsageCount(0), onEventCount(0),
              onCallbackCount(0)
        {
            writtenSizePointerVector.reserve(nodeCount);
        }
        CorePerformanceCounters(const CorePerformanceCounters &);
        void operator=(const CorePerformanceCounters &);
    };
    /**
     * @brief Base-class of callback handler.
     * To qualify as argument of registerCallback() and registerPerformanceNeutralCallback() actor methods,
     * a class must publicly derive from Callback and implement the public method:<br>
     * <code>void onCallback() throw()</code>
     */
    class Callback : private MultiDoubleChainLink<Callback>
    {
      public:
        /**
         * @brief Destructor where a call to unregister() is performed.
         */
        inline ~Callback() noexcept { unregister(); }
        /**
         * @brief Getter to status to registration with an actor.
         * @return true if this callback-handler is currently registered with an actor.
         */
        inline bool isRegistered() const noexcept { return chain != 0; }
        /**
         * @brief Unregister this callback-handler if it is currently registered with an actor.
         * No effect otherwise. After this method returns, onCallback() can never be called on
         * the sub-class, until this callback-handler is explicitly registered again with
         * an actor using the actor's methods registerCallback() or registerPerformanceNeutralCallback().
         */
        inline void unregister() noexcept
        {
            if (chain != 0)
            {
                chain->remove(this);
                chain = 0;
            }
        }

      protected:
        /**
         * @brief Default constructor.
         */
        inline Callback() noexcept : chain(0) {}

      private:
        friend class AsyncActor;
        friend class AsyncNode;
        friend struct AsyncNodeBase;
        struct Chain : DoubleChain<0u, Chain>
        {
            inline static Callback *getItem(::tredzone::MultiDoubleChainLink<Callback> *link) noexcept
            {
                return static_cast<Callback *>(link);
            }
            inline static const Callback *getItem(const ::tredzone::MultiDoubleChainLink<Callback> *link) noexcept
            {
                return static_cast<const Callback *>(link);
            }
            inline static ::tredzone::MultiDoubleChainLink<Callback> *getLink(Callback *item) noexcept
            {
                return static_cast<::tredzone::MultiDoubleChainLink<Callback> *>(item);
            }
            inline static const ::tredzone::MultiDoubleChainLink<Callback> *getLink(const Callback *item) noexcept
            {
                return static_cast<const ::tredzone::MultiDoubleChainLink<Callback> *>(item);
            }
        };
        Chain *chain;
        NodeActorId nodeActorId;
        EventTable *actorEventTable;
        void (*onCallback)(Callback &) noexcept;
    };
    typedef std::basic_string<char, std::char_traits<char>, Allocator<char>>
        string_type;                                 ///< std::basic_string specialization using AsyncActor::Allocator.
    typedef Property<Allocator<char>> property_type; ///< tredzone::Property specialization using AsyncActor::Allocator.
    typedef OutputStringStream<char, Allocator<char>,
                               TREDZONE_DEFAULT_STREAM_BUFFER_INCREMENT_SIZE>
        ostringstream_type; ///< tredzone::OutputStringStream specialization using AsyncActor::Allocator.

#pragma pack(push)
#pragma pack(1)
    class ActorId;
    /**
     * @brief In-process actor address.
     * An instance of InProcessActorId class uniquely identifies
     * an actor instance within its running AsyncEngine instance.
     * During engine runtime, the address is never recycled and
     * remains unique after the actor is destroyed.
     */
    class InProcessActorId
    {
        friend struct AsyncActor::EventBase;

      public:
        /**
         * @brief Default constructor. Initialize the actor-id to invalid.
         */
        inline InProcessActorId() noexcept : nodeId(0), nodeActorId(0) {}
        /**
         * @brief Constructor with actor-id initialized
         * to match the one of the actor passed as argument.
         * @param actor reference to the actor from which the actor-id is copied.
         */
        inline InProcessActorId(const AsyncActor &actor) noexcept : nodeId(actor.actorId.nodeId),
                                                                    nodeActorId(actor.actorId.nodeActorId),
                                                                    eventTable(actor.actorId.eventTable)
        {
        }
        /**
         * @brief Assign operator to null. Which invalidates this actor-id.
         * @param tredzone::null represents the invalid value of actor-id.
         * @return A reference to this instance.
         */
        inline InProcessActorId &operator=(const Null &) noexcept
        {
            nodeActorId = 0;
            return *this;
        }
        /**
         * @brief Compare for equality this actor-id to another one.
         * @param other another in-process-actor-id.
         * @return true if the actor-ids are identical.
         */
        inline bool operator==(const InProcessActorId &other) const noexcept
        {
            return nodeActorId == other.nodeActorId && nodeId == other.nodeId;
        }
        /**
         * @brief Compare for equality this actor-id to invalid (null).
         * @param tredzone::null represents the invalid value of actor-id.
         * @return true if this actor-id is invalid.
         */
        inline bool operator==(const Null &) const noexcept { return nodeActorId == 0; }
        /**
         * @brief Compare for inequality this actor-id to another one.
         * @param other another in-process-actor-id.
         * @return true if the actor-ids are not identical.
         */
        inline bool operator!=(const InProcessActorId &other) const noexcept
        {
            return nodeActorId != other.nodeActorId || nodeId != other.nodeId;
        }
        /**
         * @brief Compare for inequality this actor-id to invalid (null).
         * @param tredzone::null represents the invalid value of actor-id.
         * @return true if this actor-id is valid.
         */
        inline bool operator!=(const Null &) const noexcept { return nodeActorId != 0; }
        /**
         * @brief Compare for inferiority this actor-id to another one.
         * @param other another in-process-actor-id.
         * @return true if this actor-id is lesser than other.
         */
        inline bool operator<(const InProcessActorId &other) const noexcept
        {
            return nodeId < other.nodeId || (nodeId == other.nodeId && nodeActorId < other.nodeActorId);
        }
        /**
         * @brief Getter to the node-id part of this actor-id.
         * The node-id part of actor-id is equal to the node-id of
         * the event-loop (cpu-core) running the actor represented by this actor-id.
         * @return The node-id part of this actor-id.
         * If this actor-id is invalid, the returned information is arbitrary and
         * not significant.
         */
        inline NodeId getNodeId() const noexcept { return nodeId; }
        /**
         * @brief Getter to the node-actor-id part of this actor-id.
         * @return The node-actor-id part of this actor-id.
         * If this actor-id is invalid, zero is returned.
         */
        inline NodeActorId getNodeActorId() const noexcept { return nodeActorId; }

      protected:
        /**
         * @brief Internal usage constructor.
         */
        inline InProcessActorId(NodeId pnodeId, NodeActorId pnodeActorId, EventTable *peventTable) noexcept
            : nodeId(pnodeId),
              nodeActorId(pnodeActorId),
              eventTable(peventTable)
        {
        }
        /**
         * @brief Internal usage constructor.
         */
        inline InProcessActorId(int) noexcept {}

      private:
        friend class AsyncActor;
        friend class AsyncNodesHandle;
        friend class AsyncEngineToEngineSerialConnector;
        friend std::ostream &operator<<(std::ostream &, const AsyncActor::ActorId &);
        template <class> friend class Accessor;

        NodeId nodeId;
        NodeActorId nodeActorId;
        EventTable *eventTable;
    };

    /**
     * @brief Cluster wide actor-id.
     * An instance of ActorId class uniquely identifies
     * an actor instance within a cluster of engines.
     * During cluster runtime, the address is never recycled and
     * remains unique after the actor is destroyed.
     * From a data-model standpoint, ActorId is composed of
     * InProcessActorId and ActorId::RouteId
     */
    class ActorId : public InProcessActorId
    {
      public:
        /**
         * @brief This exception is thrown when attempt is
         * made to get engine (process) local information out of an
         * actor-id from another engine.
         */
        struct NotInProcessException : std::exception
        {
            virtual const char *what() const noexcept { return "tredzone::AsyncActor::ActorId::NotInProcessException"; }
        };
        /**
         * @brief Super-class of RouteId containing the route-id
         * information uniquely identifying an engine-to-engine route.
         * An instance of this class can be compared to an
         * instance of RouteId, but cannot be used
         * to compose a new actor-id.
         */
        class RouteIdComparable
        {
          public:
            typedef uint32_t NodeConnectionId; ///< Scalar type representing the connection-id within an event-loop
                                               ///(there is one event-loop per cpu-core). Valid values start at 1 (0 is
                                               ///invalid).

            /**
             * @brief Default constructor. Initializes this route-id to invalid.
             */
            inline RouteIdComparable() noexcept : nodeId(0), nodeConnectionId(0) {}
            /**
             * @brief Constructor with explicit member initialization.
             * @param pnodeId initialization value for nodeId member.
             * @param pnodeConnectionId initialization value for nodeConnectionId member.
             */
            inline RouteIdComparable(NodeId pnodeId, NodeConnectionId pnodeConnectionId) noexcept
                : nodeId(pnodeId),
                  nodeConnectionId(pnodeConnectionId)
            {
            }
            /**
             * @brief Assign operator with another route-id.
             * @param other another route-id.
             * @return A reference to this instance.
             */
            inline RouteIdComparable &operator=(const RouteIdComparable &other) noexcept
            {
                nodeId = other.nodeId;
                nodeConnectionId = other.nodeConnectionId;
                return *this;
            }
            /**
             * @brief Assign operator to null. Which invalidates route-id.
             * @param tredzone::null represents the invalid value of route-id.
             * @return A reference to this instance.
             */
            inline RouteIdComparable &operator=(const Null &) noexcept
            {
                nodeConnectionId = 0;
                return *this;
            }
            /**
             * @brief Compare for equality this route-id to another one.
             * @param other another route-id.
             * @return true if the route-ids are identical.
             */
            inline bool operator==(const RouteIdComparable &other) const noexcept
            {
                return nodeId == other.nodeId && nodeConnectionId == other.nodeConnectionId;
            }
            /**
             * @brief Compare for equality this route-id to invalid (null).
             * @param tredzone::null represents the invalid value of route-id.
             * @return true if this route-id is invalid.
             */
            inline bool operator==(const Null &) const noexcept { return nodeConnectionId == 0; }
            /**
             * @brief Compare for inequality this route-id to another one.
             * @param other another route-id.
             * @return true if the route-ids are not identical.
             */
            inline bool operator!=(const RouteIdComparable &other) const noexcept
            {
                return nodeId != other.nodeId || nodeConnectionId != other.nodeConnectionId;
            }
            /**
             * @brief Compare for inequality this route-id to invalid (null).
             * @param tredzone::null represents the invalid value of route-id.
             * @return true if this route-id is valid.
             */
            inline bool operator!=(const Null &) const noexcept { return nodeConnectionId != 0; }
            /**
             * @brief Compare for inferiority this route-id to another one.
             * @param other another route-id.
             * @return true if this route-id is lesser than other.
             */
            inline bool operator<(const RouteIdComparable &other) const noexcept
            {
                return nodeId < other.nodeId || (nodeId == other.nodeId && nodeConnectionId < other.nodeConnectionId);
            }
            /**
             * @brief Checks if the route-id is invalid.
             * Which translates into the fact that the actor-id
             * with this route-id is local to this engine:
             * it does not have a valid route to another engine.
             * @return true if the route-id is invalid.
             */
            inline bool isInProcess() const noexcept { return nodeConnectionId == 0; }
            /**
             * @brief Getter to the node-id part of this route-id.
             * The node-id part of route-id is equal to the node-id of
             * the event-loop (cpu-core) running the connection represented by this route-id.
             * @return The node-id part of this route-id.
             * If this route-id is invalid, the returned information is arbitrary and
             * not significant.
             */
            inline NodeId getNodeId() const noexcept { return nodeId; }
            /**
             * @brief Getter to the node-connection-id part of this route-id.
             * @return The node-connection-id part of this route-id.
             * If this route-id is invalid, zero is returned.
             */
            inline NodeConnectionId getNodeConnectionId() const noexcept { return nodeConnectionId; }

          protected:
            template <class> friend class Accessor;

            NodeId nodeId;                     ///< node-id part of this route-id.
            NodeConnectionId nodeConnectionId; ///< node-connection-id part of this route-id.
        };
        /**
         * @brief RouteId extends RouteIdComparable with
         * the pointer to the engine-to-engine connection.
         * Therefore it can be used to compose an actor-id
         * with a new route-id.
         */
        class RouteId : public RouteIdComparable
        {
          public:
            typedef RouteIdComparable::NodeConnectionId NodeConnectionId;

            /**
             * @brief Default constructor. Initializes this route-id to invalid.
             */
            inline RouteId() noexcept {}
            /**
             * @brief Assign operator with another route-id.
             * @param other another route-id.
             * @return A reference to this instance.
             */
            inline RouteId &operator=(const RouteId &other) noexcept
            {
                RouteIdComparable::operator=(other);
                nodeConnection = other.nodeConnection;
                return *this;
            }
            /**
             * @brief Assign operator to null. Which invalidates route-id.
             * @param tredzone::null represents the invalid value of route-id.
             * @return A reference to this instance.
             */
            inline RouteId &operator=(const Null &null) noexcept
            {
                RouteIdComparable::operator=(null);
                return *this;
            }

          private:
            friend class AsyncActor;
            friend class AsyncNodesHandle;
            friend class AsyncEngineToEngineConnector;
            template <class> friend class Accessor;

            NodeConnection *nodeConnection;

            inline RouteId(NodeId pnodeId, NodeConnectionId pnodeConnectionId, NodeConnection *pnodeConnection) noexcept
                : RouteIdComparable(pnodeId, pnodeConnectionId),
                  nodeConnection(pnodeConnection)
            {
            }
        };

        /**
         * @brief Default constructor. Initializes this actor-id to invalid.
         */
        inline ActorId() noexcept {}
        /**
         * @brief Constructor with actor-id initialized
         * to match the one of the actor passed as argument.
         * @param actor reference to the actor from which the actor-id is copied.
         */
        inline ActorId(const AsyncActor &actor) noexcept : InProcessActorId(actor) {}
        /**
         * @brief Assign operator to null. Which invalidates this actor-id.
         * @param tredzone::null represents the invalid value of actor-id.
         * @return A reference to this instance.
         */
        inline ActorId &operator=(const Null &null) noexcept
        {
            InProcessActorId::operator=(null);
            return *this;
        }
        /**
         * @brief Compare for equality this actor-id to another one.
         * @param other another actor-id.
         * @return true if the actor-ids are identical.
         */
        inline bool operator==(const ActorId &other) const noexcept
        {
            return InProcessActorId::operator==(other) && routeId == other.routeId;
        }
        /**
         * @brief Compare for equality this actor-id to invalid (null).
         * @param tredzone::null represents the invalid value of actor-id.
         * @return true if this actor-id is invalid.
         */
        inline bool operator==(const Null &null) const noexcept { return InProcessActorId::operator==(null); }
        /**
         * @brief Compare for inequality this actor-id to another one.
         * @param other another actor-id.
         * @return true if the actor-ids are not identical.
         */
        inline bool operator!=(const ActorId &other) const noexcept
        {
            return InProcessActorId::operator!=(other) || routeId != other.routeId;
        }
        /**
         * @brief Compare for inequality this actor-id to invalid (null).
         * @param tredzone::null represents the invalid value of actor-id.
         * @return true if this actor-id is valid.
         */
        inline bool operator!=(const Null &null) const noexcept { return InProcessActorId::operator!=(null); }
        /**
         * @brief Compare for inferiority this actor-id to another one.
         * @param other another actor-id.
         * @return true if this actor-id is lesser than other.
         */
        inline bool operator<(const ActorId &other) const noexcept
        {
            return routeId < other.routeId || (routeId == other.routeId && InProcessActorId::operator<(other));
        }
        /**
         * @brief Checks if the route-id part of this actor-id is invalid.
         * Which translates into the fact that this actor-id
         * is local to this engine: it does not have a valid route to another engine.
         * @return true if the route-id part of this actor-id is invalid.
         */
        inline bool isInProcess() const noexcept { return routeId.isInProcess(); }
        /**
         * @brief Checks if this actor-id is running on the same
         * event-loop (cpu-core) as another one.
         * @param other another actor-id.
         * @return true if the two actor-ids are running on the same event-loop (cpu-core).
         */
        inline bool isSameCoreAs(const ActorId &other) const noexcept
        {
            return isInProcess() && other.isInProcess() && nodeId == other.nodeId;
        }
        /**
         * @brief Getter to the route-id part of this actor-id.
         * @return The route-id part of this actor-id.
         */
        inline const RouteId &getRouteId() const noexcept { return routeId; }
        /**
         * @brief Getter to the node-id of the actor if it is running in this process.
         * @throw NotInProcessException if the actor not running in this process.
         * @return The core-index (node-id) of the actor which is running in this process.
         */
        inline size_t getCoreIndex() const
        { // throw (NotInProcessException)
            assert(isInProcess());
            if (isInProcess() == false)
            {
                throw NotInProcessException();
            }
            return nodeId;
        }
        /**
         * @brief Getter to the core-id of the actor if it is running in this process.
         * @throw NotInProcessException if the actor not running in this process.
         * @return The core-id of the in-this-process event-loop (cpu-core) running this actor.
         */
        CoreId getCore() const; // throw (NotInProcessException)

      private:
        friend class Event;
        friend class AsyncNode;
        friend class AsyncActor;
        friend class AsyncEngineToEngineConnector;
        friend class AsyncEngineToEngineSerialConnector;
        friend class AsyncEngineToEngineConnectorEventFactory;
        template <class> friend class Accessor;

        RouteId routeId;

        inline ActorId(NodeId pnodeId, NodeActorId pnodeActorId, EventTable *peventTable) noexcept
            : InProcessActorId(pnodeId, pnodeActorId, peventTable)
        {
        }
        inline ActorId(const InProcessActorId &other) noexcept : InProcessActorId(other) {}
        inline ActorId(const InProcessActorId &other, const RouteId &prouteId) noexcept : InProcessActorId(other),
                                                                                          routeId(prouteId)
        {
        }
        inline ActorId(int) noexcept : InProcessActorId(0) {}
    };
#pragma pack(pop)

    /**
     * @brief Getter to the actor-id of this actor.
     * @return A reference to the actor-id of this actor.
     */
    inline const ActorId &getActorId() const noexcept { return actorId; }
    /**
     * @brief Getter to the core-id of this actor.
     * @return The core-id of the event-loop (cpu-core) running this actor.
     */
    CoreId getCore() const noexcept;
    /**
     * @brief Getter to the current engine instance.
     * @return The current engine instance.
     */
    AsyncEngine &getEngine() noexcept;
    /**
     * @brief Getter to the current engine instance.
     * @return The current engine const instance.
     */
    const AsyncEngine &getEngine() const noexcept;
    /**
     * @brief Getter to local event-loop (cpu-core) allocator.
     * @return A copy of a local event-loop (cpu-core) allocator.
     */
    AllocatorBase getAllocator() const noexcept;
    /**
     * @brief Create a new typed actor-reference to an existing actor.
     * For this operation to succeed the following conditions must be met:
     * - the referenced actor was not destroyed
     * - the referenced actor must be ran by the same event-loop (cpu-core) as this actor
     * - the referenced actor must be of type _AsyncActor (the template generic type)
     *
     * @param actorId actor-id of an existing actor.
     * @return A new typed actor-reference smart-pointer to the actor represented by actorId.
     * @throw CircularReferenceException would create a circular referencing in the reference tree.
     * actorId represents an actor that either already directly references this actor,
     * or references an actor that directly references this actor. This would create
     * a dead-lock situation as actors are not allowed to be destroyed while they are still
     * referenced.
     * @throw ReferenceLocalActorException the referenced actor either is not ran by
     * the same event-loop (cpu-core) as this actor, or was destroyed, and does not exist anymore.
     * @throw std::bad_cast the referenced actor is not of type _AsyncActor (the template generic type).
     */
    template <class _AsyncActor> inline ActorReference<_AsyncActor> referenceLocalActor(const ActorId &actorId);
    /**
     * @brief Creates a new actor which will be ran by the same event-loop (cpu-core) as this actor.
     * The created actor will be of type _AsyncActor (the template generic type),
     * and will be constructed using its default constructor (constructor with no arguments).
     * _AsyncActor must have AsyncActor as a public super-class.
     * The usual way to use this method is for an actor instance to embed another actor instance.
     * Therefore, when the embedding actor is destroyed, the embedded actor should seamlessly be destroyed.
     * To reflect this general case behavior, the actor created with this method is marked for destruction.
     * When all actor-references to the new actor are destroyed, the new actor is automatically destroyed.
     * @note To prevent automatic actor destruction, the following code should be used instead:
     * \code
     * referenceLocalActor<_AsyncActor>(newUnreferencedActor<_AsyncActor>());
     * \endcode
     * @attention During engine shutdown (destructor of AsyncEngine being called),
     * to avoid live-lock situations, no new actor can be created.
     * Attempting to do so will result in a ShutdownException exception.
     * @return An actor-reference smart-pointer to the newly created actor.
     * @throw CircularReferenceException Although unlikely, it is possible
     * that at construction, the created actor directly or indirectly references this actor.
     * If this was not detected, it would result in a circular reference situation
     * with a dead-lock occurring at destruction. Therefore an exception is thrown
     * in this case preventing the actor from being created, hence avoiding circular referencing.
     * @throw ShutdownException The engine is shutting down (destructor of AsyncEngine being called),
     * no actor can be created anymore.
     * @throw std::bad_alloc
     * @throw ? Any other exception possibly thrown depending on _AsyncActor (the template generic type)
     * constructor call.
     */
    template <class _AsyncActor> inline ActorReference<_AsyncActor> newReferencedActor();
    /**
     * @brief Creates a new actor which will be ran by the same event-loop (cpu-core) as this actor.
     * The created actor will be of type _AsyncActor (the template first generic type),
     * and will be constructed using its one-parameter constructor
     * assignable from _AsyncActorInit type (the template second generic type).
     * _AsyncActor must have AsyncActor as a public super-class.
     * The usual way to use this method is for an actor instance to embed another actor instance.
     * Therefore, when the embedding actor is destroyed, the embedded actor should seamlessly be destroyed.
     * To reflect this general case behavior, the actor created with this method is marked for destruction.
     * When all actor-references to the new actor are destroyed, the new actor is automatically destroyed.
     * @note To prevent automatic actor destruction, the following code should be used instead:
     * \code
     * referenceLocalActor<_AsyncActor>(newUnreferencedActor<_AsyncActor>(actorInit));
     * \endcode
     * @attention During engine shutdown (destructor of AsyncEngine being called),
     * to avoid live-lock situations, no new actor can be created.
     * Attempting to do so will result in a ShutdownException exception.
     * @param actorInit parameter to be passed as argument to the constructor of the new actor.
     * @return An actor-reference smart-pointer to the newly created actor.
     * @throw CircularReferenceException Although unlikely, it is possible
     * that at construction, the created actor directly or indirectly references this actor.
     * If this was not detected, it would result in a circular reference situation
     * with a dead-lock occurring at destruction. Therefore an exception is thrown
     * in this case preventing the actor from being created, hence avoiding circular referencing.
     * @throw ShutdownException The engine is shutting down (destructor of AsyncEngine being called),
     * no actor can be created anymore.
     * @throw std::bad_alloc
     * @throw ? Any other exception possibly thrown depending on _AsyncActor (the template generic type)
     * constructor call.
     */
    template <class _AsyncActor, class _AsyncActorInit>
    inline ActorReference<_AsyncActor> newReferencedActor(const _AsyncActorInit &actorInit);
    /**
     * @brief Creates a single actor instance per _AsyncActor (the template generic type) class,
     * and per event-loop (cpu-core).
     * When this method is called for the first time using _AsyncActor as template generic type,
     * it exactly behaves as newReferencedActor<_AsyncActor>().
     * Subsequent calls to this method with the same template generic type will return
     * an actor-reference to the already existing actor in this event-loop (cpu-core).
     * @note The singleton-actor is automatically destroyed when all actor-references to it are
     * destroyed. The first call to this method after the singleton-actor was destroyed
     * will create a new, therefore <b>different</b> singleton-actor.
     * @attention During engine shutdown (destructor of AsyncEngine being called),
     * to avoid live-lock situations, no new actor can be created.
     * Attempting to do so will result in a ShutdownException exception.
     * @return An actor-reference smart-pointer to the event-loop (cpu-core) singleton-actor.
     * @throw CircularReferenceException It is possible that the singleton-actor
     * directly or indirectly references this actor.
     * If this was not detected, it would result in a circular reference situation
     * with a dead-lock occurring at destruction. Therefore an exception is thrown
     * in this case preventing the actor from being created, hence avoiding circular referencing.
     * @throw ShutdownException The engine is shutting down (destructor of AsyncEngine being called),
     * no actor can be created anymore.
     * @throw std::bad_alloc
     * @throw ? Any other exception possibly thrown depending on _AsyncActor (the template generic type)
     * constructor call.
     */
    template <class _AsyncActor> inline ActorReference<_AsyncActor> newReferencedSingletonActor();
    /**
     * @brief Creates a single actor instance per _AsyncActor (the template first generic type) class,
     * and per event-loop (cpu-core).
     * When this method is called for the first time using _AsyncActor as template first generic type,
     * it exactly behaves as newReferencedActor<_AsyncActor>(const _AsyncActorInit&).
     * Subsequent calls to this method with the same template first generic type, regardless of
     * the actorInit parameter will return an actor-reference to the already
     * existing actor in this event-loop (cpu-core).
     * @note The singleton-actor is automatically destroyed when all actor-references to it are
     * destroyed. The first call to this method after the singleton-actor was destroyed
     * will create a new, therefore <b>different</b> singleton-actor.
     * @attention During engine shutdown (destructor of AsyncEngine being called),
     * to avoid live-lock situations, no new actor can be created.
     * Attempting to do so will result in a ShutdownException exception.
     * @param actorInit parameter to be passed as argument to the constructor of the new singleton-actor.
     * @return An actor-reference smart-pointer to the event-loop (cpu-core) singleton-actor.
     * @throw CircularReferenceException It is possible that the singleton-actor
     * directly or indirectly references this actor.
     * If this was not detected, it would result in a circular reference situation
     * with a dead-lock occurring at destruction. Therefore an exception is thrown
     * in this case preventing the actor from being created, hence avoiding circular referencing.
     * @throw ShutdownException The engine is shutting down (destructor of AsyncEngine being called),
     * no actor can be created anymore.
     * @throw UndersizedException The number of different classes used to create singleton-actors
     * exceeds the limit (4096).
     * @throw std::bad_alloc
     * @throw ? Any other exception possibly thrown depending on _AsyncActor (the template first generic type)
     * constructor call.
     */
    template <class _AsyncActor, class _AsyncActorInit>
    inline ActorReference<_AsyncActor> newReferencedSingletonActor(const _AsyncActorInit &actorInit);
    /**
     * @brief Creates a new actor which will be ran by the same event-loop (cpu-core) as this actor.
     * The created actor will be of type _AsyncActor (the template generic type),
     * and will be constructed using its default constructor (constructor with no arguments).
     * _AsyncActor must have AsyncActor as a public super-class.
     * @attention During engine shutdown (destructor of AsyncEngine being called),
     * to avoid live-lock situations, no new actor can be created.
     * Attempting to do so will result in a ShutdownException exception.
     * @return The actor-id of the newly created actor.
     * @throw ShutdownException The engine is shutting down (destructor of AsyncEngine being called),
     * no actor can be created anymore.
     * @throw std::bad_alloc
     * @throw ? Any other exception possibly thrown depending on _AsyncActor (the template generic type)
     * constructor call.
     */
    template <class _AsyncActor> inline const ActorId &newUnreferencedActor();
    /**
     * @brief Creates a new actor which will be ran by the same event-loop (cpu-core) as this actor.
     * The created actor will be of type _AsyncActor (the template first generic type),
     * and will be constructed using its one-parameter constructor
     * assignable from _AsyncActorInit type (the template second generic type).
     * _AsyncActor must have AsyncActor as a public super-class.
     * @attention During engine shutdown (destructor of AsyncEngine being called),
     * to avoid live-lock situations, no new actor can be created.
     * Attempting to do so will result in a ShutdownException exception.
     * @param actorInit parameter to be passed as argument to the constructor of the new actor.
     * @return The actor-id of the newly created actor.
     * @throw ShutdownException The engine is shutting down (destructor of AsyncEngine being called),
     * no actor can be created anymore.
     * @throw std::bad_alloc
     * @throw ? Any other exception possibly thrown depending on _AsyncActor (the template first generic type)
     * constructor call.
     */
    template <class _AsyncActor, class _AsyncActorInit>
    inline const ActorId &newUnreferencedActor(const _AsyncActorInit &actorInit);
    /**
     * @brief Registers the callback-handler passed as a parameter.
     * The callback-handler is of template generic type _Callback
     * which must meet the following conditions:
     * - has AsyncActor::Callback as a public super-class
     * - publicly implement the method <code>void onCallback() throw()</code>
     *
     * This method causes the event-loop (cpu-core) running this actor
     * to call onCallback() <b>once</b> on the next loop iteration.
     * Calling this method multiple times will always produce the same
     * effect as calling it a single time.
     * @note _Callback does not need to be of polymorphic (virtual) type.
     * @param callbackHandler instance of _Callback implementing the onCallback() method.
     */
    template <class _Callback> inline void registerCallback(_Callback &callbackHandler) noexcept;
    /**
     * @brief Behaves exactly as registerCallback<_Callback>(_Callback&) with only one difference:
     * it does not take part in the cumulative usage event-loop count (see CorePerformanceCounters::getLoopUsageCount(),
     * CorePerformanceCounters::getOnCallbackCount()).
     * @note This method has a very specific and limited usage.
     * It is useful when some periodic calls are typically required
     * by the system (e.g. yield the event-loop thread), or third party libraries.
     * We would  not want to count these callbacks as significant functional usage.
     * They are mechanical mandatory overheads.
     * @param callbackHandler instance of _Callback implementing the onCallback() method.
     * @see CorePerformanceCounters
     */
    template <class _Callback> inline void registerPerformanceNeutralCallback(_Callback &callbackHandler) noexcept;
    /**
     * @brief Marks this actor for later destruction by the runtime. The actual destruction is asynchronous.
     * When eligible for destruction, the onDestroyRequest() method of this actor will be called.
     * If destruction should be aborted (see onDestroyRequest()), destruction can be requested at a later point by calling this method again.
     * Calling this method multiple times has the same effect as calling it once.
     * @note An actor onDestroyRequest() method will be called when the following conditions are met:
     * - its requestDestroy() method was called
     * 		- explicitly
     * 		- or the engine is shutting down
     * 		- or an actor-reference returned by newReferencedActor() was destroyed
     * 		- or an actor-reference returned by newReferencedSingletonActor() was destroyed
     * - and there are no actor-reference left referencing the actor
     * - and, if the actor was created as a service,
     * it is its turn in the services' destruction-sequence (see AsyncEngine::StartSequence)
     * @see ActorReference
     */
    void requestDestroy(void) noexcept;
    /**
     * @brief Getter to the event-loop (cpu-core) singleton instance of CorePerformanceCounters.
     * @return the event-loop (cpu-core) singleton instance of CorePerformanceCounters.
     */
    const CorePerformanceCounters &getCorePerformanceCounters() const noexcept;
    /**
     * @brief Registers the event-handler passed as a parameter.
     * The event-handler is of template generic type _EventHandler
     * which must meet the following conditions:
     * - publicly implement the method <code>void onEvent(const _Event&)</code>
     * - _Event has AsyncActor::Event as a public super-class
     *
     * This method causes the event-loop (cpu-core) running this actor
     * to call onEvent(const _Event&) every time an instance of _Event
     * was pushed to this actor using Event::Pipe.
     * unregisterEventHandler() or unregisterAllEventHandlers() must first be called
     * before calling this method a second time using the same template
     * generic parameters (regardless of eventHandler parameter).
     * @note _EventHandler does not need to be of polymorphic (virtual) type.
     * @attention Any uncaught exception from onEvent(const _Event&)
     * other than ReturnToSenderException will be handled by the
     * current engine exception-handler (see AsyncExceptionHandler).
     * @param eventHandler instance of _EventHandler implementing the onEvent(const _Event&) method.
     * @throw AlreadyRegisterdEventHandlerException On a second attempt to call this method
     * using the same template generic parameters (regardless of eventHandler parameter),
     * without unregistering eventHandler in between the attempts.
     * @throw std::bad_alloc
     */
    template <class _Event, class _EventHandler> inline void registerEventHandler(_EventHandler &eventHandler);
    /**
     * @brief Registers the event-handler passed as a parameter.
     * The event-handler is of template generic type _EventHandler
     * which must meet the following conditions:
     * - publicly implement the method <code>void onUndeliveredEvent(const _Event&)</code>
     * - _Event has AsyncActor::Event as a public super-class
     *
     * This method causes the event-loop (cpu-core) running this actor
     * to call onUndeliveredEvent(const _Event&) every time an instance of _Event
     * was pushed from this actor using Event::Pipe, and could not be delivered
     * (did not trigger the onEvent(const _Event&) at the receiving end).
     * unregisterUndeliveredEventHandler() or unregisterAllEventHandlers() must first be called
     * before calling this method a second time using the same template
     * generic parameters (regardless of eventHandler parameter).
     * @note Instance of _Event can fail to be delivered,
     * hence trigger onUndeliveredEvent(const _Event&), in the following cases:
     * - the receiving actor does not exist (never existed, or was destroyed)
     * - the receiving actor exists but did not register an event-handler for _Event
     * - the receiving actor threw ReturnToSenderException exception from
     * its registered event-handler
     *
     * @note _EventHandler does not need to be of polymorphic (virtual) type.
     * @attention Any uncaught exception from onUndeliveredEvent(const _Event&)
     * will be handled by the current engine exception-handler
     * (see AsyncExceptionHandler).
     * @param eventHandler instance of _EventHandler implementing the onUndeliveredEvent(const _Event&) method.
     * @throw AlreadyRegisterdEventHandlerException On a second attempt to call this method
     * using the same template generic parameters (regardless of eventHandler parameter),
     * without unregistering eventHandler in between the attempts.
     * @throw std::bad_alloc
     */
    template <class _Event, class _EventHandler>
    inline void registerUndeliveredEventHandler(_EventHandler &eventHandler);
    /**
     * @brief Unregisters any the event-handler previously registered
     * using registerEventHandler<_Event, ?>(), regardless of the template second generic type.
     * If no event-handler is registered for _Event, this method does not do anything.
     * @note After this method returns onEvent(const _Event&)
     * can never be called by event-loop.
     */
    template <class _Event> inline void unregisterEventHandler() noexcept;
    /**
     * @brief Unregisters any the event-handler previously registered
     * using registerUndeliveredEventHandler<_Event, ?>(), regardless of
     * the template second generic type.
     * If no event-handler is registered for _Event, this method does not do anything.
     * @note After this method returns onUndeliveredEvent(const _Event&)
     * can never be called by event-loop.
     */
    template <class _Event> inline void unregisterUndeliveredEventHandler() noexcept;
    /**
     * @brief Unregisters all event-handlers previously registered
     * using registerEventHandler<?, ?>() and/or registerUndeliveredEventHandler<?, ?>(),
     * regardless of the template first and second generic types.
     * If no event-handler is registered, this method does not do anything.
     * @note After this method returns any onEvent() and onUndeliveredEvent() methods
     * can never be called by event-loop.
     */
    void unregisterAllEventHandlers() noexcept;
    /**
     * @brief Getter.
     * @return true if an event-handler was registered using registerEventHandler<_Event, ?>(),
     * regardless of the template second generic type.
     */
    template <class _Event> inline bool isRegisteredEventHandler() const noexcept;
    /**
     * @brief Getter.
     * @return true if an event-handler was registered using registerUndeliveredEventHandler<_Event, ?>(),
     * regardless of the template second generic type.
     */
    template <class _Event> inline bool isRegisteredUndeliveredEventHandler() const noexcept;

  protected:
    /**
     * @brief Default constructor.
     * @throw ShutdownException The engine is shutting down (destructor of AsyncEngine being called),
     * no actor can be created anymore.
     * @throw std::bad_alloc
     */
    AsyncActor();
    /**
     * @brief Copy constructor.
     * @param other another actor.
     * @throw ShutdownException The engine is shutting down (destructor of AsyncEngine being called),
     * no actor can be created anymore.
     * @throw std::bad_alloc
     */
    AsyncActor(const AsyncActor &other);
    /**
     * @brief Destructor.
     * @note AsyncActor is a polymorphic (virtual) class.
     */
    virtual ~AsyncActor() noexcept;
    /**
     * @brief Polymorphic callback method called when a route
     * to another engine is permanently lost.
     * @note To be enabled this method requires the following conditions to be met:
     * - at least one Event::Pipe instance has existed between this actor
     * and an actor sitting on a different engine
     * - at least one event-handler is currently registered using registerUndeliveredEventHandler()
     *
     * @attention Any uncaught exception from this method specialization
     * will be handled by the current engine exception-handler
     * (see AsyncExceptionHandler).
     * @param routeId The lost route.
     * @see AsyncExceptionHandler
     */
    virtual void onUnreachable(const ActorId::RouteIdComparable &routeId);
    /**
     * @brief Polymorphic callback method called when this actor is eligible
     * for destruction (see destroy()).
     * @attention To confirm destruction, the specialization of this method
     * must explicitly call acceptDestroy(). To defer destruction,
     * the requestDestroy() method must at some point be called again (directly managed by this actor).
     * <br>Example:
     * \code
     * class MyActor : public tredzone::Actor {
     * protected:
     *     virtual void onDestroyRequest() throw() {
     *         if (isWorkflowCompleted())
     *         {    // work finished, accept destruction
     *              acceptDestroy();
     *         }
     *         else
     *         {    // still work to do, abort destroy and try later
     *              requestDestroy();
     *         }
     *     }
     *
     * private:
     *
     *     bool isWorkflowCompleted() throw();
     * };
     * \endcode
     */
    virtual void onDestroyRequest() noexcept;
    
    /**
     * @brief Accept destroy request by flagging the Actor for (sometime later) destruction
     */
    virtual void acceptDestroy(void) noexcept;
    
    /**
     * @brief Getter.
     * @return The event-loop instance running this actor.
     */
    AsyncEngineEventLoop &getEventLoop() const noexcept;

  private:
    friend class AsyncNode;
    friend struct AsyncNodeBase;
    friend class AsyncNodesHandle;
    friend class AsyncEngine;
    friend class AsyncEngineToEngineConnector;
    friend class AsyncEngineToEngineSharedMemoryConnector;
    typedef MultiDoubleChainLink<AsyncActor, 2u> super;
    typedef uint16_t SingletonActorIndex;
    typedef ActorReferenceBase::Chain ReferenceToChain;
    template <class _Event, class _EventHandler> struct StaticEventHandler;
    template <class _Callback> struct StaticCallbackHandler;
    struct Chain : DoubleChain<0u, Chain>
    {
        inline static AsyncActor *getItem(super *link) noexcept { return static_cast<AsyncActor *>(link); }
        inline static const AsyncActor *getItem(const super *link) noexcept
        {
            return static_cast<const AsyncActor *>(link);
        }
        inline static super *getLink(AsyncActor *item) noexcept { return static_cast<super *>(item); }
        inline static const super *getLink(const AsyncActor *item) noexcept { return static_cast<const super *>(item); }
    };
    struct OnUnreachableChain : DoubleChain<1u, OnUnreachableChain>
    {
        inline static AsyncActor *getItem(super *link) noexcept { return static_cast<AsyncActor *>(link); }
        inline static const AsyncActor *getItem(const super *link) noexcept
        {
            return static_cast<const AsyncActor *>(link);
        }
        inline static super *getLink(AsyncActor *item) noexcept { return static_cast<super *>(item); }
        inline static const super *getLink(const AsyncActor *item) noexcept { return static_cast<const super *>(item); }
    };
    template <class _Actor> struct ActorWrapper : virtual AsynActorBase, _Actor
    {
        inline ActorWrapper(AsyncNode &asyncNode) : AsynActorBase(&asyncNode) {}
        template <class _ActorInit>
        inline ActorWrapper(AsyncNode &asyncNode, const _ActorInit &actorInit)
            : AsynActorBase(&asyncNode), _Actor(actorInit)
        {
        }
        inline void operator delete(void *p) noexcept
        {
            AllocatorBase(*static_cast<ActorWrapper *>(p)->AsyncActor::asyncNode).deallocate(sizeof(ActorWrapper), p);
        }
        inline void operator delete(void *, void *p) noexcept
        {
            AllocatorBase(*static_cast<ActorWrapper *>(p)->AsyncActor::asyncNode).deallocate(sizeof(ActorWrapper), p);
        }
    };
    struct RetainedSingletonActorIndex
    {
        const SingletonActorIndex singletonActorIndex;
        /**
         * throw (std::bad_alloc)
         */
        inline RetainedSingletonActorIndex() : singletonActorIndex(retainSingletonActorIndex()) {}
        inline ~RetainedSingletonActorIndex() noexcept { releaseSingletonActorIndex(singletonActorIndex); }
    };

    EventTable &eventTable;
    SingletonActorIndex singletonActorIndex;
    ActorId actorId;
    Chain *chain;
    OnUnreachableChain *onUnreachableChain;
    size_t referenceFromCount;
    ReferenceToChain referenceToChain;
    bool m_DestroyRequestedFlag;
    bool onUnreferencedDestroyFlag;
    size_t processOutPipeCount;
#ifndef NDEBUG
    size_t debugPipeCount;
#endif

    uint8_t registerLowPriorityEventHandler(void *, EventId, void *,
                                            bool (*)(void *, const Event &));                // throw (std::bad_alloc)
    void registerLowPriorityEventHandler(EventId, void *, bool (*)(void *, const Event &));  // throw (std::bad_alloc)
    void registerHighPriorityEventHandler(EventId, void *, bool (*)(void *, const Event &)); // throw (std::bad_alloc)
    void registerUndeliveredEventHandler(EventId, void *, bool (*)(void *, const Event &));  // throw (std::bad_alloc)
    uint8_t unregisterLowPriorityEventHandler(void *, EventId) noexcept;
    void unregisterLowPriorityEventHandlers(void *) noexcept;
    void unregisterHighPriorityEventHandler(EventId) noexcept;
    void unregisterEventHandler(EventId) noexcept;
    void unregisterUndeliveredEventHandler(EventId) noexcept;
    bool isRegisteredLowPriorityEventHandler(void *, EventId) const noexcept;
    bool isRegisteredHighPriorityEventHandler(EventId) const noexcept;
    bool isRegisteredEventHandler(EventId) const noexcept;
    bool isRegisteredUndeliveredEventHandler(EventId) const noexcept;
    void registerCallback(void (*onCallback)(Callback &) noexcept, Callback &) noexcept;
    void registerPerformanceNeutralCallback(void (*onCallback)(Callback &) noexcept, Callback &) noexcept;
    AsyncActor *getSingletonActor(SingletonActorIndex) noexcept;
    void reserveSingletonActor(SingletonActorIndex); // throw (CircularReferenceException)
    void setSingletonActor(SingletonActorIndex, AsyncActor &) noexcept;
    void unsetSingletonActor(SingletonActorIndex) noexcept;
    template <class _AsyncActor>
    inline static _AsyncActor &newActor(AsyncNode &); // throw (std::bad_alloc, ShutdownException, ...)
    template <class _AsyncActor, class _AsyncActorInit>
    inline static _AsyncActor &newActor(AsyncNode &,
                                        const _AsyncActorInit &); // throw (std::bad_alloc, ShutdownException, ...)
    template <class _AsyncActor> inline static SingletonActorIndex getSingletonActorIndex()
    { // throw (std::bad_alloc)
        static RetainedSingletonActorIndex retainedSingletonActorIndex;
        return retainedSingletonActorIndex.singletonActorIndex;
    }
    static SingletonActorIndex retainSingletonActorIndex(); // throw (std::bad_alloc)
    static void releaseSingletonActorIndex(SingletonActorIndex) noexcept;
    AsyncActor &getReferenceToLocalActor(const ActorId &); // throw (ReferenceLocalActorException)
    inline AsyncNode *getAsyncNode() noexcept { return asyncNode; }

    AsyncActor &operator=(const AsyncActor &);
};

/**
 * @brief Used to force call to onUndeliveredEvent() (see registerUndeliveredEventHandler()).
 * <br>Example:
 * \code
 * class MyActor : public tredzone::Actor {
 * public:
 *     class MyEvent : public Event {
 *     };
 *     class MyEventHandler {
 *     public:
 *         void onEvent(const MyEvent& event) {
 *             doSomeThingWithMyEvent(event);
 *             throw ReturnToSenderException();     // the sender actor will be called on its event-handler
 * onUndeliveredEvent(const MyEvent&)
 *         }
 *
 *     private:
 *         void doSomeThingWithMyEvent(const MyEvent&);
 *     };
 *     MyActor() {
 *         registerEventHandler<MyEvent>(myEventHandler);
 *     }
 *
 * private:
 *     MyEventHandler myEventHandler;
 * };
 * \endcode
 */
struct AsyncActor::ReturnToSenderException : std::exception
{
    virtual const char *what() const noexcept { return "tredzone::AsyncActor::ReturnToSenderException"; }
};

/**
 * @brief Thrown when the following limits are exceeded:
 * - The number of Event sub-classes specialized at runtime exceeds the limit (4096).
 * Event specialization occurs when calling:
 *     - registerEventHandler<_Event, _EventHandler>()
 *     - registerUndeliveredEventHandler<_Event, _EventHandler>()
 *     - Event::getClassId<_Event>()
 *     - Event::Pipe::push<_Event>()
 *     - Event::BufferedPipe::push<_Event>()
 * - The number of AsyncActor sub-classes used to create singleton-actors at runtime exceeds the limit (4096).
 * Singleton_actor creation occurs when calling newReferencedSingletonActor().
 */
struct AsyncActor::UndersizedException : std::bad_alloc
{
    enum TypeEnum
    {
        EVENT_ID,
        SINGLETON_ACTOR_INDEX
    };

    const TypeEnum type;

    inline UndersizedException(TypeEnum ptype) : type(ptype) {}
    virtual const char *what() const noexcept
    {
        switch (type)
        {
        case EVENT_ID:
            return "tredzone::AsyncActor::UndersizedException(EVENT_ID)";
        case SINGLETON_ACTOR_INDEX:
            return "tredzone::AsyncActor::UndersizedException(SINGLETON_ACTOR_INDEX)";
        }
        return "tredzone::AsyncActor::UndersizedException";
    }
};

struct AsyncActor::EventBase
{

  public:
    EventBase() : classId(0), sourceActorId(0), destinationActorId(0), routeOffset(0)
    {
        breakThrow(std::runtime_error("illegal direct base instantiation"));
    }

  private:
    friend class AsyncActor::Event;
    friend class AsyncExceptionHandler;
    friend class AsyncEngineToEngineConnectorEventFactory;
    typedef uint16_t route_offset_type;

    AsyncActor::EventId classId;
    AsyncActor::InProcessActorId sourceActorId;
    AsyncActor::InProcessActorId destinationActorId;
    route_offset_type routeOffset;

    EventBase(AsyncActor::EventId _classId, AsyncActor::InProcessActorId _sourceActorId,
              AsyncActor::InProcessActorId _destinationActorId, route_offset_type _routeOffset)
        : classId(_classId), sourceActorId(_sourceActorId), destinationActorId(_destinationActorId),
          routeOffset(_routeOffset)
    {
    }
};

/**
 * @brief Base-class for events used in:
 * - registerEventHandler()
 * - registerUndeliveredEventHandler()
 * - Event::Pipe::push()
 * - Event::BufferedPipe::push()
 * - tredzone::AsyncEngineToEngineConnectorEventFactory::newEvent()
 *
 * Event and Event sub-classes cannot only be instantiated using the following factory methods:
 * - Event::Pipe::push()
 * - Event::BufferedPipe::push()
 * - tredzone::AsyncEngineToEngineConnectorEventFactory::newEvent()
 */
#pragma pack(push)
#pragma pack(1)
class AsyncActor::Event : private MultiForwardChainLink<Event>, virtual private EventBase
{
  public:
    class AllocatorBase;
    template <class T> class Allocator;
    typedef Property<Allocator<char>> property_type;
    class Batch;
    class Pipe;
    class BufferedPipe;
    /**
     * @brief Thrown when cluster-event-id is not unique.
     * Another event-based class has the same cluster-event-id
     * (see isE2ECapable()).
     */
    struct DuplicateAbsoluteEventIdException : std::exception
    {
        virtual const char *what() const noexcept
        {
            return "tredzone::AsyncActor::Event::DuplicateAbsoluteEventIdException";
        }
    };
    /**
     * @brief Double ended queue like, used in event serialization in cluster operations
     * (see EventE2ESerializeFunction).
     * This class is a wrapper to tredzone::SerialBufferChain.
     */
    class SerialBuffer : private SerialBufferChain<AsyncActor::Allocator<char>>
    {
      public:
        typedef SerialBufferChain<AsyncActor::Allocator<char>> Base;
        typedef Base::WriteMark WriteMark;

        /**
         * @brief Getter.
         * @return true if this buffer is empty.
         */
        inline bool empty() const noexcept { return Base::empty(); }
        /**
         * @brief Getter.
         * @return Byte count of this buffer content.
         */
        inline size_t size() const noexcept { return Base::size(); }
        /**
         * @brief Getter.
         * @return const pointer to read-end of this buffer.
         * @see SerialBufferChain::getCurrentReadBuffer().
         */
        inline const void *getCurrentReadBuffer() const noexcept { return Base::getCurrentReadBuffer(); }
        /**
         * @brief Getter.
         * @return Contiguous byte count in this buffer from read-end (see getCurrentReadBuffer()).
         * @attention The returned size can be greater than this actual buffer's content size.
         * @see SerialBufferChain::getCurrentReadBufferSize().
         *
         */
        inline size_t getCurrentReadBufferSize() const noexcept { return Base::getCurrentReadBufferSize(); }
        /**
         * @brief Removes bytes from this buffer's read-end.
         * @param sz Bytes count to be removed from this buffer's read-end.
         * @see SerialBufferChain::decreaseCurrentReadBufferSize().
         */
        inline void decreaseCurrentReadBufferSize(size_t sz) noexcept { Base::decreaseCurrentReadBufferSize(sz); }
        /**
         * @brief Getter.
         * @return pointer to write-end of this buffer.
         * @see SerialBufferChain::getCurrentWriteBuffer().
         */
        inline void *getCurrentWriteBuffer() const noexcept { return Base::getCurrentWriteBuffer(); }
        /**
         * @brief Getter.
         * @return Contiguous byte count of this buffer from write-end (see getCurrentWriteBuffer()).
         * @attention The returned size is not part of this buffer's content.
         * Use increaseCurrentWriteBufferSize() to increase this buffer's content size.
         * @see SerialBufferChain::getCurrentWriteBufferSize().
         *
         */
        inline size_t getCurrentWriteBufferSize() const noexcept { return Base::getCurrentWriteBufferSize(); }
        /**
         * @brief Getter.
         * @return Get position-mark to write-end of this buffer.
         * @see SerialBufferChain::getCurrentWriteMark().
         */
        inline WriteMark getCurrentWriteMark() const noexcept { return Base::getCurrentWriteMark(); }
        /**
         * @brief Getter.
         * @param mark position-mark within read-end and write-end of this buffer (see getCurrentWriteMark()).
         * @return Contiguous byte count of this buffer from provided position-mark.
         * @see SerialBufferChain::getWriteMarkBuffer().
         */
        inline void *getWriteMarkBuffer(const WriteMark &mark) const noexcept { return Base::getWriteMarkBuffer(mark); }
        /**
         * @brief Getter.
         * @param mark position-mark within read-end and write-end of this buffer (see getCurrentWriteMark()).
         * @return Contiguous byte count in this buffer from provided position-mark.
         * @attention The returned size can be greater than this actual buffer's content size.
         * @see SerialBufferChain::getWriteMarkBufferSize().
         *
         */
        inline size_t getWriteMarkBufferSize(const WriteMark &mark) const noexcept
        {
            return Base::getWriteMarkBufferSize(mark);
        }
        /**
         * @brief Adds bytes to this buffer's write-end.
         * @param sz Bytes count to be added to this buffer's write-end.
         * @see SerialBufferChain::increaseCurrentWriteBufferSize().
         */
        inline void increaseCurrentWriteBufferSize(size_t sz)
        { // throw(std::bad_alloc)
            Base::increaseCurrentWriteBufferSize(sz);
        }
        /**
         * @brief Copies a provided source-buffer to this buffer at a provided position-mark.
         * @attention It is the user's responsibility to make sure that this buffer
         * has enough size starting the provided position-mark.
         * <br>Example:
         * \code
         * void myFunction(SerialBuffer& serialBuffer) {
         *     char sourceBuffer[] = "WHATEVER THE CONTENT";
         *     if (sizeof(sourceBuffer) > serialBuffer.getCurrentWriteBufferSize()) {
         *         SerialBuffer::WriteMark mark = getCurrentWriteMark();
         *         serialBuffer.increaseCurrentWriteBufferSize(sizeof(sourceBuffer)); // make room before using
         * copyWriteBuffer()
         *         serialBuffer.copyWriteBuffer(sourceBuffer, sizeof(sourceBuffer), mark);
         *     } else {
         *         memcpy(serialBuffer.getCurrentWriteBuffer(), sizeof(sourceBuffer), sourceBuffer);
         *         serialBuffer.increaseCurrentWriteBufferSize(sizeof(sourceBuffer));
         *     }
         * }
         * \endcode
         * @param srcBuffer source-buffer to copy from.
         * @param srcBufferSz size to copy from source-buffer.
         * @param mark This buffer's position-mark to copy at.
         * @see SerialBufferChain::copyWriteBuffer().
         */
        inline void copyWriteBuffer(const void *srcBuffer, size_t srcBufferSz, const WriteMark &mark) const noexcept
        {
            Base::copyWriteBuffer(srcBuffer, srcBufferSz, mark);
        }

      private:
        friend class AsyncEngineToEngineSerialConnector;

        SerialBuffer(AsyncActor &routeActor, size_t bufferSize) : Base(routeActor.getAllocator(), bufferSize) {}
    };
    typedef void (*EventE2EDeserializeFunction)(
        AsyncEngineToEngineConnectorEventFactory &, const void *,
        size_t); ///< Function type for cluster-event deserialization (see isE2ECapable())
    typedef void (*EventE2ESerializeFunction)(
        SerialBuffer &, const Event &); ///< Function type for cluster-event serialization (see isE2ECapable())
                                        /**
                                         * @brief Event reference placeholder used for event-name output operator specialization.
                                         */
    struct OStreamName
    {
        const Event &event; ///< const reference to an event.
                            /**
                             * @brief Constructor.
                             * @param event const reference to an event.
                             */
        inline OStreamName(const Event &pevent) noexcept : event(pevent) {}
    };
    /**
     * @brief Event reference placeholder used for event-content output operator specialization.
     */
    struct OStreamContent
    {
        const Event &event; ///< const reference to an event.
                            /**
                             * @brief Constructor.
                             * @param event const reference to an event.
                             */
        inline OStreamContent(const Event &pevent) noexcept : event(pevent) {}
    };

    /**
     * @brief Copy constructor.
     * @param other Another event to copy from.
     */
    inline Event(const Event &) noexcept : EventBase(0, 0, 0, 0), MultiForwardChainLink<Event>()
    {
        assert(sourceActorId.getNodeActorId() != 0);
    }
    /**
     * @brief Assignment operator.
     * @param other Another event to assign from.
     */
    inline Event &operator=(const Event &) noexcept { return *this; }
    /**
     * @brief Getter.
     * @return The engine (process) unique run-time id for this Event sub-class.
     */
    inline EventId getClassId() const noexcept { return classId; }
    /**
     * @brief Getter.
     * @return The place holder to output this event's name.
     * @see nameToOStream()
     */
    inline OStreamName getName() const noexcept { return OStreamName(*this); }
    /**
     * @brief Getter.
     * @return The place holder to output this event's content.
     * @see contentToOStream()
     */
    inline OStreamContent getContent() const noexcept { return OStreamContent(*this); }
    /**
     * @brief Getter.
     * @return The actor-id of the actor which served as context for this event creation.
     * @see getSourceInProcessActorId()
     * @see Pipe::push()
     * @see BufferedPipe::push()
     */
    inline ActorId getSourceActorId() const noexcept
    {
        return isRouteToSource() ? ActorId(sourceActorId, getRouteId()) : ActorId(sourceActorId);
    }
    /**
     * @brief Getter.
     * @return A const reference to the in-process-actor-id of the actor
     * which served as context for this event creation.
     * @note Whenever possible, this method should be preferred to getSourceActorId().
     * @see Pipe::push()
     * @see BufferedPipe::push()
     */
    inline const InProcessActorId &getSourceInProcessActorId() const noexcept { return sourceActorId; }
    /**
     * @brief Getter.
     * @return The destination actor-id of this event.
     * @see getDestinationInProcessActorId()
     * @see Pipe::push()
     * @see BufferedPipe::push()
     */
    inline ActorId getDestinationActorId() const noexcept
    {
        return isRouteToDestination() ? ActorId(destinationActorId, getRouteId()) : ActorId(destinationActorId);
    }
    /**
     * @brief Getter.
     * @return A const reference to the in-process-actor-id
     * of the destination actor of this event.
     * @note Whenever possible, this method should be preferred to getDestinationActorId().
     * @see Pipe::push()
     * @see BufferedPipe::push()
     */
    inline const InProcessActorId &getDestinationInProcessActorId() const noexcept { return destinationActorId; }
    /**
     * @brief Getter.
     * @attention This method can only be called if the event was routed (see isRouted()).
     * Otherwise it can cause a memory fault and lead to unpredictable behavior.
     * @return A const reference to the route-id corresponding to the connection
     * used to route this event.
     */
    inline const ActorId::RouteId &getRouteId() const noexcept
    {
        assert((routeOffset >> 1) != 0);
        return *reinterpret_cast<const ActorId::RouteId *>(reinterpret_cast<const char *>(this) - (routeOffset >> 1));
    }
    /**
     * @brief Getter.
     * @return true if this event was exchanged between two engines (processes).
     */
    inline bool isRouted() const noexcept { return routeOffset != 0; }
    /**
     * @brief Getter.
     * @attention This method should only be called if the event was routed (see isRouted()).
     * @return true if this event was routed back to its source actor.
     */
    inline bool isRouteToSource() const noexcept { return routeOffset != 0 && (routeOffset & 1) == 0; }
    /**
     * @brief Getter.
     * @attention This method should only be called if the event was routed (see isRouted()).
     * @return true if this event was routed to its destination actor.
     */
    inline bool isRouteToDestination() const noexcept { return (routeOffset & 1) == 1; }
    /**
     * @brief Static getter.
     * @return The engine (process) unique run-time id for the template generic type _Event.
     * @throw UndersizedException The number of Event sub-classes specialized at runtime exceeds the limit (4096).
     * @throw std::bad_alloc
     */
    template <class _Event> inline static EventId getClassId()
    {
        static RetainedEventId retainedEventId(_Event::nameToOStream, _Event::contentToOStream, _Event::isE2ECapable);
        return retainedEventId.eventId;
    }
    /**
     * @brief Static method to output the name of this event-base class to the provided output-stream.
     * @note This method needs to be specialized (simply override the static method) by each sub-class.
     * @param s Stream to output to.
     * @param event Event instance.
     */
    static void nameToOStream(std::ostream &s, const Event &event);
    /**
     * @brief Static method to output the content of this event-base-class
     * provided instance to the provided output-stream.
     * @note This method needs to be specialized (simply override the static method) by each sub-class.
     * It is safe to static_cast the provided event parameter to the form in which the method was overriden.
     * <br>Example:
     * \code
     * class MyEvent : public tredzone::Actor::Event {
     * public:
     *     static void nameToOStream(std::ostream& s, const Event& event) {
     *         const MyEvent& myEvent = static_cast<const MyEvent&>(event);
     *         s << myEvent;
     *     }
     * };
     * \endcode
     * @param s Stream to output to.
     * @param event Event instance.
     */
    static void contentToOStream(std::ostream &s, const Event &event);
    /**
     * @brief Static method to provide cluster-operation operators for this event-base-class.
     * @note This method needs to be specialized (simply override the static method) by each sub-class.
     * By default events are not cluster enabled.
     * @param[out] clusterId pointer to a unique cluster-id string of this event sub-class.
     * @param[out] serializeFn pointer to the serialize functor of this event sub-class.
     * @param[out] deserializeFn pointer to the deserialize functor of this event sub-class.
     * @return true if this event sub-class is cluster enabled. (default Event implementation returns false).
     */
    static bool isE2ECapable(const char *&clusterId, EventE2ESerializeFunction &serializeFn,
                             EventE2EDeserializeFunction &deserializeFn);
    /**
     * @brief Creates a string copy using an event-allocator.
     * @attention Like any allocation using Event::Allocator, no deallocation is required.
     * @param allocator event-allocator (see Pipe::getAllocator()).
     * @param s Source string to copy.
     * @return A pointer to a C-string (including null-char terminator) copy of s.
     * @throw std::bad_alloc
     */
    inline static const char *newCString(const AllocatorBase &allocator, const char *s);
    /**
     * @brief Creates a string copy using an event-allocator.
     * @attention Like any allocation using Event::Allocator, no deallocation is required.
     * @param allocator event-allocator (see Pipe::getAllocator()).
     * @param s Source string to copy.
     * @return A pointer to a C-string (including null-char terminator) copy of s.
     * @throw std::bad_alloc
     */
    inline static const char *newCString(const AllocatorBase &allocator, const AsyncActor::string_type &s);
    /**
     * @brief Creates a string copy using an event-allocator.
     * @attention Like any allocation using Event::Allocator, no deallocation is required.
     * @param allocator event-allocator (see Pipe::getAllocator()).
     * @param s Source output-string-stream to extract string and copy.
     * @return A pointer to a C-string (including null-char terminator) copy of extracted string from s.
     * @throw std::bad_alloc
     */
    inline static const char *newCString(const AllocatorBase &allocator, const AsyncActor::ostringstream_type &s);

  protected:
    /**
     * @brief Default constructor.
     */
    inline Event() noexcept : EventBase(0, 0, 0, 0) { assert(sourceActorId.getNodeActorId() != 0); }
    /**
     * @brief Destructor.
     */
    inline ~Event() noexcept {}

  private:
    friend class AsyncNodesHandle;
    friend class AsyncExceptionHandler;
    friend class AsyncEngineToEngineConnector;
    friend class AsyncEngineToEngineConnectorEventFactory;
    friend std::ostream &operator<<(std::ostream &, const AsyncActor::Event::OStreamName &);
    friend std::ostream &operator<<(std::ostream &, const AsyncActor::Event::OStreamContent &);

    typedef void (*EventToOStreamFunction)(std::ostream &, const Event &);
    typedef bool (*EventIsE2ECapableFunction)(const char *&, EventE2ESerializeFunction &,
                                              EventE2EDeserializeFunction &);
    struct RetainedEventId
    {
        const EventId eventId;
        /**
         * throw (std::bad_alloc, UndersizedException)
         */
        inline RetainedEventId(EventToOStreamFunction peventNameToOStreamFunction,
                               EventToOStreamFunction peventContentToOStreamFunction,
                               EventIsE2ECapableFunction peventIsE2ECapableFunction)
            : eventId(retainEventId(peventNameToOStreamFunction, peventContentToOStreamFunction,
                                    peventIsE2ECapableFunction))
        {
        }
        inline ~RetainedEventId() noexcept { releaseEventId(eventId); }
    };
    struct Chain : ForwardChain<0u, Chain>
    {
        inline static Event *getItem(MultiForwardChainLink<Event> *link) noexcept { return static_cast<Event *>(link); }
        inline static const Event *getItem(const MultiForwardChainLink<Event> *link) noexcept
        {
            return static_cast<const Event *>(link);
        }
        inline static MultiForwardChainLink<Event> *getLink(Event *item) noexcept
        {
            return static_cast<MultiForwardChainLink<Event> *>(item);
        }
        inline static const MultiForwardChainLink<Event> *getLink(const Event *item) noexcept
        {
            return static_cast<const MultiForwardChainLink<Event> *>(item);
        }
    };

    static EventId retainEventId(EventToOStreamFunction, EventToOStreamFunction,
                                 EventIsE2ECapableFunction); // throw (std::bad_alloc)
    static void releaseEventId(EventId) noexcept;
    static std::pair<bool, EventId> findEventId(const char *) noexcept;
    static bool isE2ECapable(EventId, const char *&, EventE2ESerializeFunction &, EventE2EDeserializeFunction &);
    static bool isE2ECapable(const char *, EventId &, EventE2ESerializeFunction &, EventE2EDeserializeFunction &);
    static void toOStream(std::ostream &, const OStreamName &);
    static void toOStream(std::ostream &, const OStreamContent &);
};
#pragma pack(pop)

/**
 * @brief Used to track event commit.
 * Using an instance of this class, it is possible to now
 * if a previously pushed event can still be amended using
 * its reference returned by:
 * - Event::Pipe::push()
 * - Event::BufferedPipe::push()
 *
 * Example:
 * \code
 * class MyActor : public tredzone::Actor, public tredzone::Actor::Callback {
 * public:
 *     struct MyEvent : tredzone::Actor::Event {
 *         unsigned count;
 *         MyEvent() :
 *             count(0) {
 *         }
 *     };
 *
 *     MyActor(const ActorId& destinationActorId) :
 *         pipeToDestinationActor(*this, destinationActorId),
 *         batch(pipeToDestinationActor, Event::Batch::IS_PUSH_COMMITTED_TRUE),
 *         lastMyEvent(0) {
 *         registerCallback(*this);
 *     }
 *
 *     void onCallback() throw() {
 *         registerCallback(*this);
 *         try {
 *             if (batch.isPushCommitted(pipeToDestinationActor)) {
 *                 lastMyEvent = &pipeToDestinationActor.push<MyEvent>();
 *             } else {
 *                 assert(lastMyEvent != 0);
 *                 ++(lastMyEvent->count);
 *             }
 *         } catch(...) {
 *         }
 *     }
 *
 * private:
 *     Event::Pipe pipeToDestinationActor;
 *     Event::Batch batch;
 *     MyEvent* lastMyEvent;
 * };
 * \endcode
 */
class AsyncActor::Event::Batch
{
  public:
    /**
     * @brief Type used to overload Batch constructor.
     */
    enum IsPushCommittedTrueEnum
    {
        IS_PUSH_COMMITTED_TRUE ///< Singleton value
    };

    /**
     * @brief Constructor.
     * Immediate (same event-loop iteration) call to isPushCommitted() returns false.
     * @param pipe event-pipe used to push events for which commit is monitored.
     */
    inline Batch(const Pipe &pipe) noexcept;
    /**
     * @brief Constructor.
     * First call to isPushCommitted() returns true.
     * @param pipe event-pipe used to push events for which commit is monitored.
     * @param IS_PUSH_COMMITTED_TRUE.
     */
    inline Batch(const Pipe &pipe, IsPushCommittedTrueEnum) noexcept;
    /**
     * @brief Checks whether the previously pushed event can still be amended.
     * If the previously pushed event cannot be amended, a new push must be performed
     * and a new event reference must be stored.
     * If true was returned, then an immediate following call (in the same event-loop iteration)
     * to this method will return false. In this case, assuming a new event was pushed,
     * next event-loop iteration(s) will eventually make this method return true. And so on.
     * @note To start a new Pipe::push() / Batch::isPushCommitted() cycle, just assign your
     * batch-instance with a new batch-instance, immediately in the same event-loop iteration
     * as Pipe::push(). Example:
     * \code
     * class MyEvent : public tredzone::Actor::Event {
     * };
     *
     * MyEvent& firstCycleIteration(tredzone::Actor::Event::Pipe& pipe, tredzone::Actor::Event::Batch& batch) {
     *     batch = tredzone::Actor::Event::Batch(pipe); // this call initializes batch with respect to a push occurring in
     * the same event-loop iteration
     *     return pipe.push<MyEvent>(); // push occurring in the same event-loop iteration as batch initialization
     * }
     *
     * MyEvent& nextCycleIteration(MyEvent& myCurrentEvent, tredzone::Actor::Event::Pipe& pipe, tredzone::Actor::Event::Batch&
     * batch) {
     *     if (batch.isPushCommitted(pipe)) {
     *         return pipe.push<MyEvent>();
     *     } else {
     *         return myCurrentEvent;
     *     }
     * }
     * \endcode
     * @param pipe event-pipe with the same event-loop (cpu-core) destination as the pipe used
     * at initialization.
     * @return false if the previously pushed event can still be amended.
     */
    inline bool isPushCommitted(const Pipe &pipe) noexcept;

  private:
    friend class AllocatorBase;

    uint64_t batchId;

    inline bool checkHasChanged(uint64_t) noexcept;
    inline uint64_t getCurrentBatchId(const Pipe &) noexcept;
    inline void forceChange(const Pipe &) noexcept;

#ifndef NDEBUG
    typedef uint64_t (*DebugBatchFn)(void *, bool);
    void *debugContext;
    DebugBatchFn debugBatchFn;
    Batch() noexcept;
    Batch(void *, DebugBatchFn) noexcept;
    bool debugCheckHasChanged() noexcept;
#endif
};

/**
 * @brief Non-template base-class of stl-compliant AsyncActor::Event::Allocator.
 *
 * This is a local allocator construct, with no global context.<br>
 * Context is obtained using an Event::Pipe. Context depends on pipe's
 * origin actor and destination actor-id.
 * It is not thread-safe. Although a protected default constructor exists for stl-compliance,
 * an allocation attempt using a default-constructed instance will always throw an std::bad_alloc exception.<br>
 * A usable instance of this class can only be obtained from
 * the AsyncActor::Event::Pipe::getAllocator() factory-method.
 * @attention Using this allocator, allocated space is transient
 * and only valid during event transmission, it is then recycled for
 * the next batch (see Batch) of events' transmission. Hense
 * deallocation is not required.
 */
class AsyncActor::Event::AllocatorBase
{
  public:
    /**
     * @brief Compare 2 allocators based on there contexts.<br>
     * Two allocators with the same contexts can be used indifferently
     * with respect to allocation operations.
     * @param other reference to another allocator instance.
     * @return true if the 2 allocators have the same context.
     */
    inline bool operator==(const AllocatorBase &other) const noexcept { return factory == other.factory; }
    /**
     * @brief Compare 2 allocators based on there contexts.<br>
     * Two allocators with the same contexts can be used indifferently
     * with respect to allocation operations.
     * @param other reference to another allocator instance.
     * @return true if the 2 allocators have different contexts.
     */
    inline bool operator!=(const AllocatorBase &other) const noexcept { return factory != other.factory; }

  protected:
    /**
     * @brief Default constructor.
     * @note As explained in the class-description, this constructor should not be used.
     */
    inline AllocatorBase() noexcept;
    /**
     * @brief Constructor with context.
     * @param pipe context.
     */
    inline AllocatorBase(const Pipe &pipe) noexcept;
    /**
     * @brief Allocates a bloc of memory.
     * @param sz byte count of the requested memory size.
     * @return A pointer to the allocated memory bloc.
     * @throw std::bad_alloc
     */
    inline void *allocate(size_t sz);
    /**
     * @brief Allocates a bloc of memory, and provide page information.
     * @param sz byte count of the requested memory size.
     * @param[out] eventPageIndex index of the reusable memory page. Starts at 0 and increments by one.
     * @param[out] eventPageOffset byte count starting offset of the allocated bloc within the memory page.
     * @return A pointer to the allocated memory bloc.
     * @throw std::bad_alloc
     */
    inline void *allocate(size_t sz, uint32_t &eventPageIndex, size_t &eventPageOffset);
    /**
     * @brief Getter.
     * @return the maximum byte count that can be allocated in a single call to allocate().
     * @see AsyncEngine::getEventAllocatorPageSizeByte().
     */
    size_t max_size() const noexcept;

  private:
    friend class Pipe;
    friend class AsyncEngineToEngineConnectorEventFactory;
    struct Factory
    {
        typedef void *(*AllocateFn)(size_t, void *);
        typedef void *(*AllocateAndGetIndexFn)(size_t, void *, uint32_t &, size_t &);
        void *context; // valid pointer throughout the node lifetime
        AllocateFn allocateFn;
        AllocateAndGetIndexFn allocateAndGetIndexFn;
        inline Factory() noexcept
        {
#ifndef NDEBUG
            context = 0;
            allocateFn = 0;
            allocateAndGetIndexFn = 0;
#endif
        }
        inline Factory(void *pcontext, AllocateFn pallocateFn, AllocateAndGetIndexFn pallocateAndGetIndexFn) noexcept
            : context(pcontext),
              allocateFn(pallocateFn),
              allocateAndGetIndexFn(pallocateAndGetIndexFn)
        {
        }
    };

    const Factory *const factory;
#ifndef NDEBUG
    AsyncActor::Event::Batch debugEventBatch;
#endif

    inline AllocatorBase(const Factory &) noexcept;
#ifndef NDEBUG
    inline AllocatorBase(const Factory &, AsyncActor::Event::Batch::DebugBatchFn) noexcept;
#endif
};

/**
 * @brief STL-compliant allocator template based on AsyncActor::Event::AllocatorBase.
 *
 * This is a nested template class. Therefore, there is no Allocator<void> specialization,
 * as nested template class specialization is forbidden by the language.
 */
template <class T> class AsyncActor::Event::Allocator : public AsyncActor::Event::AllocatorBase
{
  public:
    typedef T value_type;              ///< STL-compliant
    typedef size_t size_type;          ///< STL-compliant
    typedef ptrdiff_t difference_type; ///< STL-compliant
    typedef T *pointer;                ///< STL-compliant
    typedef const T *const_pointer;    ///< STL-compliant
    typedef T &reference;              ///< STL-compliant
    typedef const T &const_reference;  ///< STL-compliant

    /**
     * @brief STL-compliant.
     */
    template <class U> struct rebind
    {
        typedef Allocator<U> other; ///< STL-compliant
    };

    /**
     * @brief Default constructor which follows the same rules as parent default constructor
     * AllocatorBase::AllocatorBase(). This constructor should not be used, and is only there for stl-compliance.
     */
    inline Allocator() noexcept : AllocatorBase() {}
    /**
     * @brief Default copy constructor.
     * @param other allocator to copy context from.
     */
    inline Allocator(const AllocatorBase &other) noexcept : AllocatorBase(other) {}
    /**
     * @brief STL-compliant.
     */
    inline pointer address(reference r) const { return &r; }
    /**
     * @brief STL-compliant.
     */
    inline const_pointer address(const_reference r) const { return &r; }
    /**
     * @brief Allocates an array of T entry-type in the current event-batch dedicated memory.
     * @param n entry count in the array to be allocated
     * @param hint not used (for stl-compliance)
     * @return A pointer to the allocated array
     * @throw std::bad_alloc
     */
    inline pointer allocate(size_type n, const void * = 0)
    {
        return static_cast<pointer>(AllocatorBase::allocate(n * sizeof(T)));
    }
    /**
     * @brief For STL-compliance. Does not do anything.
     */
    inline void deallocate(pointer, size_type) noexcept {}
/**
 * @brief STL-compliant.
 */
#ifdef TREDZONE_CPP11_SUPPORT
    template <class U, class... Args> void construct(U *p, Args &&... args)
    {
        new ((void *)p) U(std::forward<Args>(args)...);
    }
#else
    inline void construct(pointer p, const T &init) { new (p) T(init); }
#endif

    /**
     * @brief STL-compliant.
     */
    inline void destroy(pointer p) { p->~T(); }
    /**
     * @brief Getter.
     * @return the maximum byte count that can be allocated in a single call to allocate().
     * @see AsyncEngine::getEventAllocatorPageSizeByte().
     */
    inline size_type max_size() const noexcept { return AllocatorBase::max_size() / sizeof(T); }
};

/**
 * @brief Event factory class.
 * It connects 2 actors, one (source) on the current event-loop,
 * the other (destination) on any event-loop (including another engine (process)).
 *
 * Using the push() method, classes publicly deriving from Event are created an transmitted
 * to destination actor (see AsyncActor::registerEventHandler()).
 * Destination actor is only identified by its actor-id, it can be invalid.
 * If actor-id does not refer to a valid actor, pushed events are returned to
 * the source actor (see AsyncActor::registerUndeliveredEventHandler()).
 * <br>Should the pushed event embed any memory allocating container,
 * it is imperative that Event::Allocator is used. Indeed events are shared
 * between two event-loops (cpu-cores), and demand that memory is managed
 * accordingly. Event::Allocator is the only allocator that guarantees
 * thread-safety with optimum performances. Event::Allocator has to be initialized
 * using the very Event::Pipe instance with which the event was pushed.
 */
class AsyncActor::Event::Pipe
{
  public:
    /**
     * @brief Constructor.
     * @param sourceActor source actor.
     * @param destinationActorId destination actor-id.
     */
    Pipe(AsyncActor &sourceActor, const ActorId &destinationActorId = ActorId()) noexcept;
    /**
     * @brief Copy constructor.
     * @param other another pipe.
     */
    inline Pipe(const Pipe &other) noexcept : sourceActor(other.sourceActor),
                                              destinationActorId(other.destinationActorId),
                                              asyncNode(other.asyncNode),
                                              eventFactory(other.eventFactory)
    {
#ifndef NDEBUG
        ++sourceActor.debugPipeCount;
#endif
        if (destinationActorId.isInProcess() == false)
        {
            ++sourceActor.processOutPipeCount;
        }
    }
    /**
     * @brief Destructor.
     */
    inline ~Pipe() noexcept
    {
#ifndef NDEBUG
        assert(sourceActor.debugPipeCount > 0);
        --sourceActor.debugPipeCount;
#endif
        unregisterProcessOutPipe();
    }
    /**
     * @brief Getter to local event allocator.
     * @return A copy of a local event allocator.
     */
    inline Event::AllocatorBase getAllocator() const noexcept { return Event::AllocatorBase(*this); }
    /**
     * @brief Getter.
     * @return actor-id of the source actor.
     */
    inline const ActorId &getSourceActorId() const noexcept { return sourceActor.getActorId(); }
    /**
     * @brief Getter.
     * @return actor-id of the destination actor.
     */
    inline const ActorId &getDestinationActorId() const noexcept { return destinationActorId; }
    /**
     * @brief Changes the destination actor-id.
     * @param destinationActorId actor-id of the new destination actor.
     */
    void setDestinationActorId(const ActorId &destinationActorId) noexcept;
    /**
     * @brief Creates a new instance of the template generic type _Event
     * using its default constructor.
     * _Event must publicly inherit from Event and have a public default constructor
     * (constructor with no arguments).
     * The created event is transmitted to pipe's destination actor (see AsyncActor::registerEventHandler()).
     * If the transmission was unsuccessful (e.g. invalid destination actor), the event will
     * be returned to pipe's source actor (see AsyncActor::registerUndeliveredEventHandler()).
     * @attention The newly created event's reference remains valid until the current event-batch
     * is committed (see Batch). Under batch-control, future access to that reference
     * is safe.
     * @return A reference to the newly created event.
     * @throw std::bad_alloc
     * @throw ? Any other exception possibly thrown depending on _Event (the template generic type)
     * constructor call.
     */
    template <class _Event> inline _Event &push()
    {
        EventChain *destinationEventChain;
        _Event *ret = newEvent<_Event>(destinationEventChain);
        destinationEventChain->push_back(ret);
        return *ret;
    }
    /**
     * @brief Creates a new instance of the template generic type _Event
     * using its one-argument constructor.
     * _Event must publicly inherit from Event and have a public one-argument constructor.
     * The created event is transmitted to pipe's destination actor (see AsyncActor::registerEventHandler()).
     * If the transmission was unsuccessful (e.g. invalid destination actor), the event will
     * be returned to pipe's source actor (see AsyncActor::registerUndeliveredEventHandler()).
     * @attention The newly created event's reference remains valid until the current event-batch
     * is committed (see Batch). Under batch-control, future access to that reference
     * is safe.
     * @param eventInit parameter to be passed as argument to the constructor of the new event.
     * @return A reference to the newly created event.
     * @throw std::bad_alloc
     * @throw ? Any other exception possibly thrown depending on _Event (the template generic type)
     * constructor call.
     */

    template <class _Event, class _EventInit> inline _Event &push(const _EventInit &eventInit)
    { // throw (std::bad_alloc, ...)
        EventChain *destinationEventChain;
        _Event *ret = newEvent<_Event>(destinationEventChain, eventInit);
        destinationEventChain->push_back(ret);
        return *ret;
    }
    /**
     * @brief Allocates an array of T entry-type in the current event-batch dedicated memory.
     * @note equivalent to (with less overhead):
     * \code
     * Event::Allocator<T>(this->getAllocator()).allocate(n);
     * \endcode
     * @param n entry count in the array to be allocated
     * @return A pointer to the allocated array
     * @throw std::bad_alloc
     */
    template <class T> inline T *allocate(size_t n)
    {
        return static_cast<T *>((*eventFactory.allocateFn)(n * sizeof(T), eventFactory.context));
    }
    /**
     * @brief Allocates an array of T entry-type in the current event-batch dedicated memory,
     * and provide page information.
     * @param n entry count in the array to be allocated
     * @param[out] eventPageIndex index of the reusable memory page. Starts at 0 and increments by one.
     * @param[out] eventPageOffset byte count starting offset of the allocated bloc within the memory page.
     * @return A pointer to the allocated array
     * @throw std::bad_alloc
     */
    template <class T> inline T *allocate(size_t n, uint32_t &eventPageIndex, size_t &eventPageOffset)
    { // throw (std::bad_alloc, ...)
        return static_cast<T *>((*eventFactory.allocateAndGetIndexFn)(n * sizeof(T), eventFactory.context,
                                                                      eventPageIndex, eventPageOffset));
    }

  private:
    friend class AllocatorBase;
    friend class Batch;
    friend class BufferedPipe;
    friend class AsyncEngineToEngineConnectorEventFactory;
    typedef Event::Chain EventChain;

    template <class _Event> struct EventWrapper : virtual private EventBase, _Event
    {
        inline EventWrapper(const Pipe &eventPipe, route_offset_type routeOffset)
            : EventBase(Event::getClassId<_Event>(), eventPipe.sourceActor.getActorId(), eventPipe.destinationActorId,
                        routeOffset)
        {
        }
        template<class _EventInit> inline EventWrapper(const Pipe& eventPipe,
				const _EventInit& eventInit, route_offset_type routeOffset) :
				EventBase(Event::getClassId<_Event>(), eventPipe.sourceActor.getActorId(), eventPipe.destinationActorId, routeOffset), _Event(eventInit) {}
    };
    struct EventFactory : AllocatorBase::Factory
    {
        typedef void *(Pipe::*NewFn)(size_t, EventChain *&, uintptr_t, Event::route_offset_type &);
#ifndef NDEBUG
        typedef uint64_t (*BatchFn)(void *, bool);
#else
        typedef uint64_t (*BatchFn)(void *);
#endif
        NewFn newFn;
        BatchFn batchFn;
        inline EventFactory(void *pcontext, NewFn pnewFn, AllocateFn pallocateFn,
                            AllocateAndGetIndexFn pallocateAndGetIndexFn, BatchFn pbatchFn) noexcept
            : AllocatorBase::Factory(pcontext, pallocateFn, pallocateAndGetIndexFn),
              newFn(pnewFn),
              batchFn(pbatchFn)
        {
        }
    };

    AsyncActor &sourceActor;
    ActorId destinationActorId;
    AsyncNode &asyncNode;
    EventFactory eventFactory;

    Pipe &operator=(const Pipe &);
    inline EventFactory getEventFactory() noexcept;
    inline void registerProcessOutPipe() noexcept;
    inline void unregisterProcessOutPipe() noexcept
    {
        if (destinationActorId.isInProcess() == false)
        {
            assert(sourceActor.processOutPipeCount > 0);
            --sourceActor.processOutPipeCount;
        }
    }
    void *newInProcessEvent(size_t, EventChain *&, uintptr_t, Event::route_offset_type &);    // throw (std::bad_alloc)
    void *newOutOfProcessEvent(size_t, EventChain *&, uintptr_t, Event::route_offset_type &); // throw (std::bad_alloc)
    inline void *newOutOfProcessEvent(void *, size_t, EventChain *&, uintptr_t,
                                      Event::route_offset_type &); // throw (std::bad_alloc)
    void *newOutOfProcessSharedMemoryEvent(size_t, EventChain *&, uintptr_t,
                                           Event::route_offset_type &);                       // throw (std::bad_alloc)
    static void *allocateInProcessEvent(size_t, void *);                                      // throw (std::bad_alloc)
    static void *allocateInProcessEvent(size_t, void *, uint32_t &, size_t &);                // throw (std::bad_alloc)
    static void *allocateOutOfProcessSharedMemoryEvent(size_t, void *);                       // throw (std::bad_alloc)
    static void *allocateOutOfProcessSharedMemoryEvent(size_t, void *, uint32_t &, size_t &); // throw (std::bad_alloc)
#ifndef NDEBUG
    static uint64_t batchInProcessEvent(void *, bool) noexcept;
    static uint64_t batchOutOfProcessSharedMemoryEvent(void *, bool) noexcept;
#else
    static uint64_t batchInProcessEvent(void *) noexcept;
    static uint64_t batchOutOfProcessSharedMemoryEvent(void *) noexcept;
#endif
    template <class _Event> inline _Event *newEvent(EventChain *&destinationEventChain)
    { // throw (std::bad_alloc, ...)
        Event::route_offset_type routeOffset = 0;
        _Event *ret =
            new ((this->*eventFactory.newFn)(sizeof(EventWrapper<_Event>), destinationEventChain,
                                             (uintptr_t) static_cast<Event *>((EventWrapper<_Event> *)0), routeOffset))
                EventWrapper<_Event>(*this, routeOffset);
        return ret;
    }
    template<class _Event, class _EventInit> inline _Event* newEvent(EventChain*& destinationEventChain,
				const _EventInit& eventInit) // throw (std::bad_alloc, ...)
    { // throw (std::bad_alloc, ...)
        Event::route_offset_type routeOffset = 0;
        _Event *ret =
            new ((this->*eventFactory.newFn)(sizeof(EventWrapper<_Event>), destinationEventChain,
                                             (uintptr_t) static_cast<Event *>((EventWrapper<_Event> *)0), routeOffset))
                EventWrapper<_Event>(*this, eventInit,routeOffset);
        return ret;
    }
};

/**
 * @brief Event factory class extending Event::Pipe.
 * <br>Like Event::Pipe, it connects 2 actors, one (source) on the current event-loop,
 * the other (destination) on any event-loop (including another engine (process)).
 * To Event::Pipe, it adds the ability to cancel event-pushes.
 * To that end, two new methods were introduced: flush() and clear().
 *
 * Using the push() method, classes publicly deriving from Event are created an transmitted
 * to destination actor (see AsyncActor::registerEventHandler()).
 * Destination actor is only identified by its actor-id, it can be invalid.
 * If actor-id does not refer to a valid actor, pushed events are returned to
 * the source actor (see AsyncActor::registerUndeliveredEventHandler()).
 * <br>Should the pushed event embed any memory allocating container,
 * it is imperative that Event::Allocator is used. Indeed events are shared
 * between two event-loops (cpu-cores), and demand that memory is managed
 * accordingly. Event::Allocator is the only allocator that guarantees
 * thread-safety with optimum performances. Event::Allocator has to be initialized
 * using the very Event::BufferedPipe instance with which the event was pushed.
 *
 * Using an instance of Event::BufferedPipe, it is possible to create
 * atomic grouped event pushes involving multiple pipes: either
 * all push calls succeed (no thrown exception) or none is performed.
 * <br>Example:
 * \code
 * class MyEvent : public tredzone::Actor::Event {
 * };
 *
 * void atomicPush(tredzone::Actor& source, const tredzone::Actor::ActorId& firstDestination, const tredzone::Actor::ActorId&
 * secondDestination) {
 *     tredzone::Actor::Event::BufferedPipe bufferedPipe(source, firstDestination);
 *     bufferedPipe.push<MyEvent>(); // creates and stores the event, without actually pushing it
 *     tredzone::Actor::Event::Pipe(source, secondDestination).push<MyEvent>(); // if this push throws an exception, the
 * buffered-pipe stored event will not be released, hense not actually pushed
 *     // both pushes went through, we can flush bufferedPipe
 *     bufferedPipe.flush(); // actually pushes the stored event
 *
 * }
 * \endcode
 */
class AsyncActor::Event::BufferedPipe : public AsyncActor::Event::Pipe
{
  public:
    /**
     * @brief Thrown if stored events are no more valid due to
     * internal BufferedPipe batch-control.
     */
    struct MixedBatchBufferedEventsException : std::exception
    {
        virtual const char *what() const noexcept
        {
            return "tredzone::AsyncActor::Event::BufferedPipe::MixedBatchBufferedEventsException";
        }
    };

    /**
     * @brief Constructor.
     * @param sourceActor source actor.
     * @param destinationActorId destination actor-id.
     */
    inline BufferedPipe(AsyncActor &asyncActor, const ActorId &destinationActorId) noexcept
        : Pipe(asyncActor, destinationActorId),
          batch(*this),
          destinationEventChain(0)
    {
    }
    /**
     * @brief Copy constructor.
     * @param other another pipe.
     */
    inline BufferedPipe(const Pipe &pipe) noexcept : Pipe(pipe), batch(*this), destinationEventChain(0) {}
    /**
     * @brief Creates a new instance of the template generic type _Event
     * using its default constructor.
     * _Event must publicly inherit from Event and have a public default constructor
     * (constructor with no arguments).
     * The created event is stored for later transfer (see flush()) to pipe's destination actor
     * (see AsyncActor::registerEventHandler()).
     * Eventually, if the transmission was unsuccessful (e.g. invalid destination actor),
     * the event will be returned to pipe's source actor (see AsyncActor::registerUndeliveredEventHandler()).
     * @attention The newly created event's reference remains valid until the current event-batch
     * is committed (see Batch). Under batch-control, future access to that reference
     * is safe.
     * @return A reference to the newly created event.
     * @throw MixedBatchBufferedEventsException Existing non-flushed/cleared Event
     * that had been pushed in a different event-batch than this push.
     * @throw std::bad_alloc
     * @throw ? Any other exception possibly thrown depending on _Event (the template generic type)
     * constructor call.
     */
    template <class _Event> inline _Event &push()
    {
        if (batch.isPushCommitted(*this) && !eventChain.empty())
        {
            clear();
            throw MixedBatchBufferedEventsException();
        }
#ifndef NDEBUG
        EventChain *oldDestinationEventChain = destinationEventChain;
#endif
        _Event *ret = newEvent<_Event>(destinationEventChain);
        assert(oldDestinationEventChain == 0 || oldDestinationEventChain == destinationEventChain);
        eventChain.push_back(ret);
        return *ret;
    }
    /**
     * @brief Creates a new instance of the template generic type _Event
     * using its one-argument constructor.
     * _Event must publicly inherit from Event and have a public one-argument constructor.
     * The created event is stored for later transfer (see flush()) to pipe's destination actor
     * (see AsyncActor::registerEventHandler()).
     * Eventually, if the transmission was unsuccessful (e.g. invalid destination actor),
     * the event will be returned to pipe's source actor (see AsyncActor::registerUndeliveredEventHandler()).
     * @attention The newly created event's reference remains valid until the current event-batch
     * is committed (see Batch). Under batch-control, future access to that reference
     * is safe.
     * @param eventInit parameter to be passed as argument to the constructor of the new event.
     * @return A reference to the newly created event.
     * @throw MixedBatchBufferedEventsException Existing non-flushed/cleared Event
     * that had been pushed in a different event-batch than this push.
     * @throw std::bad_alloc
     * @throw ? Any other exception possibly thrown depending on _Event (the template generic type)
     * constructor call.
     */
    template <class _Event, class _EventInit> inline _Event &push(const _EventInit &eventInit)
    {
        if (batch.isPushCommitted(*this) && !eventChain.empty())
        {
            clear();
            throw MixedBatchBufferedEventsException();
        }
#ifndef NDEBUG
        EventChain *oldDestinationEventChain = destinationEventChain;
#endif
        _Event *ret = newEvent<_Event>(destinationEventChain, eventInit);
        assert(oldDestinationEventChain == 0 || oldDestinationEventChain == destinationEventChain);
        eventChain.push_back(ret);
        return *ret;
    }
    /**
     * @brief Releases all events previously created using push().
     * The events can be received by the pipe's destination actor.
     * @attention Between Two consecutive flushes, all event-pushes
     * must occur within the same event-batch (see Batch).
     * This is naturally guaranteed if all event-pushes and this flush are
     * performed within the same event-loop iteration.
     * Otherwise, a batch-control is required.
     * @throw MixedBatchBufferedEventsException Event have been pushed
     * in a different event-batch than this flush.
     * @see Batch
     */
    inline void flush()
    {
        if (!eventChain.empty())
        {
            if (batch.isPushCommitted(*this))
            {
                clear();
                throw MixedBatchBufferedEventsException();
            }
            assert(destinationEventChain != 0);
            destinationEventChain->push_back(eventChain);
        }
    }
    /**
     * @brief Clears all events previously created using push().
     * No event can be received by the pipe's destination actor.
     */
    inline void clear() noexcept
    {
        EventChain emptyEventChain;
        eventChain.swap(emptyEventChain);
    }
    /**
     * @brief Changes the destination actor-id.
     * @param destinationActorId actor-id of the new destination actor.
     */
    inline void setDestinationActorId(const ActorId &destinationActorId) noexcept
    {
        clear();
        Pipe::setDestinationActorId(destinationActorId);
        batch.isPushCommitted(*this);
    }

  private:
    Batch batch;
    EventChain *destinationEventChain;
    EventChain eventChain;

    BufferedPipe(const BufferedPipe &);
    BufferedPipe &operator=(const BufferedPipe &);
};

template <class _Event, class _EventHandler> struct AsyncActor::StaticEventHandler
{
    static bool onEvent(void *eventHandler, const Event &event)
    {
        assert(event.getClassId() == Event::getClassId<_Event>());
        assert(eventHandler != 0);
        static_cast<_EventHandler *>(eventHandler)->onEvent(static_cast<const _Event &>(event));
        return true;
    }
    static bool onUndeliveredEvent(void *eventHandler, const Event &event)
    {
        assert(event.getClassId() == Event::getClassId<_Event>());
        assert(eventHandler != 0);
        static_cast<_EventHandler *>(eventHandler)->onUndeliveredEvent(static_cast<const _Event &>(event));
        return true;
    }
};

template <class _Callback> struct AsyncActor::StaticCallbackHandler
{
    static void onCallback(Callback &callback) noexcept { static_cast<_Callback &>(callback).onCallback(); }
};

template <class _Callback> void AsyncActor::registerCallback(_Callback &callback) noexcept
{
    registerCallback(StaticCallbackHandler<_Callback>::onCallback, callback);
}

template <class _Callback> void AsyncActor::registerPerformanceNeutralCallback(_Callback &callback) noexcept
{
    registerPerformanceNeutralCallback(StaticCallbackHandler<_Callback>::onCallback, callback);
}

template <class _Event, class _EventHandler> void AsyncActor::registerEventHandler(_EventHandler &eventHandler)
{
    if (isRegisteredEventHandler<_Event>())
    {
        throw AlreadyRegisterdEventHandlerException();
    }
    registerHighPriorityEventHandler(Event::getClassId<_Event>(), &eventHandler,
                                     StaticEventHandler<_Event, _EventHandler>::onEvent);
}

template <class _Event, class _EventHandler>
void AsyncActor::registerUndeliveredEventHandler(_EventHandler &eventHandler)
{
    if (isRegisteredUndeliveredEventHandler<_Event>())
    {
        throw AlreadyRegisterdEventHandlerException();
    }
    registerUndeliveredEventHandler(Event::getClassId<_Event>(), &eventHandler,
                                    StaticEventHandler<_Event, _EventHandler>::onUndeliveredEvent);
}

template <class _Event> void AsyncActor::unregisterEventHandler() noexcept
{
    unregisterEventHandler(Event::getClassId<_Event>());
}

template <class _Event> void AsyncActor::unregisterUndeliveredEventHandler() noexcept
{
    unregisterUndeliveredEventHandler(Event::getClassId<_Event>());
}

template <class _Event> bool AsyncActor::isRegisteredEventHandler() const noexcept
{
    return isRegisteredEventHandler(Event::getClassId<_Event>());
}

template <class _Event> bool AsyncActor::isRegisteredUndeliveredEventHandler() const noexcept
{
    return isRegisteredUndeliveredEventHandler(Event::getClassId<_Event>());
}

template <class _AsyncActor>
AsyncActor::ActorReference<_AsyncActor> AsyncActor::referenceLocalActor(const ActorId &pactorId)
{
    return ActorReference<_AsyncActor>(*this, dynamic_cast<_AsyncActor &>(getReferenceToLocalActor(pactorId)), false);
}

template <class _AsyncActor> AsyncActor::ActorReference<_AsyncActor> AsyncActor::newReferencedActor()
{
    _AsyncActor &actor = newActor<_AsyncActor>(*asyncNode);
    try
    {
        return ActorReference<_AsyncActor>(*this, actor);
    }
    catch (CircularReferenceException &)
    {
        actor.requestDestroy();
        throw;
    }
}

template <class _AsyncActor, class _AsyncActorInit>
AsyncActor::ActorReference<_AsyncActor> AsyncActor::newReferencedActor(const _AsyncActorInit &init)
{
    _AsyncActor &actor = newActor<_AsyncActor>(*asyncNode, init);
    try
    {
        return ActorReference<_AsyncActor>(*this, actor);
    }
    catch (CircularReferenceException &)
    {
        actor.requestDestroy();
        throw;
    }
}

template <class _AsyncActor> AsyncActor::ActorReference<_AsyncActor> AsyncActor::newReferencedSingletonActor()
{
    SingletonActorIndex singletonActorIndex = getSingletonActorIndex<_AsyncActor>();
    AsyncActor *actor = getSingletonActor(singletonActorIndex);
    if (actor != 0)
    {
        assert(dynamic_cast<_AsyncActor *>(actor) != 0);
        return ActorReference<_AsyncActor>(*this, static_cast<_AsyncActor &>(*actor));
    }
    reserveSingletonActor(singletonActorIndex);
    try
    {
        ActorReference<_AsyncActor> ret = newReferencedActor<_AsyncActor>();
        setSingletonActor(singletonActorIndex, *ret);
        return ret;
    }
    catch (...)
    {
        unsetSingletonActor(singletonActorIndex);
        throw;
    }
}

template <class _AsyncActor, class _AsyncActorInit>
AsyncActor::ActorReference<_AsyncActor> AsyncActor::newReferencedSingletonActor(const _AsyncActorInit &init)
{
    SingletonActorIndex singletonActorIndex = getSingletonActorIndex<_AsyncActor>();
    AsyncActor *actor = getSingletonActor(singletonActorIndex);
    if (actor != 0)
    {
        assert(dynamic_cast<_AsyncActor *>(actor) != 0);
        return ActorReference<_AsyncActor>(*this, static_cast<_AsyncActor &>(*actor));
    }
    reserveSingletonActor(singletonActorIndex);
    try
    {
        ActorReference<_AsyncActor> ret = newReferencedActor<_AsyncActor, _AsyncActorInit>(init);
        setSingletonActor(singletonActorIndex, *ret);
        return ret;
    }
    catch (...)
    {
        unsetSingletonActor(singletonActorIndex);
        throw;
    }
}

template <class _AsyncActor> const AsyncActor::ActorId &AsyncActor::newUnreferencedActor()
{
    return newActor<_AsyncActor>(*asyncNode).getActorId();
}

template <class _AsyncActor, class _AsyncActorInit>
const AsyncActor::ActorId &AsyncActor::newUnreferencedActor(const _AsyncActorInit &init)
{
    return newActor<_AsyncActor>(*asyncNode, init).getActorId();
}

template <class _AsyncActor> _AsyncActor &AsyncActor::newActor(AsyncNode &asyncNode)
{
    return *new (AllocatorBase(asyncNode).allocate(sizeof(ActorWrapper<_AsyncActor>)))
        ActorWrapper<_AsyncActor>(asyncNode);
}

template <class _AsyncActor, class _AsyncActorInit>
_AsyncActor &AsyncActor::newActor(AsyncNode &asyncNode, const _AsyncActorInit &init)
{
    return *new (AllocatorBase(asyncNode).allocate(sizeof(ActorWrapper<_AsyncActor>)))
        ActorWrapper<_AsyncActor>(asyncNode, init);
}

const char *AsyncActor::Event::newCString(const AllocatorBase &a, const char *s)
{
    size_t sz = std::strlen(s) + 1;
    char *ret = Allocator<char>(a).allocate(sz);
    std::memcpy(ret, s, sz);
    return ret;
}

const char *AsyncActor::Event::newCString(const AllocatorBase &a, const AsyncActor::string_type &s)
{
    size_t sz = s.size() + 1;
    char *ret = Allocator<char>(a).allocate(sz);
    std::memcpy(ret, s.c_str(), sz);
    return ret;
}

const char *AsyncActor::Event::newCString(const AllocatorBase &a, const AsyncActor::ostringstream_type &s)
{
    size_t sz = s.size() + 1;
    char *ret = Allocator<char>(a).allocate(sz);
    std::memcpy(ret, s.c_str(), sz);
    return ret;
}

AsyncActor::Event::AllocatorBase::AllocatorBase() noexcept : factory(0) {}

AsyncActor::Event::AllocatorBase::AllocatorBase(const Pipe &peventPipe) noexcept : factory(&peventPipe.eventFactory)
#ifndef NDEBUG
                                                                                       ,
                                                                                   debugEventBatch(peventPipe)
#endif
{
}

AsyncActor::Event::AllocatorBase::AllocatorBase(const Factory &pfactory) noexcept : factory(&pfactory) {}

#ifndef NDEBUG
AsyncActor::Event::AllocatorBase::AllocatorBase(const Factory &pfactory,
                                                AsyncActor::Event::Batch::DebugBatchFn pdebugBatchFn) noexcept
    : factory(&pfactory),
      debugEventBatch(pfactory.context, pdebugBatchFn)
{
}
#endif

void *AsyncActor::Event::AllocatorBase::allocate(size_t sz)
{ // throw (std::bad_alloc)
    assert(factory != 0);
    assert(!debugEventBatch.debugCheckHasChanged());
    if (factory == 0)
    {
        throw std::bad_alloc();
    }
    return (*factory->allocateFn)(sz, factory->context);
}

void *AsyncActor::Event::AllocatorBase::allocate(size_t sz, uint32_t &eventPageIndex, size_t &eventPageOffset)
{ // throw (std::bad_alloc)
    assert(factory != 0);
    assert(!debugEventBatch.debugCheckHasChanged());
    if (factory == 0)
    {
        throw std::bad_alloc();
    }
    return (*factory->allocateAndGetIndexFn)(sz, factory->context, eventPageIndex, eventPageOffset);
}

AsyncActor::Event::Batch::Batch(const Pipe &eventPipe) noexcept : batchId(getCurrentBatchId(eventPipe))
#ifndef NDEBUG
                                                                      ,
                                                                  debugContext(eventPipe.eventFactory.context),
                                                                  debugBatchFn(eventPipe.eventFactory.batchFn)
#endif
{
}

AsyncActor::Event::Batch::Batch(const Pipe &eventPipe, IsPushCommittedTrueEnum) noexcept
    : batchId(getCurrentBatchId(eventPipe))
#ifndef NDEBUG
          ,
      debugContext(eventPipe.eventFactory.context),
      debugBatchFn(eventPipe.eventFactory.batchFn)
#endif
{
    Batch::forceChange(eventPipe);
}

bool AsyncActor::Event::Batch::checkHasChanged(uint64_t currentBatchId) noexcept
{
    return (currentBatchId != batchId || currentBatchId == std::numeric_limits<uint64_t>::max());
}

uint64_t AsyncActor::Event::Batch::getCurrentBatchId(const Pipe &eventPipe) noexcept
{
#ifndef NDEBUG
    debugContext = eventPipe.eventFactory.context;
#endif
    return (*eventPipe.eventFactory.batchFn)(eventPipe.eventFactory.context
#ifndef NDEBUG
                                             ,
                                             true
#endif
                                             );
}

bool AsyncActor::Event::Batch::isPushCommitted(const Pipe &eventPipe) noexcept
{
    uint64_t currentBatchId = getCurrentBatchId(eventPipe);
    bool ret = checkHasChanged(currentBatchId);
    batchId = currentBatchId;
    return ret;
}

/**
 * @brief Force the next call to isNewPushRequired() to return true.
 * @param eventPipe event Pipe to verify
 */
void AsyncActor::Event::Batch::forceChange(const Pipe &eventPipe) noexcept
{
    batchId = getCurrentBatchId(eventPipe) - 1;
}

inline std::ostream &operator<<(std::ostream &os, const AsyncActor::ActorId::RouteIdComparable &routeIdComparable)
{
    return os << (unsigned)routeIdComparable.getNodeId() << '-' << routeIdComparable.getNodeConnectionId();
}

inline std::ostream &operator<<(std::ostream &os, const AsyncActor::ActorId &actorId)
{
    if (actorId.isInProcess())
    {
        return os << (unsigned)actorId.nodeId << '.' << actorId.getNodeActorId();
    }
    else
    {
        return os << actorId.getRouteId() << '.' << (unsigned)actorId.nodeId << '.' << actorId.getNodeActorId();
    }
}

inline std::ostream &operator<<(std::ostream &os, const AsyncActor::Event::OStreamName &eventName)
{
    AsyncActor::Event::toOStream(os, eventName);
    return os;
}

inline std::ostream &operator<<(std::ostream &os, const AsyncActor::Event::OStreamContent &eventContent)
{
    AsyncActor::Event::toOStream(os, eventContent);
    return os;
}

inline std::ostream &operator<<(std::ostream &os, const AsyncActor::Event &event)
{
    return os << AsyncActor::Event::OStreamName(event) << '{' << AsyncActor::Event::OStreamContent(event) << '}';
}

inline std::ostream &operator<<(std::ostream &s, const AsyncActor::property_type::Collection &p)
{
    return p.toOStream(s);
}

inline std::ostream &operator<<(std::ostream &s, const AsyncActor::Event::property_type::Collection &p)
{
    return p.toOStream(s);
}
}

#ifdef TREDZONE_CPP11_SUPPORT
#include <functional>
namespace std
{
template <> struct hash<tredzone::AsyncActor::ActorId>
{
    std::size_t operator()(const tredzone::AsyncActor::ActorId &actorId) const noexcept
    {
        return static_cast<size_t>(actorId.getNodeId()) + static_cast<size_t>(actorId.getNodeActorId()) * 100;
    }
};
}
#endif