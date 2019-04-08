/**
 * @file testasyncengine.cpp
 * @brief test Simplx engine
 * @copyright 2013-2019 Tredzone (www.tredzone.com). All rights reserved.
 * Please see accompanying LICENSE file for licensing terms.
 */

#include <gtest/gtest.h>

#include "trz/engine/engine.h"
#include "trz/engine/internal/node.h"

using namespace tredzone;

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

struct TestStartSequence : Engine::StartSequence
{
    TestStartSequence(const tredzone::Engine::CoreSet &coreSet = TestCoreSet()) : Engine::StartSequence(coreSet) {}
};

class TestEngine
{
  public:
    TestEngine(Engine::StartSequence &startSequence)
        : asyncEngine((startSequence.setEngineCustomCoreActorFactory(testAsyncEngineCustomCoreActorFactory),
                       startSequence))
    {
        EXPECT_EQ((testAsyncEngineCustomCoreActorFactory.coreCount == 0), (startSequence.getCoreSet().size() == 0));
    }

  private:
    struct TestAsyncEngineCustomCoreActorFactory : EngineCustomCoreActorFactory
    {
        volatile unsigned coreCount;
        TestAsyncEngineCustomCoreActorFactory() : coreCount(0) {}
        virtual ~TestAsyncEngineCustomCoreActorFactory() noexcept { EXPECT_EQ(0u, coreCount); }
        virtual Actor::ActorReference<Actor> newCustomCoreActor(Engine &, Actor::CoreId, bool,
                                                                          Actor &parent)
        {
            return parent.newReferencedActor<TestCoreActor>(&coreCount);
        }
    };
    struct TestCoreActor : Actor
    {
        volatile unsigned &coreCount;
        TestCoreActor(volatile unsigned *pcoreCount) : coreCount(*pcoreCount)
        {
            std::cout << "TestEngine::Engine::TestCoreActor::TestCoreActor(), " << std::endl;
            atomicAddAndFetch(&coreCount, 1);
        }
        virtual ~TestCoreActor() noexcept
        {
            std::cout << "TestEngine::Engine::TestCoreActor::~TestCoreActor(), " << std::endl;
            atomicSubAndFetch(&coreCount, 1);
        }
    };

    TestAsyncEngineCustomCoreActorFactory testAsyncEngineCustomCoreActorFactory;
    Engine asyncEngine;
};

struct TestModule : Actor
{
    typedef property_type::Collection Parameters;
    TestModule(const Parameters &) { std::cout << "TestModule::TestModule(), " << info() << std::endl; }
    virtual ~TestModule() noexcept { std::cout << "TestModule::~TestModule(), " << info() << std::endl; }
    std::string info() const
    {
        std::stringstream s;
        s << cppDemangledTypeInfoName(typeid(*this)) << std::ends;
        return s.str();
    }
};

template <class _AsyncEngine>
void testInit()
{
    {
        Engine::CoreSet coreSet;
        TestStartSequence startSequence(coreSet);
        _AsyncEngine engine(startSequence);
    }
    {
        TestStartSequence startSequence;
        startSequence.addActor<TestModule>(0, Actor::property_type::Collection(startSequence.getAllocator()));
        _AsyncEngine engine(startSequence);
    }
}

static
void testInit()
{
    testInit<Engine>();
    testInit<TestEngine>();
}

struct TestInitException : std::exception
{
    virtual const char *what() const noexcept { return "TestInitException"; }
};

struct TestInitExceptionActor : Actor
{
    TestInitExceptionActor() { throw TestInitException(); }
};

void testInitException()
{
    TestStartSequence startSequence;
    startSequence.addActor<Actor>(0);
    startSequence.addActor<Actor>(1);
    startSequence.addActor<TestInitExceptionActor>(0);

    ASSERT_THROW(Engine engine(startSequence), TestInitException);
}

struct testCoreInUseExceptionModule : Actor
{
    testCoreInUseExceptionModule() { getEngine().newCore<Actor>(getCore(), false); }
};

void testCoreInUseException()
{
    TestStartSequence startSequence;
    startSequence.addActor<Actor>(0);
    startSequence.addActor<testCoreInUseExceptionModule>(1);

    ASSERT_THROW(Engine engine(startSequence), Engine::CoreInUseException);
}

void testAllocator()
{
    unsigned index = sizeof(void *) == 4 ? 2 : 3;
    for (size_t imax = sizeof(void *), i = 0; index < 20; imax *= 2, ++index)
    {
        for (; i <= imax; ++i)
        {
            ASSERT_EQ(index, AsyncNodeAllocator::index(i));
        }
    }

    TestStartSequence startSequence;
    Actor::Allocator<char> allocator(startSequence.getAllocator());
    for (size_t i = 1; i < 100000; ++i)
    {
        char *p = allocator.allocate(i);
        for (size_t j = 0; j < i; ++j)
        {
            p[j] = (char)j;
        }
        allocator.deallocate(p, i);
    }
}

class TestBasicService : public Actor
{
  public:
    template<int>
    struct TestService : Service
    {
    };

    struct Shared
    {
        bool service1DestroyedFlag;
        bool service2DestroyedFlag;
        bool service3DestroyedFlag;
        bool service4DestroyedFlag;
        bool service5DestroyedFlag;
        static ThreadLocalStorage<Shared *> tls;

        Shared()
            : service1DestroyedFlag(false), service2DestroyedFlag(false), service3DestroyedFlag(false),
              service4DestroyedFlag(false), service5DestroyedFlag(false)
        {
            EXPECT_EQ(0, tls.get());
        }
    };

    struct Service2Actor : Actor
    {
        Service2Actor() { requestDestroy(); }
        virtual void onDestroyRequest() noexcept
        {
            Shared *shared = Shared::tls.get();
            ASSERT_TRUE(shared != 0);
            ASSERT_TRUE(shared->service5DestroyedFlag);
            ASSERT_TRUE(shared->service4DestroyedFlag);
            ASSERT_TRUE(shared->service3DestroyedFlag);
            ASSERT_FALSE(shared->service2DestroyedFlag);
            ASSERT_FALSE(shared->service1DestroyedFlag);
            shared->service2DestroyedFlag = true;
            Actor::onDestroyRequest();
        }
    };
    struct Service3Actor : Actor
    {
        Service3Actor() { requestDestroy(); }
        virtual void onDestroyRequest() noexcept
        {
            Shared *shared = Shared::tls.get();
            ASSERT_TRUE(shared != 0);
            ASSERT_TRUE(shared->service5DestroyedFlag);
            ASSERT_TRUE(shared->service4DestroyedFlag);
            ASSERT_FALSE(shared->service3DestroyedFlag);
            ASSERT_FALSE(shared->service2DestroyedFlag);
            ASSERT_FALSE(shared->service1DestroyedFlag);
            shared->service3DestroyedFlag = true;
            Actor::onDestroyRequest();
        }
    };
    struct Service4Actor : Actor
    {
        Service4Actor() { requestDestroy(); }
        virtual void onDestroyRequest() noexcept
        {
            Shared *shared = Shared::tls.get();
            ASSERT_TRUE(shared != 0);
            ASSERT_TRUE(shared->service5DestroyedFlag);
            ASSERT_FALSE(shared->service4DestroyedFlag);
            ASSERT_FALSE(shared->service3DestroyedFlag);
            ASSERT_FALSE(shared->service2DestroyedFlag);
            ASSERT_FALSE(shared->service1DestroyedFlag);
            shared->service4DestroyedFlag = true;
            Actor::onDestroyRequest();
        }
    };
    struct Service5Actor : Actor
    {
        Service5Actor() { requestDestroy(); }
        virtual void onDestroyRequest() noexcept
        {
            Shared *shared = Shared::tls.get();
            ASSERT_TRUE(shared != 0);
            ASSERT_FALSE(shared->service5DestroyedFlag);
            ASSERT_FALSE(shared->service4DestroyedFlag);
            ASSERT_FALSE(shared->service3DestroyedFlag);
            ASSERT_FALSE(shared->service2DestroyedFlag);
            ASSERT_FALSE(shared->service1DestroyedFlag);
            shared->service5DestroyedFlag = true;
            Actor::onDestroyRequest();
        }
    };
    struct NonServiceActor : Actor
    {
        Shared &shared;
        NonServiceActor(Shared *const shared) : shared(*shared) {}
        virtual ~NonServiceActor() noexcept
        {
            Shared::tls.set(&shared);
            EXPECT_FALSE(shared.service5DestroyedFlag);
            EXPECT_FALSE(shared.service4DestroyedFlag);
            EXPECT_FALSE(shared.service3DestroyedFlag);
            EXPECT_FALSE(shared.service2DestroyedFlag);
            EXPECT_FALSE(shared.service1DestroyedFlag);
        }
    };

    TestBasicService(Shared *const shared) : shared(*shared)
    {
        EXPECT_FALSE(shared->service5DestroyedFlag);
        EXPECT_FALSE(shared->service4DestroyedFlag);
        EXPECT_FALSE(shared->service3DestroyedFlag);
        EXPECT_FALSE(shared->service2DestroyedFlag);
        EXPECT_FALSE(shared->service1DestroyedFlag);
        requestDestroy();
    }

  protected:
    virtual void onDestroyRequest() noexcept
    {
        ASSERT_TRUE(shared.service5DestroyedFlag);
        ASSERT_TRUE(shared.service4DestroyedFlag);
        ASSERT_TRUE(shared.service3DestroyedFlag);
        ASSERT_TRUE(shared.service2DestroyedFlag);
        ASSERT_FALSE(shared.service1DestroyedFlag);
        shared.service1DestroyedFlag = true;
        Actor::onDestroyRequest();
    }

  private:
    Shared &shared;
};

ThreadLocalStorage<TestBasicService::Shared *> TestBasicService::Shared::tls;

void testBasicService()
{
    TestBasicService::Shared shared;
    TestStartSequence startSequence;
    startSequence.addServiceActor<TestBasicService::TestService<1>, TestBasicService>(0, &shared);
    startSequence.addServiceActor<TestBasicService::TestService<2>, TestBasicService::Service2Actor>(0);

    ASSERT_THROW((startSequence.addServiceActor<TestBasicService::TestService<2>, TestBasicService::Service3Actor>(0)),
                 Engine::StartSequence::DuplicateServiceException);

    startSequence.addServiceActor<TestBasicService::TestService<3>, TestBasicService::Service3Actor>(0);
    startSequence.addServiceActor<Engine::StartSequence::AnonymousService, TestBasicService::Service4Actor>(0);
    startSequence.addServiceActor<Engine::StartSequence::AnonymousService, TestBasicService::Service5Actor>(0);
    startSequence.addActor<TestBasicService::NonServiceActor>(0, &shared);
    ASSERT_FALSE(shared.service1DestroyedFlag);
    ASSERT_FALSE(shared.service2DestroyedFlag);
    ASSERT_FALSE(shared.service3DestroyedFlag);
    ASSERT_FALSE(shared.service4DestroyedFlag);
    ASSERT_FALSE(shared.service5DestroyedFlag);
    {
        Engine engine(startSequence);
    }
    ASSERT_TRUE(shared.service1DestroyedFlag);
    ASSERT_TRUE(shared.service2DestroyedFlag);
    ASSERT_TRUE(shared.service3DestroyedFlag);
    ASSERT_TRUE(shared.service4DestroyedFlag);
    ASSERT_TRUE(shared.service5DestroyedFlag);
}

struct TestActor : public Actor
{
    struct Tag : Service
    {
    };

    TestActor() { requestDestroy(); }
    virtual void onDestroyRequest() noexcept { Actor::onDestroyRequest(); }
};

struct TestMemoryHogActor : public Actor
{
    struct Shared
    {
        Shared() : isDestroyed(false) {}
        bool isDestroyed;
    };

    TestMemoryHogActor(Shared *const shared) : shared(*shared)
    {
        std::cout << "TestMemoryHogActor()" << std::endl;
        char *test = new (Allocator<char>(getAllocator()).allocate(16)) char[16];
        test[0] = 't';
        test[1] = 'r';
        test[2] = 'z';
        test[3] = '\0';
        std::cout << test << std::endl;
    }

    ~TestMemoryHogActor() noexcept
    {
        std::cout << "~TestMemoryHogActor()" << std::endl;
        shared.isDestroyed = true;
    }

  private:
    Shared &shared;
};

// test Anonymous Service
void testAnonymousService()
{
    Engine::StartSequence startSequence;
    startSequence.addServiceActor<TestStartSequence::AnonymousService, TestActor>(0);
    startSequence.addServiceActor<TestActor::Tag, TestActor>(0);

    Engine engine(startSequence);
}

void testInvalidCore()
{
    TestStartSequence startSequence;
    ASSERT_THROW(startSequence.addActor<TestActor>((tredzone::Engine::CoreId)tredzone::cpuGetCount()), std::runtime_error);
}

void testUnreleasedMemory()                 // [PL to check]
{
    TestMemoryHogActor::Shared shared;
    TestStartSequence startSequence;
    startSequence.addActor<TestMemoryHogActor>(0, &shared);

    ASSERT_FALSE(shared.isDestroyed);
    {
        Engine engine(startSequence);
    }
    ASSERT_TRUE(shared.isDestroyed);
    sleep(1000);
}

TEST(Engine, init) { testInit(); }
TEST(Engine, initException) { testInitException(); }
TEST(Engine, allocator) { testAllocator(); }
TEST(Engine, coreInUseException) { testCoreInUseException(); }
TEST(Engine, basicService) { testBasicService(); }
TEST(Engine, anonymousService) { testAnonymousService(); }
TEST(Engine, invalidCore) { testInvalidCore(); }
TEST(Engine, DISABLED_unreleasedMemory) { testUnreleasedMemory(); }