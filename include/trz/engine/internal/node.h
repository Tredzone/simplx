/**
 * @file node.h
 * @brief Vertx engine graph node
 * @copyright 2013-2019 Tredzone (www.tredzone.com). All rights reserved.
 * Please see accompanying LICENSE file for licensing terms.
 */

#pragma once

#include <csignal>
#include <iostream>
#include <map>
#include <set>
#include <typeinfo>
#include <vector>

#include "trz/engine/engine.h"
#include "trz/engine/internal/intrinsics.h"
#include "trz/engine/internal/parallel.h"
#include "trz/engine/internal/RefMapper.h"
#include "trz/pattern/enterprise.h"

#define CRITICAL_ASSERT(x)                                                                                             \
    if (!(x))                                                                                                          \
    {                                                                                                                  \
        std::cout << "CRITICAL_ASSERT(" << __FILE__ << ':' << __LINE__ << ')' << std::endl;                            \
        exit(-1);                                                                                                      \
    }

namespace tredzone
{
// import into namespace
using std::string;

// forward declarations
class EngineToEngineConnector;
class AsyncNode;

#pragma pack(push)
#pragma pack(1)
struct Actor::EventTable
{
    struct RegisteredEvent
    {
        EventId eventId;
        void *eventHandler;
        bool (*staticEventHandler)(void *, const Event &);
        inline RegisteredEvent() noexcept;
    };
    static const int HIGH_FREQUENCY_CALLBACK_ARRAY_SIZE =
        (3 * CACHE_LINE_SIZE - sizeof(NodeActorId) - 5 * sizeof(void *) - sizeof(size_t)) / sizeof(RegisteredEvent);
    static const int LOW_FREQUENCY_ARRAY_ALIGNEMENT = 5;
    NodeActorId nodeActorId;
    RegisteredEvent hfEvent[HIGH_FREQUENCY_CALLBACK_ARRAY_SIZE];
    Actor *asyncActor;
    RegisteredEvent *lfEvent;
    RegisteredEvent *undeliveredEvent;
    size_t undeliveredEventCount;
    EventTable *nextUnused;
    void *const deallocatePointer;
    const Actor * getActor() const {return asyncActor;}
    EventTable(void *) noexcept;
    ~EventTable() noexcept;
    inline bool onEvent(const Event &event, uint64_t &) const;
    bool onLowFrequencyEvent(const Event &event, uint64_t &) const;
    void onUndeliveredEvent(const Event &event) const;
    static size_t lfRegisteredEventArraySize(RegisteredEvent *) noexcept;
    static bool onUnregisteredEvent(void *, const Event &);
#ifndef NDEBUG
    bool debugCheckUndeliveredEventCount() const noexcept;
#endif
};

// is e2e-related?
struct Actor::NodeConnection : public MultiDoubleChainLink<NodeConnection>
{
    using OnOutboundEventFn = void (*)(EngineToEngineConnector *, const Actor::Event &);
    using OnInboundUndeliveredEventFn = OnOutboundEventFn;
    
    ActorId::RouteId::NodeConnectionId  nodeConnectionId;
    ActorId::RouteId::NodeConnectionId  unreachableNodeConnectionId;
    EngineToEngineConnector             *connector;
    OnOutboundEventFn                   onOutboundEventFn;
    OnInboundUndeliveredEventFn         onInboundUndeliveredEventFn;
    bool                                isEngineToEngineSharedMemoryConnectorFlag;
};
#pragma pack(pop)

class AsyncNodesHandle
{
  private:
    char cacheLineHeaderPadding[CACHE_LINE_SIZE - 1];

  public:
    static const int MAX_SIZE = Actor::MAX_NODE_COUNT;
    const size_t size;
    const size_t eventAllocatorPageSize;
    typedef Actor::NodeId NodeId;
    typedef Engine::CoreSet CoreSet;
    typedef Actor::ActorId::RouteId::NodeConnectionId NodeConnectionId;

    struct NodeHandle;
    struct WriterSharedHandle;
    typedef Actor::Event::Chain EventChain;

#pragma pack(push)
#pragma pack(1)
    struct Shared
    {
        struct UnreachableNodeConnection : MultiForwardChainLink<UnreachableNodeConnection>
        {
            NodeConnectionId nodeConnectionId;
            inline UnreachableNodeConnection(NodeConnectionId pnodeConnectionId) noexcept
                : nodeConnectionId(pnodeConnectionId)
            {
            }
        };
        typedef UnreachableNodeConnection::ForwardChain<> UnreachableNodeConnectionChain;
        struct EventAllocatorPage : MultiForwardChainLink<EventAllocatorPage>
        {
            const uint32_t index;
            inline EventAllocatorPage(uint32_t pindex) noexcept : index(pindex)
            {
                CRITICAL_ASSERT(sizeof(EventAllocatorPage) <= (unsigned)CACHE_LINE_SIZE);
                CRITICAL_ASSERT((uintptr_t) this % CACHE_LINE_SIZE == 0);
            }
            inline void *at(size_t offset) noexcept
            {
                return reinterpret_cast<char *>(this) + CACHE_LINE_SIZE + offset;
            }
        };
        typedef EventAllocatorPage::ForwardChain<> EventAllocatorPageChain;
        struct ReadWriteLocked
        {                                    // TODO make it all fit in one cache line
            bool deliveredEventsFlag;        // the reader has processed toBeDeliveredEventChain
            bool checkUndeliveredEventsFlag; // the reader has undelivered events as a writer
            NodeId writerNodeId;
            EventAllocatorPageChain usedEventAllocatorPageChain;
            EventChain toBeDeliveredEventChain;
            EventChain toBeRoutedEventChain;
            EventChain toBeUndeliveredRoutedEventChain;
            EventChain undeliveredEventChain;
            UnreachableNodeConnectionChain unreachableNodeConnectionChain;
            NodeHandle *readerNodeHandle;
            inline ReadWriteLocked() noexcept : deliveredEventsFlag(false),
                                                checkUndeliveredEventsFlag(false),
                                                writerNodeId(MAX_SIZE),
                                                readerNodeHandle(0)
            {
            }
            EventChain::iterator undeliveredEvent(const EventChain::iterator &,
                                                  AsyncNodesHandle::EventChain &) noexcept;
        };
        struct WriteCache
        {
            bool checkUndeliveredEventsFlag;
            uint8_t batchIdIncrement;
            uint64_t batchId;
            uint64_t totalWrittenByteSize;
            EventChain toBeDeliveredEventChain;
            EventChain toBeRoutedEventChain;
            EventChain toBeUndeliveredRoutedEventChain;
            UnreachableNodeConnectionChain unreachableNodeConnectionChain;
            const size_t eventAllocatorPageSize;
            size_t frontUsedEventAllocatorPageChainOffset;
            EventAllocatorPageChain usedEventAllocatorPageChain;
            EventAllocatorPageChain freeEventAllocatorPageChain;
            CacheLineAlignedBufferContainer eventAllocatorPageAllocator;
            uint32_t nextEventAllocatorPageIndex;
            WriteCache(size_t peventAllocatorPageSize);               // throw (std::bad_alloc)
            inline void newEventPage();                               // throw (std::bad_alloc)
            void *allocateEvent(size_t);                       // throw (std::bad_alloc)
            void *allocateEvent(size_t, uint32_t &, size_t &); // throw (std::bad_alloc)
        };

        WriteCache writeCache;
        char cacheLinePadding1[TREDZONE_CACHE_LINE_PADDING(sizeof(sig_atomic_t) + sizeof(bool) + sizeof(WriteCache))];
        ReadWriteLocked readWriteLocked;
        char cacheLinePadding2[TREDZONE_CACHE_LINE_PADDING(sizeof(ReadWriteLocked))];

        Shared(size_t eventAllocatorPageSize); // throw (std::bad_alloc)
    };
    struct ReaderSharedHandle
    {
        struct CacheLine1
        {
            bool *isReaderActive;
            sig_atomic_t *readerCAS;
            Shared::ReadWriteLocked *sharedReadWriteLocked;
            inline CacheLine1() noexcept : isReaderActive(0), readerCAS(0), sharedReadWriteLocked(0) {}
            
        } cl1;

        char cacheLinePadding[TREDZONE_CACHE_LINE_PADDING(sizeof(CacheLine1))];

        struct CacheLine2
        {
            bool isWriterActive;
            bool isWriteLocked;
            inline CacheLine2() noexcept : isWriterActive(false), isWriteLocked(false) {}
            
        } cl2;

        ReaderSharedHandle() noexcept;
        void init(WriterSharedHandle &) noexcept;
        inline bool getIsWriterActive() noexcept { return cl2.isWriterActive; }
        inline void setIsWriterActive(bool isWriterActive) noexcept { cl2.isWriterActive = isWriterActive; }
        inline bool getIsReaderActive() noexcept
        {
            assert(cl1.isReaderActive != 0);
            return *cl1.isReaderActive;
        }
        inline void setIsReaderActive(bool isReaderActive) noexcept
        {
            assert(cl1.isReaderActive != 0);
            *cl1.isReaderActive = isReaderActive;
        }
        inline sig_atomic_t &getReferenceToReaderCAS() noexcept
        {
            assert(cl1.readerCAS != 0);
            return *cl1.readerCAS;
        }
        inline bool getIsWriteLocked() noexcept { return cl2.isWriteLocked; }
        inline void setIsWriteLocked(bool isWriteLocked) noexcept { cl2.isWriteLocked = isWriteLocked; }
        void    read(void) noexcept;
        bool returnToSender(const Actor::Event &) noexcept;
        static void dispatchUnreachableNodes(Actor::OnUnreachableChain &, Shared::UnreachableNodeConnectionChain &,
                                             NodeId, AsyncExceptionHandler &) noexcept;
    };
    struct WriterSharedHandle
    {
        struct CacheLine1
        {
            bool *isWriterActive;
            bool *isWriteLocked;
            NodeHandle *writerNodeHandle;
            inline CacheLine1() noexcept : isWriterActive(0), isWriteLocked(0), writerNodeHandle(0) {}
        } cl1;

        char cacheLinePadding[TREDZONE_CACHE_LINE_PADDING(sizeof(CacheLine1))];

        struct CacheLine2
        {
            sig_atomic_t readerCAS;
            bool isReaderActive;
            Shared shared;
            /**
             * throw (std::bad_alloc)
             */
            inline CacheLine2(size_t eventAllocatorPageSize)
                : readerCAS(0), isReaderActive(false), shared(eventAllocatorPageSize)
            {
            }
        } cl2;

        WriterSharedHandle(size_t eventAllocatorPageSize); // throw (std::bad_alloc)
        void init(NodeId writerNodeId, NodeHandle &readerNodeHandle, NodeHandle &writerNodeHandle,
                  ReaderSharedHandle &) noexcept;
        inline bool getIsWriterActive() noexcept
        {
            assert(cl1.isWriterActive != 0);
            return *cl1.isWriterActive;
        }
        inline void setIsWriterActive(bool isWriterActive) noexcept
        {
            assert(cl1.isWriterActive != 0);
            *cl1.isWriterActive = isWriterActive;
        }
        inline bool getIsReaderActive() noexcept { return cl2.isReaderActive; }
        inline void setIsReaderActive(bool isReaderActive) noexcept { cl2.isReaderActive = isReaderActive; }
        inline sig_atomic_t &getReferenceToReaderCAS() noexcept { return cl2.readerCAS; }
        inline bool getIsWriteLocked() noexcept
        {
            assert(cl1.isWriteLocked != 0);
            return *cl1.isWriteLocked;
        }
        inline void setIsWriteLocked(bool isWriteLocked) noexcept
        {
            assert(cl1.isWriteLocked != 0);
            *cl1.isWriteLocked = isWriteLocked;
        }
        inline Shared &getReferenceToShared() noexcept { return cl2.shared; }
        bool write() noexcept;
        void writeFailed() noexcept;
        void writeDispatchAndClearUndeliveredEvents(EventChain &) noexcept;
        bool localOnEvent(const Actor::Event &, uint64_t &) noexcept;
        bool localOnOutboundEvent(const Actor::Event &) noexcept;
        void onUndeliveredEvent(const Actor::Event &) noexcept;
        void onUndeliveredEventToSourceActor(const Actor::Event &) noexcept;
    };
#pragma pack(pop)
    struct NodeHandle
    {
        CacheLineAlignedArray<ReaderSharedHandle> readerSharedHandles;
        CacheLineAlignedArray<WriterSharedHandle> writerSharedHandles;
        AsyncNode *node;
        Actor::NodeActorId nextHanlerId;
        bool stopFlag;
        bool shutdownFlag;
        bool interruptFlag;
        const CoreSet coreSet;
#ifndef NDEBUG
        bool debugNodePtrWasSet;
        bool debugSynchronizeWriteFailedOperatorCalled;
#endif

        NodeHandle(const std::pair<AsyncNodesHandle *, const CoreSet *> &); // throw (std::bad_alloc)
        ~NodeHandle() noexcept;
        inline ReaderSharedHandle &getReaderSharedHandle(NodeId writerNodeId) noexcept
        {
            assert(writerNodeId < readerSharedHandles.size());
            return readerSharedHandles[writerNodeId];
        }
        inline WriterSharedHandle &getWriterSharedHandle(NodeId readerNodeId) noexcept
        {
            assert(readerNodeId < writerSharedHandles.size());
            return writerSharedHandles[readerNodeId];
        }
        template <class _Operator> inline void foreachRead(NodeId readerNodeId, _Operator &op) noexcept
        {
            assert(readerNodeId < readerSharedHandles.size());
            for (size_t i = 0; i < readerNodeId; ++i)
            {
                op(readerSharedHandles[i], (NodeId)i);
            }
            // skip readerNodeId (uses 2 loops to avoid if statement)
            for (size_t i = readerNodeId + 1, sz = readerSharedHandles.size(); i < sz; ++i)
            {
                op(readerSharedHandles[i], (NodeId)i);
            }
        }
        template <class _Operator> inline void foreachWrite(NodeId writerNodeId, _Operator &op) noexcept
        {
            assert(writerNodeId < writerSharedHandles.size());
            for (size_t i = 0; i < writerNodeId; ++i)
            {
                op(writerSharedHandles[i], (NodeId)i);
            }
            for (size_t i = writerNodeId + 1, sz = writerSharedHandles.size(); i < sz; ++i)
            {
                op(writerSharedHandles[i], (NodeId)i);
            }
        }
    };

    AsyncNodesHandle(const std::pair<size_t, const CoreSet *> &); // throw (std::bad_alloc)
    inline NodeHandle &getNodeHandle(NodeId nodeId) noexcept
    {
        assert(nodeId < size);
        return *nodeHandles[nodeId];
    }
    inline bool isNodeActive(NodeId nodeId) const noexcept
    {
        assert(nodeId < size);
        return activeNodeHandles[nodeId];
    }
    inline bool activateNode(NodeId nodeId) noexcept
    {
        assert(nodeId < size);
        return atomicCompareAndSwap(&activeNodeHandles[nodeId], false, true);
    }
    inline void deactivateNode(NodeId nodeId) noexcept
    {
        assert(nodeId < size);
        assert(nodeHandles[nodeId]->debugNodePtrWasSet == false || nodeHandles[nodeId]->node != 0);
        nodeHandles[nodeId]->node = 0;
        memoryBarrier();
        atomicCompareAndSwap(&activeNodeHandles[nodeId], true, false);
    }

  private:
    class CacheLineAlignedNodeHandleAutoPointer
    {
      public:
        inline CacheLineAlignedNodeHandleAutoPointer() noexcept : ptr(0) {}
        inline ~CacheLineAlignedNodeHandleAutoPointer() noexcept { delete ptr; }
        /**
         * throw (std::bad_alloc)
         */
        inline void init(AsyncNodesHandle &nodesHandler, const CoreSet &coreSet)
        {
            assert(ptr == 0);
            delete ptr;
            ptr = new CacheLineAlignedObject<NodeHandle>(std::make_pair(&nodesHandler, &coreSet));
        }
        inline NodeHandle &operator*() noexcept
        {
            assert(ptr != 0);
            return **ptr;
        }
        inline const NodeHandle &operator*() const noexcept
        {
            assert(ptr != 0);
            return **ptr;
        }
        inline NodeHandle *operator->() noexcept
        {
            assert(ptr != 0);
            return ptr->CacheLineAlignedObject<NodeHandle>::operator->();
        }
        inline const NodeHandle *operator->() const noexcept
        {
            assert(ptr != 0);
            return ptr->CacheLineAlignedObject<NodeHandle>::operator->();
        }

      private:
        CacheLineAlignedObject<NodeHandle> *ptr;
        CacheLineAlignedNodeHandleAutoPointer(const CacheLineAlignedNodeHandleAutoPointer &) noexcept;
        CacheLineAlignedNodeHandleAutoPointer &operator=(const CacheLineAlignedNodeHandleAutoPointer &);
    };

    CacheLineAlignedArray<bool> activeNodeHandles;
    CacheLineAlignedArray<CacheLineAlignedNodeHandleAutoPointer> nodeHandles;
    char cacheLineTrailerPadding[CACHE_LINE_SIZE - 1];
};

class AsyncNodeAllocator
{
  public:
    typedef Actor::NodeId NodeId;

    AsyncNodeAllocator()
        : // throw (std::bad_alloc)
          blockChainArray((assert(sizeof(void *) <= 8),
                           static_cast<BlockChain *>(alignMalloc(tredzone::CACHE_LINE_SIZE, sizeof(BlockChain) * 64))))
#ifndef NDEBUG
          ,
          debugThreadId(ThreadId::current())
#endif
    {
        if (blockChainArray == 0)
        {
            throw std::bad_alloc();
        }
        assert((uintptr_t)blockChainArray % tredzone::CACHE_LINE_SIZE == 0);
        new (blockChainArray) BlockChain[64];
    }
    ~AsyncNodeAllocator() noexcept
    {
#ifndef NDEBUG

        if (!debugCheckMap.empty())
        {
            std::cout << std::endl << "~AsyncNodeAllocator(): debugCheckMap" << std::endl;

            for (DebugCheckMap::iterator i = debugCheckMap.begin(), endi = debugCheckMap.end(); i != endi; ++i)
            {
                std::cout << "unreleased memory block(" << i->first << ") size: " << i->second.sz << std::endl;

                if (i->second.bactraceVectPtr.get() != 0)
                {
                    if (i->second.bactraceVectPtr->size() != 0)
                    {
                        std::cout << "allocation backtrace : " << std::endl;
                        for (std::vector<std::string>::const_iterator i2 = i->second.bactraceVectPtr->begin(),
                                                                      endi2 = i->second.bactraceVectPtr->end();
                             i2 != endi2; ++i2)
                        {
                            std::cout << "\t" << *i2 << std::endl;
                        }
                    }
                    else
                    {
                        std::cout << "no backtrace" << std::endl;
                    }
                }
                else
                {
                    std::cout << "backtrace disabled (use tredzone::Engine::debugActivateMemoryLeakBacktrace())"
                              << std::endl;
                }
            }
        }

        assert(debugCheckMap.empty());
#endif

        while (!pageChain.empty())
        {
            tredzone::alignFree(tredzone::CACHE_LINE_SIZE, pageChain.pop_front());
        }
        alignFree(tredzone::CACHE_LINE_SIZE, blockChainArray);
    }
#ifndef NDEBUG
    inline const ThreadId &debugGetThreadId() const noexcept { return debugThreadId; }
#endif

    /**
     * throw (std::bad_alloc)
     */
    inline void *allocate(size_t sz, const void * = 0)
    {
        assert(debugThreadId == ThreadId::current());
        unsigned isz = index(sz);
        BlockChain &blockChain = blockChainArray[isz];
        if (blockChain.empty())
        {
            size_t blockSz = (size_t)1 << isz;
            size_t pageSz = systemPageSize();
            assert(pageSz % tredzone::CACHE_LINE_SIZE == 0);
            pageSz *= ((blockSz + sizeof(Block) + pageSz - 1) / pageSz);
            Block *page = static_cast<Block *>(alignMalloc(tredzone::CACHE_LINE_SIZE, pageSz));
            if (page == 0)
            {
                throw std::bad_alloc();
            }
            assert((uintptr_t)page % tredzone::CACHE_LINE_SIZE == 0);
#ifndef NDEBUG
            pageChain.push_back(new (page) Block);
#else
            pageChain.push_back(page);
#endif
            pageSz -= sizeof(Block);
            BlockChain *currentBlockChain = &blockChain;
            for (char *p = reinterpret_cast<char *>(page + 1); pageSz >= sizeof(Block);
#ifndef NDEBUG
                 currentBlockChain->push_front(new (p) Block)
#else
                 currentBlockChain->push_front(reinterpret_cast<Block *>(p))
#endif
                     ,
                      p += blockSz, pageSz -= blockSz)
            {
                if (pageSz < blockSz)
                {
                    unsigned ipageSz = highestBit(pageSz);
                    assert(ipageSz < 64);
                    blockSz = (size_t)1 << ipageSz;
                    currentBlockChain = &blockChainArray[ipageSz];
                }
            }
        }
        void *ret = blockChain.pop_front();
#ifndef NDEBUG
        try
        {
            debugCheckMap.insert(std::make_pair(ret, sz));
        }
        catch (...)
        {
            blockChain.push_front(new (ret) Block);
            throw;
        }
#endif
        return ret;
    }
    inline void deallocate(size_t sz, void *p) noexcept
    {
#ifndef NDEBUG
        assert(debugThreadId == ThreadId::current());
        DebugCheckMap::iterator i = debugCheckMap.find(p);
        assert(i != debugCheckMap.end());
        assert(i->second.sz == sz);
        debugCheckMap.erase(i);
        blockChainArray[index(sz)].push_front(new (p) Block);
#else
        blockChainArray[index(sz)].push_front(static_cast<Block *>(p));
#endif
    }
    inline static unsigned index(size_t psz) noexcept
    {
        size_t sz = std::max(psz, sizeof(void *));
        unsigned hbit = tredzone::highestBit(sz);
        unsigned dbit = hbit - tredzone::lowestBit(sz);
        unsigned ret = hbit + (dbit) / std::max<unsigned>(1, dbit);
        assert(ret < 64);
        return ret;
    }

  private:
    struct Block : MultiForwardChainLink<Block>
    {
    };
    typedef Block::ForwardChain<> BlockChain;

    BlockChain pageChain;
    BlockChain *blockChainArray;

#ifndef NDEBUG
    // debug mode
    friend class AsyncNode;
    friend class Engine;

    static volatile bool debugActivateMemoryLeakBacktraceFlag;

    struct DebugCheckMapValue
    {
        const size_t sz;
        mutable std::unique_ptr<std::vector<std::string>> bactraceVectPtr;
        inline DebugCheckMapValue(size_t psz)
            : sz(psz),
              bactraceVectPtr(debugActivateMemoryLeakBacktraceFlag ? new std::vector<std::string>(debugBacktrace()) : 0)
        {
        }
        inline DebugCheckMapValue(const DebugCheckMapValue &other)
            : sz(other.sz), bactraceVectPtr(std::move(other.bactraceVectPtr))
        {
        }
    };

    ThreadId debugThreadId;
    typedef std::map<void *, DebugCheckMapValue> DebugCheckMap;
    DebugCheckMap debugCheckMap;
#endif
};

class AsyncNodeManager : private std::unique_ptr<AsyncExceptionHandler>, public Parallel<AsyncNodesHandle>
{
  public:
    typedef Actor::CoreId CoreId;
    typedef Engine::CoreSet CoreSet;

    AsyncNodeManager(size_t eventAllocatorPageSize,
                     const CoreSet & = Engine::FullCoreSet()); // throw (std::bad_alloc)
    AsyncNodeManager(AsyncExceptionHandler &pexceptionHandler, size_t eventAllocatorPageSize,
                     const CoreSet & = Engine::FullCoreSet()); // throw (std::bad_alloc)
    inline const CoreSet &getCoreSet() const noexcept { return coreSet; }
    inline size_t getEventAllocatorPageSize() const noexcept { return nodesHandle.eventAllocatorPageSize; }

  private:
    friend class Engine;
    friend class EngineEventLoop;
    friend class AsyncNode;
    friend class AsyncNodesHandle;
    AsyncExceptionHandler &exceptionHandler;
    const CoreSet coreSet;

    void shutdown() noexcept;
};

struct AsyncNodeBase
{
public:

    using AsyncActorChain = Actor::Chain;
    using AsyncActorOnUnreachableChain = Actor::OnUnreachableChain;
    using AsyncActorCallbackChain = Actor::Callback::Chain;
    using NodeConnectionChain = Actor::NodeConnection::DoubleChain<>;
    
    typedef void (*EventToOStreamFunction)(std::ostream &, const Actor::Event &);       // remove this crap [PL]
    typedef bool (*EventIsE2ECapableFunction)(const char *&, Actor::Event::EventE2ESerializeFunction &, Actor::Event::EventE2EDeserializeFunction &);
    
    struct StaticShared
    {
    public:
        
        // remove this crap [PL]
        struct EventToStreamFunctions
        {
            EventToOStreamFunction eventNameToOStreamFunction;
            EventToOStreamFunction eventContentToOStreamFunction;
            inline EventToStreamFunctions() noexcept
                : eventNameToOStreamFunction(0), eventContentToOStreamFunction(0)
            {
            }
        };
        
        // absolute event ID dictionary (used in e2e)
        class AbsoluteEventIds
        {
        public:
            ~AbsoluteEventIds() noexcept;
            void addEventId(Actor::EventId, const char *);
            void removeEventId(Actor::EventId) noexcept;
            std::pair<bool, Actor::EventId> findEventId(const char *) const noexcept;
            AbsoluteEventVector    getAbsoluteEventDictionary(void) const;

        private:
            
            struct CStringLess
            {
                inline bool operator()(const char *x, const char *y) const noexcept { return std::strcmp(x, y) < 0; }
            };
            
            using EventIdNameMap = std::map<Actor::EventId, std::string>;
            using EventNameIdMap = std::map<const char *, Actor::EventId, CStringLess>;
            
            EventIdNameMap eventIdNameMap;
            EventNameIdMap eventNameIdMap;

            void debugCheckMaps() const;
        };

        static const int SINGLETON_ACTOR_INDEX_SIZE = 4096;

        char                                    cacheLineHeaderPadding[CACHE_LINE_SIZE - 1];
        Mutex                                   mutex;
        std::bitset<SINGLETON_ACTOR_INDEX_SIZE> singletonActorIndexBitSet;
        std::bitset<Actor::MAX_EVENT_ID_COUNT>  eventIdBitSet;                                      // for e2e or for ALL events?
        EventToStreamFunctions                  eventToStreamFunctions[Actor::MAX_EVENT_ID_COUNT];
        EventIsE2ECapableFunction               eventIsE2ECapableFunction[Actor::MAX_EVENT_ID_COUNT];
        AbsoluteEventIds                        m_AbsoluteEventIds;
        char                                    cacheLineTrailerPadding[CACHE_LINE_SIZE - 1];
    };
    
#pragma pack(push)
#pragma pack(1)
    struct SingletonActorIndexEntry : MultiDoubleChainLink<SingletonActorIndexEntry>
    {
        typedef DoubleChain<> Chain;
        bool reservedFlag;
        Actor *asyncActor;
        inline SingletonActorIndexEntry() noexcept : reservedFlag(false), asyncActor(0) {}
        inline ~SingletonActorIndexEntry() noexcept
        {
            assert(!reservedFlag);
            assert(asyncActor == 0);
        }
    };
#pragma pack(pop)
    class NodeActorCountListener : private MultiDoubleChainLink<NodeActorCountListener>
    {
      public:
        inline NodeActorCountListener() noexcept : chain(0) {}
        virtual ~NodeActorCountListener() noexcept { unsubscribe(); }
        inline void unsubscribe() noexcept
        {
            if (chain != 0)
            {
                chain->remove(this);
                chain = 0;
            }
        }

    protected:
      
        virtual void onNodeActorCountDiff(const size_t oldCount, const size_t newCount) noexcept = 0;

    private:
    
        friend class AsyncNode;
        friend struct AsyncNodeBase;
        struct Chain : DoubleChain<0u, Chain>
        {
            inline static NodeActorCountListener *
            getItem(tredzone::MultiDoubleChainLink<NodeActorCountListener> *link) noexcept
            {
                return static_cast<NodeActorCountListener *>(link);
            }
            inline static const NodeActorCountListener *
            getItem(const tredzone::MultiDoubleChainLink<NodeActorCountListener> *link) noexcept
            {
                return static_cast<const NodeActorCountListener *>(link);
            }
            inline static tredzone::MultiDoubleChainLink<NodeActorCountListener> *
            getLink(NodeActorCountListener *item) noexcept
            {
                return static_cast<tredzone::MultiDoubleChainLink<NodeActorCountListener> *>(item);
            }
            inline static const tredzone::MultiDoubleChainLink<NodeActorCountListener> *
            getLink(const NodeActorCountListener *item) noexcept
            {
                return static_cast<const tredzone::MultiDoubleChainLink<NodeActorCountListener> *>(item);
            }
        };

        Chain *chain;
    };

    static StaticShared                         s_StaticShared;
    
    AsyncNodeAllocator                                                                  nodeAllocator;
    std::vector<SingletonActorIndexEntry, Actor::Allocator<SingletonActorIndexEntry>>   singletonActorIndex;
    SingletonActorIndexEntry::Chain                                                     singletonActorIndexChain;
    
    size_t                                      m_ActorCount;
    AsyncActorChain                             asyncActorChain;
    AsyncActorChain                             destroyedActorChain;
    
    AsyncActorOnUnreachableChain                actorOnUnreachableChain;
    Actor::EventTable                           *freeEventTable;
    AsyncNodeManager                            &nodeManager;
    AsyncActorCallbackChain                     asyncActorCallbackChain;
    AsyncActorCallbackChain                     asyncActorPerformanceNeutralCallbackChain;
    NodeConnectionChain                         freeNodeConnectionChain;
    NodeConnectionChain                         inUseNodeConnectionChain;
    Actor::ActorId::RouteId::NodeConnectionId   lastNodeConnectionId;
    NodeActorCountListener::Chain               m_NodeActorCountListenerChain;
    const size_t                                eventAllocatorPageSize;
    uint8_t                                     loopUsagePerformanceCounterIncrement;

    AsyncNodeBase(AsyncNodeManager &asyncNodeManager); // throw (std::bad_alloc)
    ~AsyncNodeBase() noexcept;
};

class AsyncNode : private AsyncNodeBase, public AsyncNodeManager::Node
{
  public:
    typedef Actor::CoreId CoreId;
    typedef Engine::CoreSet CoreSet;
    typedef CoreSet::UndefinedCoreException UndefinedCoreException;
    typedef Engine::CoreInUseException CoreInUseException;
    typedef EngineCustomEventLoopFactory EventLoopFactory;
    
    ENTERPRISE_0X5030
    
    class Thread
    {
      public:
        typedef void (*StartHook)(void *);
        typedef void (*StopHook)();
        class Exception : public std::exception
        {
          public:
            Exception(const Exception &) noexcept;
            Exception(const std::string &) noexcept;
            virtual ~Exception() noexcept;
            virtual const char *what() const noexcept;

          private:
            std::unique_ptr<std::string> message;
        };
        /**
         * throw (Exception)
         */
        Thread(CoreId, bool, const ThreadRealTimeParam &, size_t stackSizeBytes, StartHook pstartHook,
               void *pstartHookArg, StopHook pstopHook);
        ~Thread() noexcept;
        void run(CacheLineAlignedObject<AsyncNode> &) noexcept;
        inline std::pair<StartHook, void *> getStartHook() const noexcept
        {
            return std::make_pair(startHook, startHookArg);
        }
        inline StopHook getStopHook() const noexcept { return stopHook; }
        
        inline CoreId   getCoreId(void) const noexcept { return coreId; }
        
      private:
        const CoreId coreId;
        const bool redZoneFlag;
        const ThreadRealTimeParam redZoneParam;
        bool inThreadFlag;
        bool runFlag;
        bool inThreadExceptionFlag;
        const StartHook startHook;
        void *const startHookArg;
        const StopHook stopHook;
        std::string inThreadExceptionWhat;
        CacheLineAlignedObject<AsyncNode> **inThreadNode;

        static void inThread(void *);
    };
    struct Init
    {
        AsyncNodeManager &nodeManager;
        CoreId coreId;
        EngineCustomEventLoopFactory &customEventLoopFactory;
        inline Init(AsyncNodeManager &pnodeManager, CoreId pcoreId,
                    EngineCustomEventLoopFactory &pcustomEventLoopFactory) noexcept
            : nodeManager(pnodeManager),
              coreId(pcoreId),
              customEventLoopFactory(pcustomEventLoopFactory)
        {
        }
    };

    std::string dbgRefLogStr;
	
    AsyncNode(const Init &); // throw (std::bad_alloc, UndefinedCoreException, CoreInUseException)
    ~AsyncNode() noexcept;
    inline const CoreSet &getCoreSet() const noexcept { return nodeHandle.coreSet; }
    inline size_t getEventAllocatorPageSize() const noexcept { return eventAllocatorPageSize; }
    inline size_t getActorCount() const noexcept { return m_ActorCount; }
    inline void subscribeNodeActorCountListener(NodeActorCountListener &listener) noexcept
    {
        listener.unsubscribe();
        (listener.chain = &m_NodeActorCountListenerChain)->push_back(&listener);
    }
    template<class _Actor>
    inline _Actor& newActor()
    {
        ENTERPRISE_0X500A(this, static_cast<Actor*>(nullptr));
        // throw (std::bad_alloc, Actor::ShutdownException, ...)
		_Actor& actor = Actor::newActor<_Actor>(*this);
        ENTERPRISE_0X500B(this, static_cast<Actor*>(nullptr), &actor);

        TraceREF(this, __func__, "-1.-1", cppDemangledTypeInfoName(typeid(*this)), actor.actorId, cppDemangledTypeInfoName(typeid(actor)))
        
		return actor;
	}
    
    template<class _Actor, class _ActorInit>
    inline _Actor& newActor(const _ActorInit& init)
    {
        ENTERPRISE_0X500C(this, static_cast<Actor*>(nullptr));
        // throw (std::bad_alloc, Actor::ShutdownException, ...)
        _Actor& actor = Actor::newActor<_Actor, _ActorInit>(*this, init);
        ENTERPRISE_0X500D(this, static_cast<Actor*>(nullptr), static_cast<Actor*>(&actor));
        
        TraceREF(this, __func__, "-1.-1", cppDemangledTypeInfoName(typeid(*this)), actor.actorId, cppDemangledTypeInfoName(typeid(actor)))
        
		return actor;
	}
    
    Actor::EventTable &retainEventTable(Actor &); // throw (std::bad_alloc, Actor::ShutdownException)
    void releaseEventTable(Actor::EventTable &) noexcept;
    void destroyAsyncActors() noexcept;
    inline void synchronize() noexcept
    {
        synchronizePreBarrier();
        synchronizePostBarrier();
    }
    inline void synchronizePreBarrier() noexcept
    {
#ifndef NDEBUG
        assert(!debugSynchronizePostBarrierFlag);
        debugSynchronizePostBarrierFlag = true;
#endif
        synchronizeUsageCount();
        synchronizeAsyncActorCallbacks();
        synchronizeLocalEvents();
        AsyncNodeManager::Node::synchronizePreBarrier();
    }
    inline void synchronizePostBarrier() noexcept
    {
#ifndef NDEBUG
        assert(debugSynchronizePostBarrierFlag);
        debugSynchronizePostBarrierFlag = false;
#endif
        AsyncNodeManager::Node::synchronizePostBarrier();
        synchronizeDestroyAsyncActors();
    }
    inline void stop() noexcept { nodeHandle.stopFlag = nodeHandle.interruptFlag = true; }
#ifndef NDEBUG
    inline const ThreadId &debugGetThreadId() const noexcept { return nodeAllocator.debugThreadId; }
#endif

  private:
    friend class Engine;
    friend class Actor;
    friend class EngineToEngineConnector;
    friend class AsyncNodesHandle;
    friend class EngineEventLoop;

    EngineCustomEventLoopFactory::EventLoopAutoPointer eventLoop;
    AsyncNodesHandle::Shared::EventAllocatorPageChain usedlocalEventAllocatorPageChain;
    Actor::CorePerformanceCounters corePerformanceCounters;
#ifndef NDEBUG
    bool debugSynchronizePostBarrierFlag;
#endif

    inline void synchronizeUsageCount() noexcept
    {
        ++corePerformanceCounters.loopTotalCount;
        corePerformanceCounters.loopUsageCount += loopUsagePerformanceCounterIncrement;
        loopUsagePerformanceCounterIncrement = 0;
        ENTERPRISE_0X500E(this);
    }
    inline static uint8_t synchronizeAsyncActorCallbacks(AsyncActorCallbackChain &callbackChain,
                                                         uint64_t &performanceCounter) noexcept
    {
        if (!callbackChain.empty())
        {
            AsyncActorCallbackChain tmp;
            tmp.swap(callbackChain);
            for (AsyncActorCallbackChain::iterator i = tmp.begin(), endi = tmp.end(); i != endi; ++i)
            {
                assert(i->chain == &callbackChain);
                i->chain = &tmp;
            }
            while (!tmp.empty())
            {
                Actor::Callback &callback = *tmp.pop_front();
                callback.chain = 0;
                if (callback.nodeActorId == callback.actorEventTable->nodeActorId)
                {
                    ++performanceCounter;
                    (*callback.onCallback)(callback);
                    ENTERPRISE_0X500F((Actor*)(callback.actorEventTable->getActor()), &callback);
                }
            }
            return 1;
        }
        return 0;
    }
    inline void synchronizeAsyncActorCallbacks() noexcept
    {
        uint64_t c = 0;
        synchronizeAsyncActorCallbacks(asyncActorPerformanceNeutralCallbackChain, c);
        loopUsagePerformanceCounterIncrement =
            synchronizeAsyncActorCallbacks(asyncActorCallbackChain, corePerformanceCounters.onCallbackCount);
    }
    
    inline void synchronizeLocalEvents() noexcept
    {
        AsyncNodesHandle::WriterSharedHandle &writerSharedHandle = nodeHandle.getWriterSharedHandle(id);
        assert(writerSharedHandle.cl2.shared.readWriteLocked.checkUndeliveredEventsFlag == false);
        assert(writerSharedHandle.cl2.shared.readWriteLocked.deliveredEventsFlag == false);
        assert(writerSharedHandle.cl2.shared.readWriteLocked.readerNodeHandle == &nodeHandle);
        assert(writerSharedHandle.cl2.shared.readWriteLocked.toBeDeliveredEventChain.empty());
        assert(writerSharedHandle.cl2.shared.readWriteLocked.undeliveredEventChain.empty());
        assert(writerSharedHandle.cl2.shared.readWriteLocked.usedEventAllocatorPageChain.empty());
        assert(writerSharedHandle.cl2.shared.readWriteLocked.writerNodeId == id);
        assert(writerSharedHandle.cl2.shared.writeCache.checkUndeliveredEventsFlag == false);
        
        if (!writerSharedHandle.cl2.shared.writeCache.toBeDeliveredEventChain.empty() ||
            !writerSharedHandle.cl2.shared.writeCache.toBeRoutedEventChain.empty() ||
            !writerSharedHandle.cl2.shared.writeCache.toBeUndeliveredRoutedEventChain.empty() ||
            !writerSharedHandle.cl2.shared.writeCache.unreachableNodeConnectionChain.empty())
        {
            loopUsagePerformanceCounterIncrement = 1;
            if (writerSharedHandle.cl2.shared.writeCache.batchId != std::numeric_limits<uint64_t>::max())
            {
                writerSharedHandle.cl2.shared.writeCache.batchId +=
                    writerSharedHandle.cl2.shared.writeCache.batchIdIncrement;
                writerSharedHandle.cl2.shared.writeCache.batchIdIncrement = 0;
            }
            
            writerSharedHandle.cl2.shared.writeCache.freeEventAllocatorPageChain.push_back(usedlocalEventAllocatorPageChain);
            
            // 3 events chains: toBeDelivered/toBeRouted/toBeUndeliveredRouted
            AsyncNodesHandle::EventChain toBeDeliveredEventChain;
            writerSharedHandle.cl2.shared.writeCache.toBeDeliveredEventChain.swap(toBeDeliveredEventChain);
            AsyncNodesHandle::EventChain toBeRoutedEventChain;
            writerSharedHandle.cl2.shared.writeCache.toBeRoutedEventChain.swap(toBeRoutedEventChain);
            AsyncNodesHandle::EventChain toBeUndeliveredRoutedEventChain;
            writerSharedHandle.cl2.shared.writeCache.toBeUndeliveredRoutedEventChain.swap(toBeUndeliveredRoutedEventChain);
            assert(usedlocalEventAllocatorPageChain.empty());
            writerSharedHandle.cl2.shared.writeCache.usedEventAllocatorPageChain.swap(usedlocalEventAllocatorPageChain);
            writerSharedHandle.cl2.shared.writeCache.frontUsedEventAllocatorPageChainOffset = 0;
            
            for (AsyncNodesHandle::EventChain::iterator i = toBeDeliveredEventChain.begin(),
                                                        endi = toBeDeliveredEventChain.end();
                 i != endi; ++i)
            {
                if (!writerSharedHandle.localOnEvent(*i, corePerformanceCounters.onEventCount))
                {
                    writerSharedHandle.onUndeliveredEvent(*i);
                }
            }
            
            // is e2e? [PL]
            for (AsyncNodesHandle::EventChain::iterator i = toBeRoutedEventChain.begin(),
                                                        endi = toBeRoutedEventChain.end();
                 i != endi; ++i)
            {
                if (!writerSharedHandle.localOnOutboundEvent(*i))
                {
                    writerSharedHandle.onUndeliveredEvent(*i);
                }
            }
            
            // is e2e? [PL]
            for (AsyncNodesHandle::EventChain::iterator i = toBeUndeliveredRoutedEventChain.begin(),
                                                        endi = toBeUndeliveredRoutedEventChain.end();
                 i != endi; ++i)
            {
                writerSharedHandle.onUndeliveredEventToSourceActor(*i);
            }
            
            AsyncNodesHandle::Shared::UnreachableNodeConnectionChain unreachableNodeConnectionChain;
            unreachableNodeConnectionChain.swap(
                writerSharedHandle.cl2.shared.writeCache.unreachableNodeConnectionChain);
            AsyncNode *node;
            Actor::OnUnreachableChain *actorOnUnreachableChain;
            if (!unreachableNodeConnectionChain.empty() &&
                !(actorOnUnreachableChain =
                      &(node = writerSharedHandle.cl1.writerNodeHandle->node)->actorOnUnreachableChain)
                     ->empty())
            {
                AsyncNodesHandle::ReaderSharedHandle::dispatchUnreachableNodes(*actorOnUnreachableChain,
                                                                               unreachableNodeConnectionChain, node->id,
                                                                               node->nodeManager.exceptionHandler);
            }
        }
    }
    
    inline void synchronizeDestroyAsyncActors() noexcept
    {
        if (!destroyedActorChain.empty())
        {
            AsyncActorChain tmp;
            tmp.swap(destroyedActorChain);
            while (!tmp.empty())
            {
                loopUsagePerformanceCounterIncrement = 1;
                Actor *asyncActor = tmp.front();
                assert(asyncActor->chain == &destroyedActorChain);
                asyncActor->chain = &tmp;
                if (asyncActor->m_ReferenceFromCount == 0 &&
                    (asyncActor->onUnreferencedDestroyFlag = false, asyncActor->m_DestroyRequestedFlag = false,
                     asyncActor->onDestroyRequest(), asyncActor->m_DestroyRequestedFlag) &&
                    asyncActor->m_ReferenceFromCount == 0)
                { // Second asyncActor->m_ReferenceFromCount == 0 occurs after asyncActor->onDestroyRequest(), in case actor is
                  // referenced back during onDestroyRequest()
                    asyncActor->chain->remove(asyncActor);
                    asyncActor->chain =
                        &destroyedActorChain; // to avoid requestDestroy() effect if called during destructor
                    if (asyncActor->singletonActorIndex != StaticShared::SINGLETON_ACTOR_INDEX_SIZE)
                    {
                        assert(asyncActor->singletonActorIndex < StaticShared::SINGLETON_ACTOR_INDEX_SIZE);
                        SingletonActorIndexEntry &singletonActorIndexEntry =
                            singletonActorIndex[asyncActor->singletonActorIndex];
                        assert(singletonActorIndexEntry.asyncActor == asyncActor);
                        assert(singletonActorIndexEntry.reservedFlag == false);
                        singletonActorIndexEntry.asyncActor = 0;
                        singletonActorIndexChain.remove(&singletonActorIndexEntry);
                    }
                    delete asyncActor;
                }
                else
                {
                    TraceREF(this, "synchronizeDestroyAsyncActors::BLOCKED_ACTOR_DESTRUCTION", "-1.-1", cppDemangledTypeInfoName(typeid(*this)), asyncActor->getActorId(), cppDemangledTypeInfoName(typeid(*asyncActor)))
                    
                    if (asyncActor->chain == &tmp)
                    {
                        assert(asyncActor == tmp.front());
                        tmp.pop_front();
                        (asyncActor->chain = &asyncActorChain)->push_back(asyncActor);
                    }
                    else
                    {
                        assert(asyncActor->chain == &destroyedActorChain);
                    }
                }
            }
        }
    }
    
    inline
    IRefMapper& getRefMapper(void) const
    {
        return m_RefMapper;
    }
    
    void    onActorAdded(Actor *actor);
    void    onActorRemoved(Actor *actor);
    
private:

    void onNodeActorCountChange_LL(const int delta, const Actor *actor) noexcept;
    
    std::unique_ptr<IRefMapper>     m_RefMapperPtr;
    IRefMapper                      &m_RefMapper;
        
};

// returns [dispatched ok]
bool Actor::EventTable::onEvent(const Event &event, uint64_t &performanceCounter) const
{
    int i = 0;
    for (EventId eventId = event.getClassId(); i < HIGH_FREQUENCY_CALLBACK_ARRAY_SIZE && hfEvent[i].eventId != eventId;
         ++i)
    {
    }
    if (i == HIGH_FREQUENCY_CALLBACK_ARRAY_SIZE)
    {
        return onLowFrequencyEvent(event, performanceCounter);
    }
    ++performanceCounter;
    assert(hfEvent[i].staticEventHandler != 0);
    ENTERPRISE_0X5010(static_cast<Actor*>(static_cast<const Actor::EventTable*>(event.getDestinationActorId().eventTable)->asyncActor)->getAsyncNode(), &event, static_cast<void*>(hfEvent[i].eventHandler));
    bool ret =  (*hfEvent[i].staticEventHandler)(hfEvent[i].eventHandler, event);
    ENTERPRISE_0X5011(static_cast<Actor*>(static_cast<const Actor::EventTable*>(event.getDestinationActorId().eventTable)->asyncActor)->getAsyncNode());
    return ret;
}

Actor::EventTable::RegisteredEvent::RegisteredEvent() noexcept : eventId(MAX_EVENT_ID_COUNT),
                                                                      eventHandler(0),
                                                                      staticEventHandler(0)
{
}

} // namespace
