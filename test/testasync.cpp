/**
 * @file testasync.cpp
 * @brief test core Simplx engine
 * @copyright 2013-2019 Tredzone (www.tredzone.com). All rights reserved.
 * Please see accompanying LICENSE file for licensing terms.
 */

#include "gtest/gtest.h"

#include "testutil.h"

#include "trz/engine/internal/node.h"

struct TestEventLoop;

namespace tredzone
{
template <>
struct tredzone::EngineCustomEventLoopFactory::EventLoopAutoPointerNew<TestEventLoop>
    : tredzone::EngineCustomEventLoopFactory::DefaultEventLoop
{
    EventLoopAutoPointerNew(AsyncNode &asyncNode)
    {
        asyncNode.stop();
        init(asyncNode);
        preRun();
        run();
        postRun();
    }
};

} // namespace tredzone

struct TestEventLoop : tredzone::EngineCustomEventLoopFactory::EventLoopAutoPointerNew<TestEventLoop>
{
    TestEventLoop(tredzone::AsyncNode &asyncNode) : EventLoopAutoPointerNew(asyncNode) {}
};

struct TestCoreSet : tredzone::Engine::CoreSet
{
    TestCoreSet()
    {
        for (int i = 0, coreCount = std::min(16, (int)tredzone::cpuGetCount()); i < coreCount; ++i)
        {
            set((tredzone::Engine::CoreId)i);
        }
    }
};

class TestInitActor : public tredzone::Actor
{
public:
    struct Exception : std::exception
    {
    };

    TestInitActor(bool throwException = false) : m_LocalDestroyFlag(false)
    {
        std::cout << "TestInitActor::TestInitActor()" << std::endl;
        if (throwException)
        {
            throw Exception();
        }
    }
    virtual ~TestInitActor() noexcept
    {
        std::cout << "TestInitActor::~TestInitActor()" << std::endl;
        requestDestroy();
        EXPECT_TRUE(m_LocalDestroyFlag);
    }

protected:
    virtual void onDestroyRequest() noexcept
    {
        std::cout << "TestInitActor::onDestroyRequest()" << std::endl;
        requestDestroy();
        ASSERT_FALSE(m_LocalDestroyFlag);
        m_LocalDestroyFlag = true;
        tredzone::Actor::onDestroyRequest();
    }

private:
    bool m_LocalDestroyFlag;
};

static void testInit()
{
    tredzone::EngineCustomEventLoopFactory customEventLoopFactory;
    tredzone::AsyncNodeManager nodeManager(1024, TestCoreSet());
    tredzone::AsyncNode node(tredzone::AsyncNode::Init(nodeManager, 0, customEventLoopFactory));
    {
        TestInitActor &handler1 = node.newActor<TestInitActor>(0);
        tredzone::Actor::ActorId handler1Id = handler1.getActorId();
        ASSERT_EQ(handler1Id.getCoreIndex(), node.id);
        ASSERT_EQ(1u, handler1Id.getNodeActorId());
        ASSERT_EQ(handler1Id, handler1Id);
        tredzone::Actor::ActorId handler2Id = handler1.newUnreferencedActor<TestInitActor>(0);
        ASSERT_EQ(handler2Id.getCoreIndex(), node.id);
        ASSERT_EQ(2u, handler2Id.getNodeActorId());
        ASSERT_NE(handler1Id, handler2Id);

        ASSERT_THROW(handler1.newUnreferencedActor<TestInitActor>(true), TestInitActor::Exception);
        ASSERT_THROW(handler1.newReferencedActor<TestInitActor>(true), TestInitActor::Exception);
        ASSERT_THROW(handler1.newReferencedSingletonActor<TestInitActor>(true), TestInitActor::Exception);
    }
    TestEventLoop testEventLoop(node);
}

struct TestInitRegisterEventHandler : tredzone::Actor
{
    template <int> struct TestEvent : Event
    {
    };
    template <int _i> struct TestEventHandler
    {
        typedef TestEvent<_i> event_type;

        void onEvent(const event_type &) {}
        void onUndeliveredEvent(const event_type &) {}
    };

    TestEventHandler<0> testEventHandler0;
    TestEventHandler<1> testEventHandler1;
    TestEventHandler<2> testEventHandler2;
    TestEventHandler<3> testEventHandler3;
    TestEventHandler<4> testEventHandler4;
    TestEventHandler<5> testEventHandler5;
    TestEventHandler<6> testEventHandler6;
    TestEventHandler<7> testEventHandler7;
    TestEventHandler<8> testEventHandler8;
    TestEventHandler<9> testEventHandler9;
    TestEventHandler<10> testEventHandler10;
    TestEventHandler<11> testEventHandler11;
    TestEventHandler<12> testEventHandler12;
    TestEventHandler<13> testEventHandler13;
    TestEventHandler<14> testEventHandler14;

    TestInitRegisterEventHandler()
    {
        EXPECT_FALSE(isRegisteredTestEventHandler(testEventHandler0));
        registerTestEventHandler(testEventHandler0);
        EXPECT_THROW(registerTestEventHandler(testEventHandler0), AlreadyRegisterdEventHandlerException);
        EXPECT_TRUE(isRegisteredTestEventHandler(testEventHandler0));

        EXPECT_FALSE(isRegisteredTestEventHandler(testEventHandler1));
        registerTestEventHandler(testEventHandler1);
        EXPECT_THROW(registerTestEventHandler(testEventHandler1), AlreadyRegisterdEventHandlerException);
        EXPECT_TRUE(isRegisteredTestEventHandler(testEventHandler1));

        EXPECT_FALSE(isRegisteredTestEventHandler(testEventHandler2));
        registerTestEventHandler(testEventHandler2);
        EXPECT_THROW(registerTestEventHandler(testEventHandler2), AlreadyRegisterdEventHandlerException);
        EXPECT_TRUE(isRegisteredTestEventHandler(testEventHandler2));

        EXPECT_FALSE(isRegisteredTestEventHandler(testEventHandler3));
        registerTestEventHandler(testEventHandler3);
        EXPECT_THROW(registerTestEventHandler(testEventHandler3), AlreadyRegisterdEventHandlerException);
        EXPECT_TRUE(isRegisteredTestEventHandler(testEventHandler3));

        EXPECT_FALSE(isRegisteredTestEventHandler(testEventHandler4));
        registerTestEventHandler(testEventHandler4);
        EXPECT_THROW(registerTestEventHandler(testEventHandler4), AlreadyRegisterdEventHandlerException);
        EXPECT_TRUE(isRegisteredTestEventHandler(testEventHandler4));

        EXPECT_FALSE(isRegisteredTestEventHandler(testEventHandler5));
        registerTestEventHandler(testEventHandler5);
        EXPECT_THROW(registerTestEventHandler(testEventHandler5), AlreadyRegisterdEventHandlerException);
        EXPECT_TRUE(isRegisteredTestEventHandler(testEventHandler5));

        EXPECT_FALSE(isRegisteredTestEventHandler(testEventHandler6));
        registerTestEventHandler(testEventHandler6);
        EXPECT_THROW(registerTestEventHandler(testEventHandler6), AlreadyRegisterdEventHandlerException);
        EXPECT_TRUE(isRegisteredTestEventHandler(testEventHandler6));

        EXPECT_FALSE(isRegisteredTestEventHandler(testEventHandler7));
        registerTestEventHandler(testEventHandler7);
        EXPECT_THROW(registerTestEventHandler(testEventHandler7), AlreadyRegisterdEventHandlerException);
        EXPECT_TRUE(isRegisteredTestEventHandler(testEventHandler7));

        EXPECT_FALSE(isRegisteredTestEventHandler(testEventHandler8));
        registerTestEventHandler(testEventHandler8);
        EXPECT_THROW(registerTestEventHandler(testEventHandler8), AlreadyRegisterdEventHandlerException);
        EXPECT_TRUE(isRegisteredTestEventHandler(testEventHandler8));

        EXPECT_FALSE(isRegisteredTestEventHandler(testEventHandler9));
        registerTestEventHandler(testEventHandler9);
        EXPECT_THROW(registerTestEventHandler(testEventHandler9), AlreadyRegisterdEventHandlerException);
        EXPECT_TRUE(isRegisteredTestEventHandler(testEventHandler9));

        EXPECT_FALSE(isRegisteredTestEventHandler(testEventHandler10));
        registerTestEventHandler(testEventHandler10);
        EXPECT_THROW(registerTestEventHandler(testEventHandler10), AlreadyRegisterdEventHandlerException);
        EXPECT_TRUE(isRegisteredTestEventHandler(testEventHandler10));

        EXPECT_FALSE(isRegisteredTestEventHandler(testEventHandler11));
        registerTestEventHandler(testEventHandler11);
        EXPECT_THROW(registerTestEventHandler(testEventHandler11), AlreadyRegisterdEventHandlerException);
        EXPECT_TRUE(isRegisteredTestEventHandler(testEventHandler11));

        EXPECT_FALSE(isRegisteredTestEventHandler(testEventHandler12));
        registerTestEventHandler(testEventHandler12);
        EXPECT_THROW(registerTestEventHandler(testEventHandler12), AlreadyRegisterdEventHandlerException);
        EXPECT_TRUE(isRegisteredTestEventHandler(testEventHandler12));

        EXPECT_FALSE(isRegisteredTestEventHandler(testEventHandler13));
        registerTestEventHandler(testEventHandler13);
        EXPECT_THROW(registerTestEventHandler(testEventHandler13), AlreadyRegisterdEventHandlerException);
        EXPECT_TRUE(isRegisteredTestEventHandler(testEventHandler13));

        EXPECT_FALSE(isRegisteredTestEventHandler(testEventHandler14));
        registerTestEventHandler(testEventHandler14);
        EXPECT_THROW(registerTestEventHandler(testEventHandler14), AlreadyRegisterdEventHandlerException);
        EXPECT_TRUE(isRegisteredTestEventHandler(testEventHandler14));

        EXPECT_FALSE(isRegisteredUndeliveredTestEventHandler(testEventHandler0));
        registerUndeliveredTestEventHandler(testEventHandler0);
        EXPECT_THROW(registerUndeliveredTestEventHandler(testEventHandler0), AlreadyRegisterdEventHandlerException);
        EXPECT_TRUE(isRegisteredUndeliveredTestEventHandler(testEventHandler0));

        EXPECT_FALSE(isRegisteredUndeliveredTestEventHandler(testEventHandler1));
        registerUndeliveredTestEventHandler(testEventHandler1);
        EXPECT_THROW(registerUndeliveredTestEventHandler(testEventHandler1), AlreadyRegisterdEventHandlerException);
        EXPECT_TRUE(isRegisteredUndeliveredTestEventHandler(testEventHandler1));

        EXPECT_FALSE(isRegisteredUndeliveredTestEventHandler(testEventHandler2));
        registerUndeliveredTestEventHandler(testEventHandler2);
        EXPECT_THROW(registerUndeliveredTestEventHandler(testEventHandler2), AlreadyRegisterdEventHandlerException);
        EXPECT_TRUE(isRegisteredUndeliveredTestEventHandler(testEventHandler2));

        EXPECT_FALSE(isRegisteredUndeliveredTestEventHandler(testEventHandler3));
        registerUndeliveredTestEventHandler(testEventHandler3);
        EXPECT_THROW(registerUndeliveredTestEventHandler(testEventHandler3), AlreadyRegisterdEventHandlerException);
        EXPECT_TRUE(isRegisteredUndeliveredTestEventHandler(testEventHandler3));

        EXPECT_FALSE(isRegisteredUndeliveredTestEventHandler(testEventHandler4));
        registerUndeliveredTestEventHandler(testEventHandler4);
        EXPECT_THROW(registerUndeliveredTestEventHandler(testEventHandler4), AlreadyRegisterdEventHandlerException);
        EXPECT_TRUE(isRegisteredUndeliveredTestEventHandler(testEventHandler4));

        EXPECT_FALSE(isRegisteredUndeliveredTestEventHandler(testEventHandler5));
        registerUndeliveredTestEventHandler(testEventHandler5);
        EXPECT_THROW(registerUndeliveredTestEventHandler(testEventHandler5), AlreadyRegisterdEventHandlerException);
        EXPECT_TRUE(isRegisteredUndeliveredTestEventHandler(testEventHandler5));

        EXPECT_FALSE(isRegisteredUndeliveredTestEventHandler(testEventHandler6));
        registerUndeliveredTestEventHandler(testEventHandler6);
        EXPECT_THROW(registerUndeliveredTestEventHandler(testEventHandler6), AlreadyRegisterdEventHandlerException);
        EXPECT_TRUE(isRegisteredUndeliveredTestEventHandler(testEventHandler6));

        EXPECT_FALSE(isRegisteredUndeliveredTestEventHandler(testEventHandler7));
        registerUndeliveredTestEventHandler(testEventHandler7);
        EXPECT_THROW(registerUndeliveredTestEventHandler(testEventHandler7), AlreadyRegisterdEventHandlerException);
        EXPECT_TRUE(isRegisteredUndeliveredTestEventHandler(testEventHandler7));

        EXPECT_FALSE(isRegisteredUndeliveredTestEventHandler(testEventHandler8));
        registerUndeliveredTestEventHandler(testEventHandler8);
        EXPECT_THROW(registerUndeliveredTestEventHandler(testEventHandler8), AlreadyRegisterdEventHandlerException);
        EXPECT_TRUE(isRegisteredUndeliveredTestEventHandler(testEventHandler8));

        EXPECT_FALSE(isRegisteredUndeliveredTestEventHandler(testEventHandler9));
        registerUndeliveredTestEventHandler(testEventHandler9);
        EXPECT_THROW(registerUndeliveredTestEventHandler(testEventHandler9), AlreadyRegisterdEventHandlerException);
        EXPECT_TRUE(isRegisteredUndeliveredTestEventHandler(testEventHandler9));

        EXPECT_FALSE(isRegisteredUndeliveredTestEventHandler(testEventHandler10));
        registerUndeliveredTestEventHandler(testEventHandler10);
        EXPECT_THROW(registerUndeliveredTestEventHandler(testEventHandler10), AlreadyRegisterdEventHandlerException);
        EXPECT_TRUE(isRegisteredUndeliveredTestEventHandler(testEventHandler10));

        EXPECT_FALSE(isRegisteredUndeliveredTestEventHandler(testEventHandler11));
        registerUndeliveredTestEventHandler(testEventHandler11);
        EXPECT_THROW(registerUndeliveredTestEventHandler(testEventHandler11), AlreadyRegisterdEventHandlerException);
        EXPECT_TRUE(isRegisteredUndeliveredTestEventHandler(testEventHandler11));

        EXPECT_FALSE(isRegisteredUndeliveredTestEventHandler(testEventHandler12));
        registerUndeliveredTestEventHandler(testEventHandler12);
        EXPECT_THROW(registerUndeliveredTestEventHandler(testEventHandler12), AlreadyRegisterdEventHandlerException);
        EXPECT_TRUE(isRegisteredUndeliveredTestEventHandler(testEventHandler12));

        EXPECT_FALSE(isRegisteredUndeliveredTestEventHandler(testEventHandler13));
        registerUndeliveredTestEventHandler(testEventHandler13);
        EXPECT_THROW(registerUndeliveredTestEventHandler(testEventHandler13), AlreadyRegisterdEventHandlerException);
        EXPECT_TRUE(isRegisteredUndeliveredTestEventHandler(testEventHandler13));

        EXPECT_FALSE(isRegisteredUndeliveredTestEventHandler(testEventHandler14));
        registerUndeliveredTestEventHandler(testEventHandler14);
        EXPECT_THROW(registerUndeliveredTestEventHandler(testEventHandler14), AlreadyRegisterdEventHandlerException);
        EXPECT_TRUE(isRegisteredUndeliveredTestEventHandler(testEventHandler14));

        unregisterAllEventHandlers();
        EXPECT_FALSE(isRegisteredTestEventHandler(testEventHandler0));
        EXPECT_FALSE(isRegisteredTestEventHandler(testEventHandler1));
        EXPECT_FALSE(isRegisteredTestEventHandler(testEventHandler2));
        EXPECT_FALSE(isRegisteredTestEventHandler(testEventHandler3));
        EXPECT_FALSE(isRegisteredTestEventHandler(testEventHandler4));
        EXPECT_FALSE(isRegisteredTestEventHandler(testEventHandler5));
        EXPECT_FALSE(isRegisteredTestEventHandler(testEventHandler6));
        EXPECT_FALSE(isRegisteredTestEventHandler(testEventHandler7));
        EXPECT_FALSE(isRegisteredTestEventHandler(testEventHandler8));
        EXPECT_FALSE(isRegisteredTestEventHandler(testEventHandler9));
        EXPECT_FALSE(isRegisteredTestEventHandler(testEventHandler10));
        EXPECT_FALSE(isRegisteredTestEventHandler(testEventHandler11));
        EXPECT_FALSE(isRegisteredTestEventHandler(testEventHandler12));
        EXPECT_FALSE(isRegisteredTestEventHandler(testEventHandler13));
        EXPECT_FALSE(isRegisteredTestEventHandler(testEventHandler14));
        EXPECT_FALSE(isRegisteredUndeliveredTestEventHandler(testEventHandler0));
        EXPECT_FALSE(isRegisteredUndeliveredTestEventHandler(testEventHandler1));
        EXPECT_FALSE(isRegisteredUndeliveredTestEventHandler(testEventHandler2));
        EXPECT_FALSE(isRegisteredUndeliveredTestEventHandler(testEventHandler3));
        EXPECT_FALSE(isRegisteredUndeliveredTestEventHandler(testEventHandler4));
        EXPECT_FALSE(isRegisteredUndeliveredTestEventHandler(testEventHandler5));
        EXPECT_FALSE(isRegisteredUndeliveredTestEventHandler(testEventHandler6));
        EXPECT_FALSE(isRegisteredUndeliveredTestEventHandler(testEventHandler7));
        EXPECT_FALSE(isRegisteredUndeliveredTestEventHandler(testEventHandler8));
        EXPECT_FALSE(isRegisteredUndeliveredTestEventHandler(testEventHandler9));
        EXPECT_FALSE(isRegisteredUndeliveredTestEventHandler(testEventHandler10));
        EXPECT_FALSE(isRegisteredUndeliveredTestEventHandler(testEventHandler11));
        EXPECT_FALSE(isRegisteredUndeliveredTestEventHandler(testEventHandler12));
        EXPECT_FALSE(isRegisteredUndeliveredTestEventHandler(testEventHandler13));
        EXPECT_FALSE(isRegisteredUndeliveredTestEventHandler(testEventHandler14));
    }
    template <class _TestEventHandler> void registerTestEventHandler(_TestEventHandler &testEventHandler)
    {
        registerEventHandler<typename _TestEventHandler::event_type>(testEventHandler);
    }
    template <class _TestEventHandler> void registerUndeliveredTestEventHandler(_TestEventHandler &testEventHandler)
    {
        registerUndeliveredEventHandler<typename _TestEventHandler::event_type>(testEventHandler);
    }
    template <class _TestEventHandler> bool isRegisteredTestEventHandler(_TestEventHandler &)
    {
        return isRegisteredEventHandler<typename _TestEventHandler::event_type>();
    }
    template <class _TestEventHandler> bool isRegisteredUndeliveredTestEventHandler(_TestEventHandler &)
    {
        return isRegisteredUndeliveredEventHandler<typename _TestEventHandler::event_type>();
    }
};

void testInitRegisterEventHandler()
{
    tredzone::EngineCustomEventLoopFactory customEventLoopFactory;
    tredzone::AsyncNodeManager nodeManager(1024, TestCoreSet());
    tredzone::AsyncNode node(tredzone::AsyncNode::Init(nodeManager, 0, customEventLoopFactory));
    node.newActor<TestInitRegisterEventHandler>();
    TestEventLoop testEventLoop(node);
}

class TestUniNodeEvent : public TestInitActor
{
  public:
    struct Exception : std::exception
    {
    };
    struct Event1 : Event
    {
        static void nameToOStream(std::ostream &os, const Event &) { os << "TestUniNodeEvent::Event1"; }
    };
    struct Event2 : Event
    {
        static void nameToOStream(std::ostream &os, const Event &) { os << "TestUniNodeEvent::Event2"; }
        static void contentToOStream(std::ostream &, const Event &) {}
    };
    struct Event3 : Event
    {
        Event3(int = 0) { throw Exception(); }
    };

    TestUniNodeEvent(int = 0) : onEventCount(0), onUndeliveredEventCount(0), eventBatch(Event::Pipe(*this, *this))
    {
        registerEventHandler<Event>(*this);
        Event::Pipe(*this, getActorId()).push<Event>();
    }
    virtual ~TestUniNodeEvent() noexcept
    {
        EXPECT_EQ(2u, onEventCount);
        EXPECT_EQ(1u, onUndeliveredEventCount);
    }
    void onEvent(const Event &event)
    {
        std::cout << "TestUniNodeEvent::onEvent(" << event << ')' << std::endl;
        ++onEventCount;
        unregisterEventHandler<Event>();
        registerUndeliveredEventHandler<Event>(*this);
        Event::Pipe queue(*this, getActorId());
        queue.push<Event>();
        ASSERT_TRUE(eventBatch.isPushCommitted(queue));
        ASSERT_FALSE(eventBatch.isPushCommitted(queue));

        ASSERT_EQ(event.getSourceActorId(), event.getDestinationActorId());
    }
    void onEvent(const Event1 &event)
    {
        std::cout << "TestUniNodeEvent::onEvent(" << event << ')' << std::endl;
        ++onEventCount;
        requestDestroy();

        Event::Pipe queue = Event::Pipe(*this, *this);
        ASSERT_TRUE(eventBatch.isPushCommitted(queue));
        ASSERT_FALSE(eventBatch.isPushCommitted(queue));

        ASSERT_EQ(event.getSourceActorId(), event.getDestinationActorId());
    }
    void onUndeliveredEvent(const Event &event)
    {
        std::cout << "TestUniNodeEvent::onUndeliveredEvent(" << event << ')' << std::endl;
        ++onUndeliveredEventCount;
        registerEventHandler<Event1>(*this);
        Event::Pipe(*this, getActorId()).push<Event1>();

        Event::Pipe queue = Event::Pipe(*this, *this);
        ASSERT_TRUE(eventBatch.isPushCommitted(queue));
        ASSERT_FALSE(eventBatch.isPushCommitted(queue));

        ASSERT_EQ(event.getSourceActorId(), event.getDestinationActorId());
    }

  private:
    unsigned onEventCount;
    unsigned onUndeliveredEventCount;
    Event::Batch eventBatch;

    virtual void onDestroyRequest() noexcept
    {
        std::cout << "TestUniNodeEvent::onDestroyRequest()" << std::endl;
        if (onEventCount == 2 && onUndeliveredEventCount == 1)
        {
            TestInitActor::onDestroyRequest();
        }
    }
};

void testUniNodeEvent()
{
    tredzone::EngineCustomEventLoopFactory customEventLoopFactory;
    tredzone::AsyncNodeManager nodeManager(1024, TestCoreSet());
    tredzone::AsyncNode node(tredzone::AsyncNode::Init(nodeManager, 0, customEventLoopFactory));
    {
        TestUniNodeEvent &handler = node.newActor<TestUniNodeEvent>(0);
        tredzone::Actor::Event::Pipe eventPipe(handler, handler);

        ASSERT_THROW(eventPipe.push<TestUniNodeEvent::Event3>(), TestUniNodeEvent::Exception);
        ASSERT_THROW(eventPipe.push<TestUniNodeEvent::Event3>(0), TestUniNodeEvent::Exception);
        ASSERT_THROW(tredzone::Actor::Event::BufferedPipe(eventPipe).push<TestUniNodeEvent::Event3>(),
                     TestUniNodeEvent::Exception);
        ASSERT_THROW(tredzone::Actor::Event::BufferedPipe(handler, eventPipe.getDestinationActorId())
                         .push<TestUniNodeEvent::Event3>(0),
                     TestUniNodeEvent::Exception);
    }
    TestEventLoop testEventLoop(node);
}

class TestMultiNodeEvent : public TestInitActor
{
  public:
    struct HFEvent : Event
    {
        static void nameToOStream(std::ostream &os, const Event &) { os << "TestMultiNodeEvent::HFEvent"; }
        static void contentToOStream(std::ostream &os, const Event &event)
        {
            os << "source=" << event.getSourceActorId() << ", destination=" << event.getDestinationActorId();
        }
    };
    struct LFEvent : Event
    {
        static void nameToOStream(std::ostream &os, const Event &) { os << "TestMultiNodeEvent::LFEvent"; }
        static void contentToOStream(std::ostream &os, const Event &event)
        {
            os << "source=" << event.getSourceActorId() << ", destination=" << event.getDestinationActorId();
        }
    };
    struct UndeliveredEvent : Event
    {
        static void nameToOStream(std::ostream &os, const Event &) { os << "TestMultiNodeEvent::UndeliveredEvent"; }
        static void contentToOStream(std::ostream &os, const Event &event)
        {
            os << "source=" << event.getSourceActorId() << ", destination=" << event.getDestinationActorId();
        }
    };

    TestMultiNodeEvent(int) : hfCallback(*this), lfCallback(*this), undeliveredCallback(*this) {}
    unsigned getHFEventCount() { return hfCallback.eventCount; }
    unsigned getLFEventCount() { return lfCallback.eventCount; }
    unsigned getUndeliveredEventCount() { return undeliveredCallback.eventCount; }

  private:
    template <class _Event> struct HFRegistration
    {
        typedef _Event event_type;
        template <class _EventHandler>
        static void registerEventHandler(TestMultiNodeEvent &asyncActor, _EventHandler &eventHandler)
        {
            asyncActor.registerEventHandler<_Event, _EventHandler>(eventHandler);
        }
    };
    template <class _Event> struct LFRegistration
    {
        typedef _Event event_type;
        template <class _EventHandler>
        static void registerEventHandler(TestMultiNodeEvent &asyncActor, _EventHandler &eventHandler)
        {
            asyncActor.registerEventHandler<_Event, _EventHandler>(eventHandler);
        }
    };
    template <class _Event> struct UndeliveredRegistration
    {
        typedef _Event event_type;
        template <class _EventHandler>
        static void registerEventHandler(TestMultiNodeEvent &asyncActor, _EventHandler &eventHandler)
        {
            asyncActor.registerUndeliveredEventHandler<_Event, _EventHandler>(eventHandler);
        }
    };
    template <class _Registration> struct Callback
    {
        typedef typename _Registration::event_type event_type;
        typedef _Registration registration_type;
        Callback(TestMultiNodeEvent &phandler) : handler(phandler), eventCount(0) {}
        TestMultiNodeEvent &handler;
        unsigned eventCount;
    };
    struct HFCallback : Callback<HFRegistration<HFEvent>>
    {
        HFCallback(TestMultiNodeEvent &handler) : Callback<HFRegistration<HFEvent>>(handler) {}
        void onEvent(const event_type &event)
        {
            std::cout << '[' << handler.getActorId() << "] TestMultiNodeEvent::HFCallback::onEvent(" << event << ')'
                      << std::endl;
            ++eventCount;
            ASSERT_NE(event.getSourceActorId(), event.getDestinationActorId());
            ASSERT_EQ(event.getDestinationActorId(), handler.getActorId());
        }
    };
    struct LFCallback : Callback<LFRegistration<LFEvent>>
    {
        LFCallback(TestMultiNodeEvent &handler) : Callback<LFRegistration<LFEvent>>(handler) {}
        void onEvent(const event_type &event)
        {
            std::cout << '[' << handler.getActorId() << "] TestMultiNodeEvent::LFCallback::onEvent(" << event << ')'
                      << std::endl;
            ++eventCount;
            ASSERT_NE(event.getSourceActorId(), event.getDestinationActorId());
            ASSERT_EQ(event.getDestinationActorId(), handler.getActorId());
        }
    };
    struct UndeliveredCallback : Callback<UndeliveredRegistration<UndeliveredEvent>>
    {
        UndeliveredCallback(TestMultiNodeEvent &handler) : Callback<UndeliveredRegistration<UndeliveredEvent>>(handler)
        {
        }
        void onUndeliveredEvent(const event_type &event)
        {
            std::cout << '[' << handler.getActorId() << "] TestMultiNodeEvent::UndeliveredCallback::onUndeliveredEvent("
                      << event << ')' << std::endl;
            ++eventCount;
            ASSERT_NE(event.getSourceActorId(), event.getDestinationActorId());
            ASSERT_EQ(event.getSourceActorId(), handler.getActorId());
        }
    };
    template <class _Callback> struct RegisteredCallback : _Callback
    {
        RegisteredCallback(TestMultiNodeEvent &handler) : _Callback(handler)
        {
            typedef typename _Callback::registration_type registration_type;
            registration_type::registerEventHandler(handler, *this);
        }
        ~RegisteredCallback() noexcept
        {
            this->_Callback::handler.template unregisterEventHandler<typename _Callback::event_type>();
        }
    };

    RegisteredCallback<HFCallback> hfCallback;
    RegisteredCallback<LFCallback> lfCallback;
    RegisteredCallback<UndeliveredCallback> undeliveredCallback;
};

void testMultinodeEvent()
{
    tredzone::EngineCustomEventLoopFactory customEventLoopFactory;
    tredzone::AsyncNodeManager nodeManager(1024, TestCoreSet());
    tredzone::AsyncNode node1(tredzone::AsyncNode::Init(nodeManager, 0, customEventLoopFactory));
    tredzone::AsyncNode node2(tredzone::AsyncNode::Init(nodeManager, 1, customEventLoopFactory));
    {
        TestMultiNodeEvent &handler1 = node1.newActor<TestMultiNodeEvent>(0);
        TestMultiNodeEvent &handler2 = node2.newActor<TestMultiNodeEvent>(0);
        ASSERT_EQ(0u, handler1.getHFEventCount());
        ASSERT_EQ(0u, handler1.getLFEventCount());
        ASSERT_EQ(0u, handler1.getUndeliveredEventCount());
        ASSERT_EQ(0u, handler2.getHFEventCount());
        ASSERT_EQ(0u, handler2.getLFEventCount());
        ASSERT_EQ(0u, handler2.getUndeliveredEventCount());
        tredzone::Actor::Event::Pipe(handler1, handler2).push<TestMultiNodeEvent::HFEvent>();
        node1.synchronize();
        node2.synchronize();
        ASSERT_EQ(0u, handler1.getHFEventCount());
        ASSERT_EQ(0u, handler1.getLFEventCount());
        ASSERT_EQ(0u, handler1.getUndeliveredEventCount());
        ASSERT_EQ(1u, handler2.getHFEventCount());
        ASSERT_EQ(0u, handler2.getLFEventCount());
        ASSERT_EQ(0u, handler2.getUndeliveredEventCount());
        tredzone::Actor::Event::Pipe(handler1, handler2).push<TestMultiNodeEvent::LFEvent>();
        node1.synchronize();
        node2.synchronize();
        ASSERT_EQ(0u, handler1.getHFEventCount());
        ASSERT_EQ(0u, handler1.getLFEventCount());
        ASSERT_EQ(0u, handler1.getUndeliveredEventCount());
        ASSERT_EQ(1u, handler2.getHFEventCount());
        ASSERT_EQ(1u, handler2.getLFEventCount());
        ASSERT_EQ(0u, handler2.getUndeliveredEventCount());
        tredzone::Actor::Event::Pipe(handler1, handler2).push<TestMultiNodeEvent::UndeliveredEvent>();
        node1.synchronize();
        node2.synchronize();
        node1.synchronize();
        ASSERT_EQ(0u, handler1.getHFEventCount());
        ASSERT_EQ(0u, handler1.getLFEventCount());
        ASSERT_EQ(1u, handler1.getUndeliveredEventCount());
        ASSERT_EQ(1u, handler2.getHFEventCount());
        ASSERT_EQ(1u, handler2.getLFEventCount());
        ASSERT_EQ(0u, handler2.getUndeliveredEventCount());
    }
    TestEventLoop testEventLoop2(node2);
    TestEventLoop testEventLoop1(node1);
}

class TestActorReference : public TestInitActor
{
  public:
    TestActorReference(bool *pdestroyedFlag) : destroyedFlag(*pdestroyedFlag) { EXPECT_FALSE(destroyedFlag); }
    virtual ~TestActorReference() noexcept
    {
        EXPECT_FALSE(destroyedFlag);
        destroyedFlag = true;
    }
    bool getDestroyedFlag() const { return destroyedFlag; }

  private:
    bool &destroyedFlag;
};

static void testActorReference()
{
    tredzone::EngineCustomEventLoopFactory customEventLoopFactory;
    tredzone::AsyncNodeManager nodeManager(1024, TestCoreSet());
    tredzone::AsyncNode node(tredzone::AsyncNode::Init(nodeManager, 0, customEventLoopFactory));
    {
        bool destroyedFlag = false;
        {
            TestInitActor &handler1 = node.newActor<TestInitActor>(0);
            tredzone::Actor::ActorReference<TestActorReference> handler2 =
                handler1.newReferencedActor<TestActorReference>(&destroyedFlag);
            ASSERT_FALSE(handler2->getDestroyedFlag());
            node.synchronize();
            ASSERT_FALSE((*handler2).getDestroyedFlag());
        }
        ASSERT_FALSE(destroyedFlag);
        node.synchronize();
        ASSERT_TRUE(destroyedFlag);
    }
    TestEventLoop testEventLoop(node);
}

namespace TestStaticActorReference
{
struct SelfReferenceActor : tredzone::Actor
{
    SelfReferenceActor(int) { newReferencedSingletonActor<SelfReferenceActor>(0); }
};
struct ReferenceStaticTestInitActor : tredzone::Actor
{
    tredzone::Actor::ActorReference<TestInitActor> ref;
    ReferenceStaticTestInitActor(int) : ref(newReferencedSingletonActor<TestInitActor>(0)) {}
};
}

// test circular reference

void testStaticActorCircularReference()
{
    tredzone::EngineCustomEventLoopFactory customEventLoopFactory;
    tredzone::AsyncNodeManager nodeManager(1024, TestCoreSet());
    tredzone::AsyncNode node(tredzone::AsyncNode::Init(nodeManager, 0, customEventLoopFactory));
    {
        node.newActor<tredzone::Actor>();

        ASSERT_THROW(node.newActor<TestStaticActorReference::SelfReferenceActor>(0), tredzone::Actor::CircularReferenceException);

        TestInitActor &handler = node.newActor<TestInitActor>(0);
        tredzone::Actor::ActorReference<TestInitActor> staticActor = handler.newReferencedSingletonActor<TestInitActor>(0);
        tredzone::Actor::ActorReference<TestInitActor> staticActor2 = handler.newReferencedSingletonActor<TestInitActor>(0);
        ASSERT_NE(&handler, &*staticActor);
        ASSERT_EQ(&*staticActor, &*staticActor2);

        tredzone::Actor::ActorReference<TestInitActor> handler2 = staticActor->newReferencedActor<TestInitActor>(0);
        
        ASSERT_THROW(handler2->newReferencedSingletonActor<TestInitActor>(0), tredzone::Actor::CircularReferenceException);
        ASSERT_THROW(handler2->newReferencedActor<TestStaticActorReference::ReferenceStaticTestInitActor>(0), tredzone::Actor::CircularReferenceException);
        ASSERT_THROW(handler2->newReferencedSingletonActor<TestStaticActorReference::ReferenceStaticTestInitActor>(0), tredzone::Actor::CircularReferenceException);

        handler2->newUnreferencedActor<TestStaticActorReference::ReferenceStaticTestInitActor>(0);
    }
    TestEventLoop testEventLoop(node);
}

struct TestOnEventException : tredzone::AsyncExceptionHandler
{
    unsigned counter;
    std::string asyncActorClassName;
    const char *onXXX_FunctionName;
    const tredzone::Actor::Event *event;
    const char *whatException;
    TestOnEventException() : counter(0), onXXX_FunctionName(0), event(0), whatException(0) {}
    
    void onEventException(tredzone::Actor *, const std::type_info &asyncActorTypeInfo,
                                  const char *onXXX_FunctionName, const tredzone::Actor::Event &event,
                                  const char *whatException) noexcept override
    {
        std::cout << "TestOnEventException::onEventException(), "
                  << (asyncActorClassName = tredzone::cppDemangledTypeInfoName(asyncActorTypeInfo))
                  << "::" << (this->onXXX_FunctionName = onXXX_FunctionName) << '(' << *(this->event = &event)
                  << ") threw (" << (this->whatException = whatException) << ')' << std::endl;
        ++counter;
    }
};

struct TestOnEventExceptionActor : TestInitActor
{
    struct Exception : std::exception
    {
        virtual const char *what() const noexcept { return "TestOnEventExceptionActor::Exception"; }
    };
    struct ExceptionEvent : Event
    {
    };
    struct ReturnToSenderEvent : Event
    {
    };

    TestOnEventExceptionActor(int = 0)
    {
        registerEventHandler<ExceptionEvent>(*this);
        registerEventHandler<ReturnToSenderEvent>(*this);
        registerUndeliveredEventHandler<ReturnToSenderEvent>(*this);
    }
    void onEvent(const ExceptionEvent &) { throw Exception(); }
    void onEvent(const ReturnToSenderEvent &)
    {
        std::cout << "TestOnEventExceptionActor::onEvent(const ReturnToSenderEvent&)" << std::endl;

        throw ReturnToSenderException();
    }
    void onUndeliveredEvent(const ReturnToSenderEvent &)
    {
        std::cout << "TestOnEventExceptionActor::onUndeliveredEvent(const ReturnToSenderEvent&)" << std::endl;

        throw Exception();
    }
};

void testOnEventException()
{
    tredzone::EngineCustomEventLoopFactory customEventLoopFactory;
    TestOnEventException testOnEventException;
    tredzone::AsyncNodeManager nodeManager(testOnEventException, 1024, TestCoreSet());
    tredzone::AsyncNode node(tredzone::AsyncNode::Init(nodeManager, 0, customEventLoopFactory));
    {
        TestOnEventExceptionActor &handler = node.newActor<TestOnEventExceptionActor>(0);

        _TREDZONE_TEST_EXIT_EXCEPTION_CATCH_BEGIN_

        TestOnEventExceptionActor::ExceptionEvent &event =
            tredzone::Actor::Event::Pipe(handler, handler).push<TestOnEventExceptionActor::ExceptionEvent>();
        node.synchronize();
        ASSERT_EQ(1u, testOnEventException.counter);
        ASSERT_EQ(testOnEventException.asyncActorClassName, tredzone::cppDemangledTypeInfoName(typeid(handler)));
        ASSERT_STREQ("onEvent", testOnEventException.onXXX_FunctionName);
        ASSERT_EQ(testOnEventException.event, &event);
        ASSERT_STREQ(testOnEventException.whatException, TestOnEventExceptionActor::Exception().what());

        _TREDZONE_TEST_EXIT_EXCEPTION_CATCH_END_

        _TREDZONE_TEST_EXIT_EXCEPTION_CATCH_BEGIN_

        TestOnEventExceptionActor::ReturnToSenderEvent &event =
            tredzone::Actor::Event::Pipe(handler, handler).push<TestOnEventExceptionActor::ReturnToSenderEvent>();
        node.synchronize();
        ASSERT_EQ(2u, testOnEventException.counter);
        ASSERT_EQ(testOnEventException.asyncActorClassName, tredzone::cppDemangledTypeInfoName(typeid(handler)));
        ASSERT_STREQ("onUndeliveredEvent", testOnEventException.onXXX_FunctionName);
        ASSERT_EQ(testOnEventException.event, &event);
        ASSERT_STREQ(testOnEventException.whatException, TestOnEventExceptionActor::Exception().what());

        _TREDZONE_TEST_EXIT_EXCEPTION_CATCH_END_

        _TREDZONE_TEST_EXIT_EXCEPTION_CATCH_BEGIN_

        tredzone::AsyncNode node2(tredzone::AsyncNode::Init(nodeManager, 1, customEventLoopFactory));
        {
            TestOnEventExceptionActor &handler2 = node2.newActor<TestOnEventExceptionActor>(0);
            TestOnEventExceptionActor::ReturnToSenderEvent &event =
                tredzone::Actor::Event::Pipe(handler, handler2)
                    .push<TestOnEventExceptionActor::ReturnToSenderEvent>();
            node.synchronize();
            node2.synchronize();
            node.synchronize();
            ASSERT_EQ(3u, testOnEventException.counter);
            ASSERT_EQ(testOnEventException.asyncActorClassName, tredzone::cppDemangledTypeInfoName(typeid(handler)));
            ASSERT_STREQ("onUndeliveredEvent", testOnEventException.onXXX_FunctionName);
            ASSERT_EQ(testOnEventException.event, &event);
            ASSERT_STREQ(testOnEventException.whatException, TestOnEventExceptionActor::Exception().what());
        }
        TestEventLoop testEventLoop(node2);

        _TREDZONE_TEST_EXIT_EXCEPTION_CATCH_END_
    }
    TestEventLoop testEventLoop(node);
}

class TestNoDestinationPipe : public TestInitActor
{
  public:
    TestNoDestinationPipe(unsigned *pcounter) : counter(*pcounter)
    {
        registerUndeliveredEventHandler<Event>(*this);
        Event::Pipe(*this, ActorId()).push<Event>();
    }

    void onUndeliveredEvent(const Event &) { ++counter; }

  private:
    unsigned &counter;
};

void testNoDestinationPipe()
{
	unsigned counter = 0;
    _TREDZONE_TEST_EXIT_EXCEPTION_CATCH_BEGIN_

    tredzone::EngineCustomEventLoopFactory customEventLoopFactory;
    tredzone::AsyncNodeManager nodeManager(1024, TestCoreSet());
    tredzone::AsyncNode node(tredzone::AsyncNode::Init(nodeManager, 1, customEventLoopFactory));
    {
        node.newActor<TestNoDestinationPipe>(&counter);
    }
    TestEventLoop testEventLoop(node);

    _TREDZONE_TEST_EXIT_EXCEPTION_CATCH_END_

    ASSERT_EQ(1u, counter);
}

TEST(Async, init) { testInit(); }
TEST(Async, initRegisterEventHandler) { testInitRegisterEventHandler(); }
TEST(Async, uniNodeEvent) { testUniNodeEvent(); }
TEST(Async, multinodeEvent) { testMultinodeEvent(); }
TEST(Async, actorReference) { testActorReference(); }
TEST(Async, DISABLED_staticActorReference) { testStaticActorCircularReference(); }      // disabled because release doesn't check circular references
TEST(Async, onEventException) { testOnEventException(); }
TEST(Async, noDestinationPipe) { testNoDestinationPipe(); }
