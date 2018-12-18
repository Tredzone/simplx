/**
 * @file testasynceventloop.cpp
 * @brief test event loop
 * @copyright 2013-2018 Tredzone (www.tredzone.com). All rights reserved.
 * Please see accompanying LICENSE file for licensing terms.
 */

#include <map>

#include "gtest/gtest.h"

#include "testutil.h"

using namespace tredzone;
using namespace std;

// need anonymous namespace to avoid clashes in same compilation unit !
namespace
{

class TestEventLoop : public EngineEventLoop
{
  public:
    class Shared
    {
      public:
        inline Shared() : eventLoopCount(0), eventLoopCountMax(0) {}
        inline ~Shared()
        {
            Mutex::Lock lock(mutex);
            EXPECT_EQ(0, eventLoopCount);
            // EXPECT_GT(eventLoopCountMax, 0u);
            EXPECT_GE(eventLoopCountMax, 0u); // will be zero if instantiated, never used, then destroyed
        }
        inline void incrementEventLoopThreadSafe()
        {
            Mutex::Lock lock(mutex);
            ++eventLoopCount;
            ++eventLoopCountMax;
            // std::cout << "++TestEventLoop, eventLoopCount=" << eventLoopCount << std::endl;
        }
        inline void decrementEventLoopThreadSafe() noexcept
        {
            Mutex::Lock lock(mutex);
            ASSERT_GT(eventLoopCount, 0);
            --eventLoopCount;
            // std::cout << "--TestEventLoop, eventLoopCount=" << eventLoopCount << std::endl;
        }

      private:
        Mutex mutex;
        int eventLoopCount;
        unsigned eventLoopCountMax;
    };

    const size_t eventAllocatorPageSizeByte;

    inline TestEventLoop(const std::pair<size_t, Shared *> &init)
        : eventAllocatorPageSizeByte(init.first), shared(*init.second)
    {
        shared.incrementEventLoopThreadSafe();
    }

    ~TestEventLoop() noexcept override // would never get called if in same compilation unit (and namespace) as
                                       // testasync.cpp
    {
        shared.decrementEventLoopThreadSafe();
    }

  protected:
    void run() noexcept override
    {
        while (isRunning())
        {
            do
            {
                synchronize();
            } while (!isInterrupted());
        }
    }

  private:
    Shared &shared;
};

template <class _EventLoop>
class TestEventLoopFactory : public EngineCustomEventLoopFactory, public TestAsyncExceptionHandler
{
  public:
    const size_t eventAllocatorPageSizeByte;

    inline TestEventLoopFactory() : eventAllocatorPageSizeByte(4096) {}

    ~TestEventLoopFactory() override {}

    EventLoopAutoPointer newEventLoop() override
    {
        return EventLoopAutoPointer::newEventLoop<_EventLoop>(
            std::make_pair(eventAllocatorPageSizeByte, &eventLoopShared));
    }

  private:
    TestEventLoop::Shared eventLoopShared;
};

template <class _EventLoopFactory> class TestStartSequence : public Engine::StartSequence
{
  public:
    inline TestStartSequence(_EventLoopFactory &testEventLoopFactory, size_t eventAllocatorPageSizeByte)
    {
        setExceptionHandler(testEventLoopFactory);
        setEngineCustomEventLoopFactory(testEventLoopFactory);
        setEventAllocatorPageSizeByte(eventAllocatorPageSizeByte);
    }
};

void testInit()
{
    TestEventLoopFactory<TestEventLoop> eventLoopFactory;
    TestStartSequence<TestEventLoopFactory<TestEventLoop>> startSequence(eventLoopFactory,
                                                                         eventLoopFactory.eventAllocatorPageSizeByte);
    startSequence.addActor<Actor>(0);
    Engine engine(startSequence);
}

class TestLocalEventLoop;

class TestLocalEventActorBase : public Actor
{
  public:
    struct TestEvent : Event, MultiForwardChainLink<TestEvent>
    {
        const unsigned count;
        mutable TestLocalEventActorBase *actor;
        inline TestEvent(unsigned pcount) noexcept : count(pcount), actor(0) {}
    };
    typedef MultiForwardChainLink<TestEvent>::ForwardChain<> TestEventChain;

    inline TestLocalEventActorBase(unsigned eventBatchSize)
        : eventCountMax(eventBatchSize * 10), eventCount(0), eventCountToBeReceivedNext(0)
    {
    }
    inline void onEvent(const TestEvent &);
    virtual void eventLoopOnEvent(TestEvent &) noexcept = 0;

  protected:
    friend class TestLocalEventLoop;

    const unsigned eventCountMax;
    unsigned eventCount;
    unsigned eventCountToBeReceivedNext;

    virtual void onDestroyRequest() noexcept
    {
        if (eventCount == eventCountMax)
        {
            Actor::onDestroyRequest();
        }
        else
        {
            requestDestroy();
        }
    }

    inline TestLocalEventLoop &getEventLoop();
};

class TestLocalEventLoop : public TestEventLoop
{
  public:
    inline TestLocalEventLoop(const std::pair<size_t, Shared *> &init)
        : TestEventLoop(init)
#ifndef NDEBUG
          ,
          debugLastSynchronizeCount(0)
#endif
    {
    }
    virtual ~TestLocalEventLoop() noexcept { EXPECT_TRUE(testEventChain.empty()); }
    inline size_t getEventFillerSize() const noexcept { return eventAllocatorPageSizeByte / 10; }
    inline void addTestEvent(TestLocalEventActorBase::TestEvent &event) noexcept { testEventChain.push_back(&event); }
#ifndef NDEBUG
    inline void debugSynchronizeCount()
    {
        if (debugLastSynchronizeCount == 0)
        {
            debugLastSynchronizeCount = debugGetSynchronizeCount();
        }
        else
        {
            ASSERT_EQ(debugLastSynchronizeCount, debugGetSynchronizeCount());
        }
    }
#endif

  protected:
    TestLocalEventActorBase::TestEventChain testEventChain;

    virtual void run() noexcept
    {
        TestLocalEventActorBase *localActor = 0;
        while (isRunning())
        {
            do
            {
#ifndef NDEBUG
                ASSERT_TRUE((debugLastSynchronizeCount == 0 && testEventChain.empty()) ||
                            debugLastSynchronizeCount == debugGetSynchronizeCount());
#endif
#ifndef NDEBUG
                debugLastSynchronizeCount = 0;
#endif
                while (!testEventChain.empty())
                {
                    TestLocalEventActorBase::TestEvent &event = *testEventChain.pop_front();
                    ASSERT_TRUE(localActor == 0 || localActor == event.actor);
                    localActor = event.actor;
                    if (localActor->eventCount < localActor->eventCountMax)
                    {
                        localActor->eventLoopOnEvent(event);
                        ++(localActor->eventCount);
                    }
                }
                synchronize();
            } while (!isInterrupted());
        }
    }

  private:
#ifndef NDEBUG
    unsigned debugLastSynchronizeCount;
#endif
};

class TestLocalEventActor : public TestLocalEventActorBase
{
  public:
    inline TestLocalEventActor(unsigned eventBatchSize);
    virtual void eventLoopOnEvent(TestEvent &) noexcept;
};

TestLocalEventActor::TestLocalEventActor(unsigned eventBatchSize) : TestLocalEventActorBase(eventBatchSize)
{
    registerEventHandler<TestEvent>(*this);
    Event::Pipe pipe(*this, *this);
    for (; eventCount < eventBatchSize; ++eventCount)
    {
        pipe.push<TestEvent>(eventCount);
        pipe.allocate<char>(getEventLoop().getEventFillerSize());
    }
}

void TestLocalEventActorBase::onEvent(const TestEvent &event)
{
    ASSERT_EQ(event.count, eventCountToBeReceivedNext++);
    TestLocalEventLoop &eventLoop = getEventLoop();
#ifndef NDEBUG
    eventLoop.debugSynchronizeCount();
#endif
    eventLoop.addTestEvent(const_cast<TestEvent &>(event));
    event.actor = this;
}

void TestLocalEventActor::eventLoopOnEvent(TestEvent &) noexcept
{
    // std::cout << "TestLocalEventActor::eventLoopOnEvent(), eventCount=" << eventCount << std::endl;
    Event::Pipe pipe(*this, *this);
    pipe.push<TestEvent>(eventCount);
    pipe.allocate<char>(getEventLoop().getEventFillerSize());
}

TestLocalEventLoop &TestLocalEventActorBase::getEventLoop()
{
    EngineEventLoop &eventLoop = Actor::getEventLoop();
    EXPECT_TRUE(dynamic_cast<TestLocalEventLoop *>(&eventLoop) != 0);
    return static_cast<TestLocalEventLoop &>(eventLoop);
}

void testLocalEvent()
{
    for (unsigned i = 1; i <= 100; ++i)
    {
        // std::cout << "testLocalEvent(), i=" << i << std::endl;
        TestEventLoopFactory<TestLocalEventLoop> eventLoopFactory;
        TestStartSequence<TestEventLoopFactory<TestLocalEventLoop>> startSequence(
            eventLoopFactory, eventLoopFactory.eventAllocatorPageSizeByte);
        startSequence.addActor<TestLocalEventActor>(0, i);
        Engine engine(startSequence);
    }
}

struct TestPingPongEventActor : TestLocalEventActorBase
{
    inline TestPingPongEventActor(unsigned eventBatchSize) : TestLocalEventActorBase(eventBatchSize) {}
    virtual void eventLoopOnEvent(TestEvent &event) noexcept
    {
        // std::cout << "TestPingPongEventActor::eventLoopOnEvent(), eventCount=" << eventCount << std::endl;
        Event::Pipe pipe(*this, event.getSourceActorId());
        pipe.push<TestEvent>(eventCount);
        pipe.allocate<char>(getEventLoop().getEventFillerSize());
    }
};

class TestPingEventActor : public TestPingPongEventActor
{
  public:
    struct Init
    {
        unsigned eventBatchSize;
        const ActorId &pongActorId;
        inline Init(unsigned peventBatchSize, const ActorId &ppongActorId) noexcept : eventBatchSize(peventBatchSize),
                                                                                      pongActorId(ppongActorId)
        {
        }
    };

    inline TestPingEventActor(const Init &init) : TestPingPongEventActor(init.eventBatchSize)
    {
        EXPECT_NE(init.pongActorId, ActorId());
        registerEventHandler<TestEvent>(*this);
        Event::Pipe pipe(*this, init.pongActorId);
        for (; eventCount < init.eventBatchSize; ++eventCount)
        {
            pipe.push<TestEvent>(eventCount);
            pipe.allocate<char>(getEventLoop().getEventFillerSize());
        }
    }
};

class TestPongEventActor : public TestPingPongEventActor
{
  public:
    struct Init
    {
        unsigned eventBatchSize;
        ActorId &pongActorId;
        inline Init(unsigned peventBatchSize, ActorId &ppongActorId) noexcept : eventBatchSize(peventBatchSize),
                                                                                pongActorId(ppongActorId)
        {
        }
    };

    inline TestPongEventActor(const Init &init) : TestPingPongEventActor(init.eventBatchSize)
    {
        init.pongActorId = *this;
        registerEventHandler<TestEvent>(*this);
    }
};

void testCoreToCoreEvent()
{
    for (unsigned i = 1; i <= 100; ++i)
    {
        // std::cout << "testCoreToCoreEvent(), i=" << i << std::endl;
        Actor::ActorId pongActorId;
        TestEventLoopFactory<TestLocalEventLoop> eventLoopFactory;
        TestStartSequence<TestEventLoopFactory<TestLocalEventLoop>> startSequence(
            eventLoopFactory, eventLoopFactory.eventAllocatorPageSizeByte);
        startSequence.addActor<TestPongEventActor>(0, TestPongEventActor::Init(i, pongActorId));
        startSequence.addActor<TestPingEventActor>(1, TestPingEventActor::Init(i, pongActorId));
        Engine engine(startSequence);
    }
}

class TestReturnToSenderEventLoop : public EngineEventLoop
{
  public:
    struct Counters
    {
        unsigned onReturnEventCount;
        unsigned onReturnedEventCount;
        unsigned onDeliveredEventCount;
        unsigned onUndeliveredEventCount;
        Counters()
            : onReturnEventCount(0), onReturnedEventCount(0), onDeliveredEventCount(0), onUndeliveredEventCount(0)
        {
        }
    };
    struct DeliveredEvent : Actor::Event
    {
        static void nameToOStream(std::ostream &s, const Event &)
        {
            s << "TestReturnToSenderEventLoop::DeliveredEvent";
        }
    };
    struct UndeliveredEvent : Actor::Event
    {
        static void nameToOStream(std::ostream &s, const Event &)
        {
            s << "TestReturnToSenderEventLoop::UndeliveredEvent";
        }
    };
    struct ReceiverActor : Actor
    {
        bool doneFlag;
        ReceiverActor(ActorId *actorId) : doneFlag(false)
        {
            *actorId = *this;
            registerEventHandler<DeliveredEvent>(*this);
            registerEventHandler<UndeliveredEvent>(*this);
        }
        void onEvent(const DeliveredEvent &)
        {
            ASSERT_TRUE(dynamic_cast<TestReturnToSenderEventLoop *>(&getEventLoop()) != 0);
            ++static_cast<TestReturnToSenderEventLoop &>(getEventLoop()).counters.onDeliveredEventCount;
            doneFlag = true;
        }
        void onEvent(const UndeliveredEvent &event)
        {
            ASSERT_TRUE(dynamic_cast<TestReturnToSenderEventLoop *>(&getEventLoop()) != 0);
            TestReturnToSenderEventLoop &eventLoop = static_cast<TestReturnToSenderEventLoop &>(getEventLoop());
            eventLoop.eventList.push_back(&event);
            ++eventLoop.counters.onReturnEventCount;
            doneFlag = true;
        }
        virtual void onDestroyRequest() noexcept
        {
            if (doneFlag)
            {
                Actor::onDestroyRequest();
            }
            else
            {
                requestDestroy();
            }
        }
    };
    template <class _PushOperator, unsigned _deliveredEventCount, unsigned _undeliveredEventCount>
    struct SenderActor : Actor
    {
        bool doneFlag;
        SenderActor(const ActorId *receiverActorId) : doneFlag(_undeliveredEventCount == 0)
        {
            registerUndeliveredEventHandler<UndeliveredEvent>(*this);
            Event::Pipe pipe(*this, *receiverActorId);
            _PushOperator push(pipe, _deliveredEventCount, _undeliveredEventCount);
        }
        void onUndeliveredEvent(const UndeliveredEvent &)
        {

            // std::cout << "TestReturnToSenderEventLoop::SenderActor::onUndeliveredEvent(" << event << ')' <<
            // std::endl;

            ASSERT_TRUE(dynamic_cast<TestReturnToSenderEventLoop *>(&getEventLoop()) != 0);
            ++static_cast<TestReturnToSenderEventLoop &>(getEventLoop()).counters.onUndeliveredEventCount;
            doneFlag = true;
        }
        virtual void onDestroyRequest() noexcept
        {
            if (doneFlag)
            {
                Actor::onDestroyRequest();
            }
            else
            {
                requestDestroy();
            }
        }
    };

    virtual ~TestReturnToSenderEventLoop() noexcept
    {
        Mutex::Lock lock(staticMutex);
        bool createdFlag = staticCoreIdCoutersMap.insert(CoreIdCoutersMap::value_type(getCoreId(), counters)).second;
        EXPECT_TRUE(createdFlag);
    }
    static Counters getCounters(Actor::CoreId coreId)
    {
        Mutex::Lock lock(staticMutex);
        CoreIdCoutersMap::iterator i = staticCoreIdCoutersMap.find(coreId);
        return i == staticCoreIdCoutersMap.end() ? Counters() : i->second;
    }
    static void clearCounters()
    {
        Mutex::Lock lock(staticMutex);
        staticCoreIdCoutersMap.clear();
    }

  protected:
    virtual void run() noexcept
    {
        while (isRunning())
        {
            do
            {
                for (; !eventList.empty(); eventList.pop_front(), ++counters.onReturnedEventCount)
                {

                    // std::cout << "TestReturnToSenderEventLoop::run(), returnToSender(" << *eventList.front() <<
                    // std::endl;

                    returnToSender(*eventList.front());
                }
                synchronize();
            } while (!isInterrupted());
        }
    }

  private:
    static Mutex staticMutex;
    typedef std::map<Actor::CoreId, Counters> CoreIdCoutersMap;
    static CoreIdCoutersMap staticCoreIdCoutersMap;
    typedef std::list<const UndeliveredEvent *> EventList;
    EventList eventList;
    Counters counters;
};

Mutex TestReturnToSenderEventLoop::staticMutex;
TestReturnToSenderEventLoop::CoreIdCoutersMap TestReturnToSenderEventLoop::staticCoreIdCoutersMap;

struct TestReturnToSenderEventLoopFactory : EngineCustomEventLoopFactory, TestAsyncExceptionHandler
{
    virtual EventLoopAutoPointer newEventLoop()
    {
        return EventLoopAutoPointer::newEventLoop<TestReturnToSenderEventLoop>();
    }
};

struct TestReturnToSenderContiguous1PushOperator
{
    TestReturnToSenderContiguous1PushOperator(Actor::Event::Pipe &pipe, unsigned deliveredEventCount,
                                              unsigned undeliveredEventCount)
    {
        for (unsigned i = 0; i < deliveredEventCount; ++i)
        {
            pipe.push<TestReturnToSenderEventLoop::DeliveredEvent>();
        }
        for (unsigned i = 0; i < undeliveredEventCount; ++i)
        {
            pipe.push<TestReturnToSenderEventLoop::UndeliveredEvent>();
        }
    }
};

struct TestReturnToSenderContiguous2PushOperator
{
    TestReturnToSenderContiguous2PushOperator(Actor::Event::Pipe &pipe, unsigned deliveredEventCount,
                                              unsigned undeliveredEventCount)
    {
        for (unsigned i = 0; i < undeliveredEventCount; ++i)
        {
            pipe.push<TestReturnToSenderEventLoop::UndeliveredEvent>();
        }
        for (unsigned i = 0; i < deliveredEventCount; ++i)
        {
            pipe.push<TestReturnToSenderEventLoop::DeliveredEvent>();
        }
    }
};

struct TestReturnToSenderInterlaced1PushOperator
{
    TestReturnToSenderInterlaced1PushOperator(Actor::Event::Pipe &pipe, unsigned deliveredEventCount,
                                              unsigned undeliveredEventCount)
    {
        for (unsigned d = 0, u = 0; d < deliveredEventCount || u < undeliveredEventCount; ++d, ++u)
        {
            if (d < deliveredEventCount)
            {
                pipe.push<TestReturnToSenderEventLoop::DeliveredEvent>();
            }
            if (u < undeliveredEventCount)
            {
                pipe.push<TestReturnToSenderEventLoop::UndeliveredEvent>();
            }
        }
    }
};

struct TestReturnToSenderInterlaced2PushOperator
{
    TestReturnToSenderInterlaced2PushOperator(Actor::Event::Pipe &pipe, unsigned deliveredEventCount,
                                              unsigned undeliveredEventCount)
    {
        for (unsigned d = 0, u = 0; d < deliveredEventCount || u < undeliveredEventCount; ++d, ++u)
        {
            if (u < undeliveredEventCount)
            {
                pipe.push<TestReturnToSenderEventLoop::UndeliveredEvent>();
            }
            if (d < deliveredEventCount)
            {
                pipe.push<TestReturnToSenderEventLoop::DeliveredEvent>();
            }
        }
    }
};

template <class _PushOperator, unsigned _deliveredEventCount, unsigned _undeliveredEventCount>
void testReturnToSender(bool sameCoreFlag)
{
    static const Actor::CoreId FIRST_CORE_ID = 0;
    static const Actor::CoreId SECOND_CORE_ID = 1;

    TestReturnToSenderEventLoop::clearCounters();
    TestReturnToSenderEventLoopFactory eventLoopFactory;
    Actor::ActorId receiverActorId;
    {
        TestStartSequence<TestReturnToSenderEventLoopFactory> startSequence(eventLoopFactory, 4096);
        startSequence.addActor<TestReturnToSenderEventLoop::ReceiverActor>(FIRST_CORE_ID, &receiverActorId);
        startSequence.addActor<
            TestReturnToSenderEventLoop::SenderActor<_PushOperator, _deliveredEventCount, _undeliveredEventCount>>(
            sameCoreFlag ? FIRST_CORE_ID : SECOND_CORE_ID, &receiverActorId);
        Engine engine(startSequence);
    }

    if (sameCoreFlag)
    {
        const TestReturnToSenderEventLoop::Counters &counters = TestReturnToSenderEventLoop::getCounters(FIRST_CORE_ID);

        // std::cout << "counters[" << (int)FIRST_CORE_ID << "]=" << counters << std::endl;

        ASSERT_EQ(counters.onDeliveredEventCount, _deliveredEventCount);
        ASSERT_EQ(counters.onUndeliveredEventCount, _undeliveredEventCount);
        ASSERT_EQ(counters.onReturnEventCount, _undeliveredEventCount);
        ASSERT_EQ(counters.onReturnedEventCount, _undeliveredEventCount);
    }
    else
    {
        const TestReturnToSenderEventLoop::Counters &countersRecv =
            TestReturnToSenderEventLoop::getCounters(FIRST_CORE_ID);
        const TestReturnToSenderEventLoop::Counters &countersSend =
            TestReturnToSenderEventLoop::getCounters(SECOND_CORE_ID);

        // std::cout << "counters[send]=" << countersSend << std::endl;
        // std::cout << "counters[recv]=" << countersRecv << std::endl;

        ASSERT_EQ(0u, countersSend.onDeliveredEventCount);
        ASSERT_EQ(countersRecv.onDeliveredEventCount, _deliveredEventCount);
        ASSERT_EQ(countersSend.onUndeliveredEventCount, _undeliveredEventCount);
        ASSERT_EQ(0u, countersRecv.onUndeliveredEventCount);
        ASSERT_EQ(0u, countersSend.onReturnEventCount);
        ASSERT_EQ(countersRecv.onReturnEventCount, _undeliveredEventCount);
        ASSERT_EQ(0u, countersSend.onReturnedEventCount);
        ASSERT_EQ(countersRecv.onReturnedEventCount, _undeliveredEventCount);
    }
}

template <unsigned _deliveredEventCount, unsigned _undeliveredEventCount> void testReturnToSender()
{
    // std::cout << "testReturnToSender(), deliveredEventCount=" << _deliveredEventCount<< ", undeliveredEventCount=" <<
    // _undeliveredEventCount << std::endl;

    // std::cout << "TestReturnToSenderContiguous1PushOperator, same core" << std::endl;
    testReturnToSender<TestReturnToSenderContiguous1PushOperator, _deliveredEventCount, _undeliveredEventCount>(true);
    // std::cout << "TestReturnToSenderContiguous1PushOperator, different cores" << std::endl;
    testReturnToSender<TestReturnToSenderContiguous1PushOperator, _deliveredEventCount, _undeliveredEventCount>(false);

    // std::cout << "TestReturnToSenderContiguous2PushOperator, same core" << std::endl;
    testReturnToSender<TestReturnToSenderContiguous2PushOperator, _deliveredEventCount, _undeliveredEventCount>(true);
    // std::cout << "TestReturnToSenderContiguous2PushOperator, different cores" << std::endl;
    testReturnToSender<TestReturnToSenderContiguous2PushOperator, _deliveredEventCount, _undeliveredEventCount>(false);

    // std::cout << "TestReturnToSenderInterlaced1PushOperator, same core" << std::endl;
    testReturnToSender<TestReturnToSenderInterlaced1PushOperator, _deliveredEventCount, _undeliveredEventCount>(true);
    // std::cout << "TestReturnToSenderInterlaced1PushOperator, different cores" << std::endl;
    testReturnToSender<TestReturnToSenderInterlaced1PushOperator, _deliveredEventCount, _undeliveredEventCount>(false);

    // std::cout << "TestReturnToSenderInterlaced2PushOperator, same core" << std::endl;
    testReturnToSender<TestReturnToSenderInterlaced2PushOperator, _deliveredEventCount, _undeliveredEventCount>(true);
    // std::cout << "TestReturnToSenderInterlaced2PushOperator, different cores" << std::endl;
    testReturnToSender<TestReturnToSenderInterlaced2PushOperator, _deliveredEventCount, _undeliveredEventCount>(false);
}

void testReturnToSender()
{
    testReturnToSender<0u, 1u>();
    testReturnToSender<1u, 0u>();
    testReturnToSender<0u, 2u>();
    testReturnToSender<2u, 0u>();
    testReturnToSender<1u, 1u>();
    testReturnToSender<1u, 2u>();
    testReturnToSender<2u, 1u>();
    testReturnToSender<1u, 3u>();
    testReturnToSender<3u, 1u>();
    testReturnToSender<2u, 3u>();
    testReturnToSender<3u, 2u>();
    testReturnToSender<3u, 3u>();
    testReturnToSender<3u, 3u>();
    testReturnToSender<4u, 3u>();
    testReturnToSender<3u, 4u>();
}

} // namespace anonymous

TEST(AsyncEventLoop, init) { testInit(); }
TEST(AsyncEventLoop, localEvent) { testLocalEvent(); }
TEST(AsyncEventLoop, coreToCoreEvent) { testCoreToCoreEvent(); }
TEST(AsyncEventLoop, returnToSender) { testReturnToSender(); }