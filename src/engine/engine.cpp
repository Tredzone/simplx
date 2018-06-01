/**
 * @file engine.cpp
 * @brief Simplx engine class
 * @copyright 2013-2018 Tredzone (www.tredzone.com). All rights reserved.
 * Please see accompanying LICENSE file for licensing terms.
 */

#include "trz/engine/internal/node.h"

extern "C"
{
    int TREDZONE_SDK_ARCHITECTURE = tredzone::AsyncEngine::SDK_ARCHITECTURE;
    int TREDZONE_SDK_COMPATIBILITY_VERSION = tredzone::AsyncEngine::SDK_COMPATIBILITY_VERSION;
    int TREDZONE_SDK_PATCH_VERSION = tredzone::AsyncEngine::SDK_PATCH_VERSION;
    int TREDZONE_SDK_COMPILER_ID = TREDZONE_COMPILER_ID;
}

bool TREDZONE_SDK_IS_DEBUG_RELEASE_COMPATIBLE(bool isNDEBUG)
{
#ifndef NDEBUG
    // debug
    if (isNDEBUG)
    {
        return false;
    }
#else
    // release
    if (!isNDEBUG)
    {
        return false;
    }
#endif
    return true;
}

bool TREDZONE_SDK_IS_COMPILER_COMPATIBLE(int compilerId, const void *compilerVersion)
{
    if (compilerId == TREDZONE_SDK_COMPILER_ID)
    {
        const TREDZONE_SDK_COMPILER_VERSION_type &gccVersion =
            *static_cast<const TREDZONE_SDK_COMPILER_VERSION_type *>(compilerVersion);
#if defined(__GNUG__)
        return gccVersion.vmajor == __GNUC__ && gccVersion.vminor == __GNUC_MINOR__;
#else
#error Not supported C++ compiler
#endif
    }
    return false;
}

bool TREDZONE_SDK_COMPATIBLE(int sdkCompatibilityVersion, int sdkPatchVersion)
{
    return sdkCompatibilityVersion == TREDZONE_SDK_COMPATIBILITY_VERSION &&
           sdkPatchVersion <= TREDZONE_SDK_PATCH_VERSION;
}

namespace tredzone
{

ThreadLocalStorage<AsyncEngine *> AsyncEngine::currentEngineTLS;

AsyncEngineCustomEventLoopFactory::EventLoopAutoPointer AsyncEngineCustomEventLoopFactory::newEventLoop()
{
    return EventLoopAutoPointer::newEventLoop<DefaultEventLoop>();
}

AsyncEngineEventLoop::AsyncEngineEventLoop() noexcept : asyncNode(0),
                                                        interruptFlag(0),
                                                        isBeingDestroyedFlag(false)
#ifndef NDEBUG
                                                            ,
                                                        debugPreRunCalledFlag(false),
                                                        debugSynchronizeCount(0)
#endif
{
}

AsyncEngineEventLoop::~AsyncEngineEventLoop() noexcept
{
#ifndef NDEBUG
    if (debugPreRunCalledFlag)
    {
        assert(asyncNode != 0);
        assert(!isRunning());
        assert(asyncNode->nodeHandle.stopFlag || asyncNode->nodeHandle.shutdownFlag);
        assert(asyncNode->asyncActorChain.empty());
        assert(asyncNode->destroyedAsyncActorChain.empty());
    }
#endif
}

AsyncEngine &AsyncEngineEventLoop::getEngine() noexcept { return AsyncEngine::getEngine(); }

const AsyncEngine &AsyncEngineEventLoop::getEngine() const noexcept { return AsyncEngine::getEngine(); }

void AsyncEngineEventLoop::init(AsyncNode &pasyncNode) noexcept
{
    assert(isBeingDestroyedFlag == false);
    assert(!debugPreRunCalledFlag);
    assert(asyncNode == 0);
    asyncNode = &pasyncNode;
    assert(interruptFlag == 0);
    interruptFlag = &asyncNode->nodeHandle.interruptFlag;
}

void AsyncEngineEventLoop::preRun() noexcept
{
#ifndef NDEBUG
    assert(!debugPreRunCalledFlag);
    debugPreRunCalledFlag = true;
    ++debugSynchronizeCount;
#endif
    asyncNode->synchronizePreBarrier();
}

void AsyncEngineEventLoop::postRun() noexcept
{
#ifndef NDEBUG
    ++debugSynchronizeCount;
#endif
    assert(debugPreRunCalledFlag);
    asyncNode->synchronizePostBarrier();
}

void AsyncEngineEventLoop::runWhileNotInterrupted() noexcept
{
#ifndef NDEBUG
    ++debugSynchronizeCount;
#endif
    assert(asyncNode != 0);
    assert(interruptFlag != 0);
    assert(debugPreRunCalledFlag);
    asyncNode->synchronizePostBarrier();
    do
    {
        asyncNode->synchronize();
    } while (!*interruptFlag);
    asyncNode->synchronizePreBarrier();
}

#ifndef NDEBUG
bool AsyncEngineEventLoop::debugIsInNodeThread() const noexcept
{
    assert(asyncNode != 0);
    return asyncNode->debugGetThreadId() == ThreadId::current();
}
#endif

Actor::NodeId AsyncEngineEventLoop::getNodeId() const noexcept
{
    assert(asyncNode != 0);
    return asyncNode->id;
}

Actor::CoreId AsyncEngineEventLoop::getCoreId() const noexcept
{
    assert(asyncNode != 0);
    return asyncNode->getCoreSet().at(asyncNode->id);
}

AsyncExceptionHandler &AsyncEngineEventLoop::getAsyncExceptionHandler() const noexcept
{
    assert(asyncNode != 0);
    return asyncNode->nodeManager.exceptionHandler;
}

void AsyncEngineEventLoop::returnToSender(const AsyncActor::Event &event) noexcept
{
#ifndef NDEBUG
    bool debugFoundFlag = false;
#endif
    assert(asyncNode != 0);
    assert(asyncNode->debugSynchronizePostBarrierFlag);
    assert(event.getDestinationActorId().isInProcess());
    assert((event.isRouted() && event.isRouteToSource()) || event.getSourceActorId().isInProcess());
    Actor::NodeId sourceNodeId =
        event.isRouted() ? event.getRouteId().getNodeId() : event.getSourceInProcessActorId().getNodeId();
    if (sourceNodeId == asyncNode->id)
    {
        AsyncNodesHandle::WriterSharedHandle &writerSharedHandle =
            asyncNode->nodeHandle.getWriterSharedHandle(sourceNodeId);
        assert(debugFoundFlag == false);
#ifndef NDEBUG
        debugFoundFlag = true;
#endif
        writerSharedHandle.onUndeliveredEvent(event);
    }
    else
    {
        AsyncNodesHandle::ReaderSharedHandle &readerSharedHandler =
            asyncNode->nodeHandle.getReaderSharedHandle(sourceNodeId);
        assert(debugFoundFlag == false);
#ifndef NDEBUG
        debugFoundFlag =
#endif
            readerSharedHandler.returnToSender(event);
    }
    assert(debugFoundFlag == true);
}

bool AsyncEngineEventLoop::isRunning() noexcept
{
    assert(asyncNode != 0);
    assert(debugPreRunCalledFlag);
    assert(asyncNode->nodeHandle.interruptFlag);
    asyncNode->nodeHandle.interruptFlag = false;

    memoryBarrier();

    if (isBeingDestroyedFlag)
    {
        asyncNode->nodeHandle.interruptFlag = true;
        assert(asyncNode->nodeHandle.stopFlag || asyncNode->nodeHandle.shutdownFlag);
        return !asyncNode->asyncActorChain.empty() || !asyncNode->destroyedAsyncActorChain.empty();
    }
    else if (asyncNode->nodeHandle.stopFlag || asyncNode->nodeHandle.shutdownFlag)
    {
        asyncNode->nodeHandle.interruptFlag = true;
        isBeingDestroyedFlag = true;
        asyncNode->destroyAsyncActors();
    }
    return true;
}

void AsyncEngineEventLoop::synchronize() noexcept
{
#ifndef NDEBUG
    ++debugSynchronizeCount;
#endif
    assert(asyncNode != 0);
    assert(debugPreRunCalledFlag);
    asyncNode->synchronizePostBarrier();
    asyncNode->synchronizePreBarrier();
}

void AsyncEngineCustomEventLoopFactory::DefaultEventLoop::run() noexcept
{
    assert(asyncNode != 0);
    assert(interruptFlag != 0);
    assert(debugPreRunCalledFlag);
    asyncNode->synchronizePostBarrier();
    while (isRunning())
    {
        do
        {
            asyncNode->synchronize();
        } while (!*interruptFlag);
    }
    asyncNode->synchronizePreBarrier();
}

class AsyncEngine::CoreActor : public AsyncActor
{
  public:
    struct Init
    {
        AsyncEngine &asyncEngine;
        const CoreId coreId;
        const bool isRedZoneFlag;
        inline Init(AsyncEngine &pasyncEngine, CoreId pcoreId, bool pisRedZoneFlag) noexcept
            : asyncEngine(pasyncEngine),
              coreId(pcoreId),
              isRedZoneFlag(pisRedZoneFlag)
        {
        }
    };
    /**
     * throw (std::bad_alloc)
     */
    CoreActor(const Init &init)
        : stopFlag(asyncNode->nodeHandle.stopFlag), shutdownFlag(asyncNode->nodeHandle.shutdownFlag),
          regularActorsCoreCount(*init.asyncEngine.regularActorsCoreCount), regularActorsCoreCountFlag(false),
          serviceSingletonActor(newReferencedSingletonActor<ServiceSingletonActor>()),
          customCoreActor(init.asyncEngine.customCoreActorFactory.newCustomCoreActor(init.asyncEngine, init.coreId,
                                                                                     init.isRedZoneFlag, *this)),
          nodeActorCountListener(*this), loopForRegularActorsCoreCountZeroCallback(*this)
    {
        asyncNode->subscribeNodeActorCountListener(nodeActorCountListener);
        assert(serviceSingletonActor->isServiceDestroyTimeFlag);
        serviceSingletonActor->isServiceDestroyTimeFlag = false;
        atomicAddAndFetch(&regularActorsCoreCount, 1);
    }
    virtual ~CoreActor() noexcept { nodeActorCountListener.unsubscribe(); }

  private:
    struct LoopForRegularActorsCoreCountZeroCallback : Callback
    {
        CoreActor &coreActor;

        inline LoopForRegularActorsCoreCountZeroCallback(CoreActor &pcoreActor) noexcept : coreActor(pcoreActor) {}
        inline void onCallback() noexcept
        {
            if (coreActor.regularActorsCoreCount == 0)
            {
                coreActor.serviceSingletonActor->destroyServiceActors();
            }
            else
            {
                coreActor.registerCallback(*this);
            }
        }
    };
    struct NodeActorCountListener : AsyncNodeBase::NodeActorCountListener
    {
        CoreActor &coreActor;
        NodeActorCountListener(CoreActor &pcoreActor) noexcept : coreActor(pcoreActor) {}
        virtual void onNodeActorCountChange(size_t oldCount, size_t newCount) noexcept
        {
            if (oldCount > newCount)
            {
                coreActor.onNodeActorsDestroyed();
            }
        }
    };

    bool &stopFlag;
    const bool &shutdownFlag;
    unsigned &regularActorsCoreCount;
    bool regularActorsCoreCountFlag;
    ActorReference<ServiceSingletonActor> serviceSingletonActor;
    ActorReference<AsyncActor> customCoreActor;
    NodeActorCountListener nodeActorCountListener;
    LoopForRegularActorsCoreCountZeroCallback loopForRegularActorsCoreCountZeroCallback;

    static const AsyncActor::ActorReferenceBase *findFirstActorReference(const AsyncActor &referencingActor,
                                                                         const AsyncActor &referencedActor) noexcept
    {
        if (&referencingActor == &referencedActor)
        {
            return 0;
        }
        const AsyncActor::ActorReferenceBase *ret = 0;
        for (AsyncActor::ReferenceToChain::const_iterator i = referencingActor.referenceToChain.begin(),
                                                          endi = referencingActor.referenceToChain.end();
             ret == 0 && i != endi; ++i)
        {
            const AsyncActor &actor = *i->getReferencedActor();
            if (&actor == &referencedActor)
            {
                ret = &*i;
            }
            else
            {
                ret = findFirstActorReference(actor, referencedActor);
            }
        }
        return ret;
    }
    const AsyncActor::ActorReferenceBase *
    findFirstActorReferenceWithinCoreAndServiceReferencedActors(const AsyncActor &referencedActor) const noexcept
    {
        assert(referencedActor.referenceFromCount > 1);
        const AsyncActor::ActorReferenceBase *ret =
            (this->referenceFromCount == 0 ? findFirstActorReference(*this, referencedActor) : 0);
        for (ServiceSingletonActor::ServiceActorList::const_iterator
                 i = serviceSingletonActor->getServiceActorList().begin(),
                 endi = serviceSingletonActor->getServiceActorList().end();
             ret == 0 && i != endi; ++i)
        {
            ret = ((*i)->referenceFromCount == 0 ? findFirstActorReference(**i, referencedActor) : 0);
        }
        assert(ret != 0);
        return ret;
    }
    size_t countReferencedActors(const AsyncActor &referencingActor) const noexcept
    {
        size_t ret = 0;
        for (AsyncActor::ReferenceToChain::const_iterator i = referencingActor.referenceToChain.begin(),
                                                          endi = referencingActor.referenceToChain.end();
             i != endi; ++i)
        {
            const AsyncActor &actor = *i->getReferencedActor();
            assert(actor.referenceFromCount >= 1);
            if (actor.referenceFromCount == 1 ||
                findFirstActorReferenceWithinCoreAndServiceReferencedActors(actor) == &*i)
            {
                ret += 1 + countReferencedActors(actor);
            }
        }
        return ret;
    }
    bool onlyCoreAndServiceReferencedActorsLeft() const noexcept
    {
        size_t count = (this->referenceFromCount == 0 ? 1 + countReferencedActors(*this) : 0);
        for (ServiceSingletonActor::ServiceActorList::const_iterator
                 i = serviceSingletonActor->getServiceActorList().begin(),
                 endi = serviceSingletonActor->getServiceActorList().end();
             i != endi; ++i)
        {
            count += ((*i)->referenceFromCount == 0 ? 1 + countReferencedActors(**i) : 0);
        }
        return count == asyncNode->getActorCount();
    }
    virtual void onDestroyRequest() noexcept
    {
        if (onlyCoreAndServiceReferencedActorsLeft())
        {
            if (serviceSingletonActor->getServiceActorList().empty())
            {
                stopFlag = true;
                if (!regularActorsCoreCountFlag)
                {
                    regularActorsCoreCountFlag = true;
                    atomicSubAndFetch(&regularActorsCoreCount, 1);
                }
                AsyncActor::onDestroyRequest();
            }
            else if (shutdownFlag && !regularActorsCoreCountFlag)
            {
                regularActorsCoreCountFlag = true;
                atomicSubAndFetch(&regularActorsCoreCount, 1);
                registerCallback(loopForRegularActorsCoreCountZeroCallback);
            }
        }
    }
    void onNodeActorsDestroyed() noexcept
    {
        if (onlyCoreAndServiceReferencedActorsLeft() &&
            (serviceSingletonActor->getServiceActorList().empty() ||
             (shutdownFlag && !serviceSingletonActor->isServiceDestroyTimeFlag)))
        {
            requestDestroy();
        }
    }
};

Mutex AsyncEngine::ServiceIndex::mutex;
unsigned AsyncEngine::ServiceIndex::staticIndexValue = 0;

AsyncEngine::AsyncEngine(const StartSequence &startSequence)
    : engineName(startSequence.getEngineName()), engineSuffix(startSequence.getEngineSuffix()),
      threadStackSizeByte(startSequence.getThreadStackSizeByte()), redZoneParam(startSequence.getRedZoneParam()),
      regularActorsCoreCount(1u), defaultCoreActorFactory(new AsyncEngineCustomCoreActorFactory),
      defaultEventLoopFactory(new AsyncEngineCustomEventLoopFactory),
      nodeManager(startSequence.getAsyncExceptionHandler() == 0
                      ? new AsyncNodeManager(startSequence.getEventAllocatorPageSizeByte(), startSequence.getCoreSet())
                      : new AsyncNodeManager(*startSequence.getAsyncExceptionHandler(),
                                             startSequence.getEventAllocatorPageSizeByte(),
                                             startSequence.getCoreSet())),
      customCoreActorFactory(
          startSequence.getAsyncEngineCustomCoreActorFactory() == 0
              ? *defaultCoreActorFactory
              : (defaultCoreActorFactory.reset(), *startSequence.getAsyncEngineCustomCoreActorFactory())),
      customEventLoopFactory(
          startSequence.getAsyncEngineCustomEventLoopFactory() == 0
              ? *defaultEventLoopFactory
              : (defaultEventLoopFactory.reset(), *startSequence.getAsyncEngineCustomEventLoopFactory()))
{
    try
    {
        *cacheLineHeaderPadding = *cacheLineTrailerPadding = '\0'; // to silence unused private field warning
        currentEngineTLS.set(this);
        start(startSequence);
        currentEngineTLS.set(0);
    }
    catch (const std::exception &e)
    {
#ifndef NDEBUG
        std::cerr << "DEBUG (" << __FILE__ << ":" << __LINE__ << "): " << e.what() << std::endl;
#endif
        currentEngineTLS.set(0);
        finish();
        throw;
    }
    catch (...)
    {
#ifndef NDEBUG
        std::cerr << "DEBUG (" << __FILE__ << ":" << __LINE__ << "): ?" << std::endl;
#endif
        currentEngineTLS.set(0);
        finish();
        throw;
    }
}

AsyncEngine::~AsyncEngine() noexcept { finish(); }

std::string AsyncEngine::getVersion()
{
    std::stringstream ss;
    ss << "Tredzone C++ Simplx framework" << (int)TREDZONE_ENGINE_VERSION_MAJOR << "." << (int)TREDZONE_ENGINE_VERSION_MINOR;
    if (TREDZONE_ENGINE_VERSION_PATCH != 0)
    {
        ss << '.' << (int)TREDZONE_ENGINE_VERSION_PATCH;
    }
    ss << " (";
    if (strcmp("master", TREDZONE_ENGINE_VERSION_BRANCH) == 0)
    {
        ss << TREDZONE_ENGINE_VERSION_BRANCH;
    }
    else
    {
        ss << "dev-" << TREDZONE_ENGINE_VERSION_BRANCH;
    }
    ss << ") "
#ifndef NDEBUG
       << "debug "
#else
       << "release "
#endif
#ifdef TREDZONE_PLATFORM_LINUX
       << "linux "
#elif defined(TREDZONE_PLATFORM_APPLE)
       << "apple "
#elif defined(TREDZONE_PLATFORM_WINDOWS)
       << "windows "
#endif
#if TREDZONE_COMPILER_ID == 1
       << "gcc" << (int)__GNUC__ << '.' << (int)__GNUC_MINOR__
#else
       << "COMPILER=" << (int)TREDZONE_COMPILER_ID
#endif
       << ' ';
    if (SDK_ARCHITECTURE == 1)
    {
        ss << "x86";
    }
    else
    {
        ss << "ARCHITECTURE=" << (int)SDK_ARCHITECTURE;
    }
    ss << ' ' << sizeof(void *) * 8 << "-bit" << std::ends;
    return ss.str();
}

void AsyncEngine::finish() noexcept
{
    nodeManager->shutdown();
    atomicSubAndFetch(&*regularActorsCoreCount, 1);
    nodeManager.reset();
}

const AsyncEngine::CoreSet &AsyncEngine::getCoreSet() const noexcept { return nodeManager->getCoreSet(); }

size_t AsyncEngine::getEventAllocatorPageSizeByte() const noexcept { return nodeManager->getEventAllocatorPageSize(); }

#ifndef NDEBUG
void AsyncEngine::debugActivateMemoryLeakBacktrace() noexcept
{
    AsyncNodeAllocator::debugActivateMemoryLeakBacktraceFlag = true;
}
#endif

struct AsyncEngine_start_NodeThread
{ // has to be declared out of AsyncEngine::start() to be used as a template parameter of
  // AsyncEngine::start::NodeThreadList (GCC compliance)
    const AsyncActor::CoreId coreId;
    std::unique_ptr<AsyncNode::Thread> thread;
    CacheLineAlignedObject<AsyncNode> *node;
    AsyncEngine_start_NodeThread(const AsyncEngine_start_NodeThread &other) : coreId(other.coreId), node(0) {}
    AsyncEngine_start_NodeThread(AsyncActor::CoreId pcoreId) : coreId(pcoreId), node(0) {}
    ~AsyncEngine_start_NodeThread()
    {
        if (node != 0)
        {
            assert(thread.get() != 0);
            thread->run(*node);
        }
    }
};

void AsyncEngine::start(const StartSequence &startSequence)
{
    struct NodeThreadList : std::list<AsyncEngine_start_NodeThread>
    {
        iterator find(CoreId coreId) noexcept
        {
            iterator ret = begin();
            for (; ret != end() && ret->coreId != coreId; ++ret)
            {
            }
            return ret;
        }
    };

    NodeThreadList nodeThreadList;
    StartSequence::StarterChain::const_iterator ilastService = startSequence.starterChain.end();
    for (StartSequence::StarterChain::const_iterator i = startSequence.starterChain.begin(),
                                                     endi = startSequence.starterChain.end();
         i != endi; ++i)
    {
        CoreId coreId = i->coreId;
        NodeThreadList::iterator inodeThread = nodeThreadList.find(coreId);
        if (inodeThread == nodeThreadList.end())
        {
            nodeThreadList.push_back(coreId);
            nodeThreadList.back().thread.reset(new AsyncNode::Thread(coreId, startSequence.isRedZoneCore(coreId),
                                                                     redZoneParam, threadStackSizeByte, threadStartHook,
                                                                     this, 0));
            cpuset_type savedThreadAffinity = threadGetAffinity();
            try
            {
                threadSetAffinity(coreId);
                nodeThreadList.back().node = new CacheLineAlignedObject<AsyncNode>(
                    AsyncNode::Init(*nodeManager.get(), coreId, customEventLoopFactory));
            }
            catch (...)
            {
                threadSetAffinity(savedThreadAffinity);
                throw;
            }
            threadSetAffinity(savedThreadAffinity);
        }
        if (i->isServiceFlag)
        {
            ilastService = i;
        }
    }
    AsyncActor::ActorId previousServiceDestroyActorId;
    for (StartSequence::StarterChain::const_iterator i = startSequence.starterChain.begin(),
                                                     endi = startSequence.starterChain.end();
         i != endi; ++i)
    {
        CoreId coreId = i->coreId;
        NodeThreadList::iterator inodeThread = nodeThreadList.find(coreId);
        assert(inodeThread != nodeThreadList.end());
        assert(inodeThread->coreId == coreId);
        assert(inodeThread->node != 0);
        cpuset_type savedThreadAffinity = threadGetAffinity();
        try
        {
            threadSetAffinity(coreId);
            AsyncActor::ActorId serviceDestroyActorId =
                i->onStart(*this, **inodeThread->node, previousServiceDestroyActorId, i == ilastService);
            if (i->isServiceFlag)
            {
                assert(serviceDestroyActorId != tredzone::null);
                previousServiceDestroyActorId = serviceDestroyActorId;
            }
        }
        catch (...)
        {
            threadSetAffinity(savedThreadAffinity);
            throw;
        }
        threadSetAffinity(savedThreadAffinity);
    }
    for (NodeThreadList::iterator i = nodeThreadList.begin(), endi = nodeThreadList.end(); i != endi; ++i)
    {
        assert(i->node != 0);
        cpuset_type savedThreadAffinity = threadGetAffinity();
        try
        {
            threadSetAffinity(i->coreId);
            (*i->node)->newAsyncActor<CoreActor>(
                CoreActor::Init(*this, i->coreId, startSequence.isRedZoneCore(i->coreId)));
        }
        catch (...)
        {
            threadSetAffinity(savedThreadAffinity);
            throw;
        }
        threadSetAffinity(savedThreadAffinity);
    }
}

void AsyncEngine::threadStartHook(void *engine)
{
    assert(AsyncEngine::currentEngineTLS.get() == 0);
    AsyncEngine::currentEngineTLS.set(static_cast<AsyncEngine *>(engine));
}

AsyncActor::ActorId AsyncEngine::newCore(CoreId coreId, bool isRedZone, NewCoreStarter &newCoreStarter)
{
    AsyncNode::Thread thread(coreId, isRedZone, redZoneParam, threadStackSizeByte, threadStartHook, this, 0);
    AsyncActor::ActorId ret;
    CacheLineAlignedObject<AsyncNode> *node = 0;
    cpuset_type savedThreadAffinity = threadGetAffinity();
    try
    {
        threadSetAffinity(coreId);
        node =
            new CacheLineAlignedObject<AsyncNode>(AsyncNode::Init(*nodeManager.get(), coreId, customEventLoopFactory));
        (*node)->newAsyncActor<CoreActor>(CoreActor::Init(*this, coreId, isRedZone));
        ret = newCoreStarter.start(**node);
    }
    catch (...)
    {
        threadSetAffinity(savedThreadAffinity);
        if (node != 0)
        {
            (*node)->stop();
            thread.run(*node);
        }
        throw;
    }
    threadSetAffinity(savedThreadAffinity);
    assert(node != 0);
    thread.run(*node);
    return ret;
}

AsyncEngine::CoreSet::CoreSet() noexcept : coreCount(0) {}

void AsyncEngine::CoreSet::set(CoreId coreId)
{
    size_t i = 0;
    for (; i < coreCount && cores[i] != coreId; ++i)
    {
    }
    if (i == coreCount)
    {
        if (coreCount == (size_t)AsyncActor::MAX_NODE_COUNT)
        {
            throw TooManyCoresException();
        }
        ++coreCount;
        cores[i] = coreId;
    }
}

AsyncEngine::CoreSet::NodeId AsyncEngine::CoreSet::index(CoreId coreId) const
{
    size_t i = 0;
    for (; i < coreCount && cores[i] != coreId; ++i)
    {
    }
    if (i == coreCount)
    {
        throw UndefinedCoreException();
    }
    return (NodeId)i;
}

AsyncEngine::CoreId AsyncEngine::CoreSet::at(NodeId nodeId) const
{
    if (nodeId >= coreCount)
    {
        throw UndefinedCoreException();
    }
    return cores[nodeId];
}

AsyncEngine::FullCoreSet::FullCoreSet()
{
    size_t n = cpuGetCount();
    if (n > (size_t)AsyncActor::MAX_NODE_COUNT)
    {
        throw TooManyCoresException();
    }
    for (size_t i = 0; i < n; ++i)
    {
        set((CoreId)i);
    }
}

/**
 * throw (std::bad_alloc)
 */
AsyncEngine::StartSequence::StartSequence(const CoreSet &pcoreSet, int)
    : coreSet(pcoreSet), asyncNodeAllocator(new AsyncNodeAllocator), asyncExceptionHandler(0),
      asyncEngineCustomCoreActorFactory(0), asyncEngineCustomEventLoopFactory(0),
      eventAllocatorPageSizeByte(DEFAULT_EVENT_ALLOCATOR_PAGE_SIZE), threadStackSizeByte(0)
{
    std::stringstream s;
    s << (uint64_t)getPID() << '-' << getTSC() << std::ends;
    engineSuffix = s.str();
}

AsyncEngine::StartSequence::~StartSequence() noexcept
{
    while (!starterChain.empty())
    {
        delete starterChain.pop_front();
    }
}

AsyncActor::AllocatorBase AsyncEngine::StartSequence::getAllocator() const noexcept
{
    return AsyncActor::AllocatorBase(*asyncNodeAllocator);
}

void AsyncEngine::StartSequence::setRedZoneParam(const ThreadRealTimeParam &predZoneParam) noexcept
{
    redZoneParam = predZoneParam;
}

const ThreadRealTimeParam &AsyncEngine::StartSequence::getRedZoneParam() const noexcept { return redZoneParam; }

/**
 * throw (std::bad_alloc)
 */
void AsyncEngine::StartSequence::setRedZoneCore(CoreId coreId)
{
    if (!isRedZoneCore(coreId))
    {
        redZoneCoreIdList.push_back(coreId);
    }
}

void AsyncEngine::StartSequence::setBlueZoneCore(CoreId coreId) noexcept { redZoneCoreIdList.remove(coreId); }

bool AsyncEngine::StartSequence::isRedZoneCore(CoreId coreId) const noexcept
{
    RedZoneCoreIdList::const_iterator i = redZoneCoreIdList.begin(), endi = redZoneCoreIdList.end();
    for (; i != endi && *i != coreId; ++i)
    {
    }
    return i != endi;
}

void AsyncEngine::StartSequence::setAsyncExceptionHandler(AsyncExceptionHandler &pasyncExceptionHandler) noexcept
{
    asyncExceptionHandler = &pasyncExceptionHandler;
}

void AsyncEngine::StartSequence::setAsyncEngineCustomCoreActorFactory(
    AsyncEngineCustomCoreActorFactory &pasyncEngineCustomCoreActorFactory) noexcept
{
    asyncEngineCustomCoreActorFactory = &pasyncEngineCustomCoreActorFactory;
}

void AsyncEngine::StartSequence::setAsyncEngineCustomEventLoopFactory(
    AsyncEngineCustomEventLoopFactory &pasyncEngineCustomEventLoopFactory) noexcept
{
    asyncEngineCustomEventLoopFactory = &pasyncEngineCustomEventLoopFactory;
}

void AsyncEngine::StartSequence::setEventAllocatorPageSizeByte(size_t peventAllocatorPageSizeByte) noexcept
{
    eventAllocatorPageSizeByte = peventAllocatorPageSizeByte;
}

void AsyncEngine::StartSequence::setThreadStackSizeByte(size_t pthreadStackSizeByte) noexcept
{
    threadStackSizeByte = pthreadStackSizeByte;
}

AsyncExceptionHandler *AsyncEngine::StartSequence::getAsyncExceptionHandler() const noexcept
{
    return asyncExceptionHandler;
}

AsyncEngineCustomCoreActorFactory *AsyncEngine::StartSequence::getAsyncEngineCustomCoreActorFactory() const noexcept
{
    return asyncEngineCustomCoreActorFactory;
}

AsyncEngineCustomEventLoopFactory *AsyncEngine::StartSequence::getAsyncEngineCustomEventLoopFactory() const noexcept
{
    return asyncEngineCustomEventLoopFactory;
}

size_t AsyncEngine::StartSequence::getEventAllocatorPageSizeByte() const noexcept { return eventAllocatorPageSizeByte; }

size_t AsyncEngine::StartSequence::getThreadStackSizeByte() const noexcept { return threadStackSizeByte; }

/**
 * throw (CoreSet::TooManyCoresException)
 */
AsyncEngine::CoreSet AsyncEngine::StartSequence::getCoreSet() const
{
    CoreSet ret = coreSet;
    for (StarterChain::const_iterator i = starterChain.begin(), endi = starterChain.end(); i != endi; ++i)
    {
        ret.set(i->coreId);
    }
    return ret;
}

void AsyncEngine::StartSequence::setEngineName(const char *pengineName) { engineName = pengineName; }

const char *AsyncEngine::StartSequence::getEngineName() const noexcept { return engineName.c_str(); }

void AsyncEngine::StartSequence::setEngineSuffix(const char *pengineSuffix) { engineSuffix = pengineSuffix; }

const char *AsyncEngine::StartSequence::getEngineSuffix() const noexcept { return engineSuffix.c_str(); }
}