/**
 * @file testasyncinitializer.cpp
 * @brief test initializer
 * @copyright 2013-2018 Tredzone (www.tredzone.com). All rights reserved.
 * Please see accompanying LICENSE file for licensing terms.
 */

#include "gtest/gtest.h"

#include "trz/engine/initializer.h"

using namespace tredzone;

#ifndef NDEBUG

struct Test1InitData
{
    static Mutex mutex;
    int &data;
    bool &destroyedFlag;
    Test1InitData(const std::pair<int *, bool *> &init) : data(*init.first), destroyedFlag(*init.second) {}
    ~Test1InitData()
    {
        Mutex::Lock lock(mutex);
        EXPECT_FALSE(destroyedFlag);
        destroyedFlag = true;
    }
};

Mutex Test1InitData::mutex;

// typedef InitializerStartSequence<Test1InitData> Test1StartSequence;

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

struct Test1StartSequence : InitializerStartSequence<Test1InitData>
{
    template <class _Init>
    Test1StartSequence(const _Init &initData, const Engine::CoreSet &coreSet = TestCoreSet(),
                       InitializerExceptionHandler *exceptHandler = 0)
        : InitializerStartSequence<Test1InitData>(initData, coreSet, exceptHandler)
    {
    }
};

struct Test1Actor : Test1StartSequence::Actor
{
    int &onInitializeCount;
    Test1Actor(int *ponInitializeCount) : onInitializeCount(*ponInitializeCount) {}
    virtual void onInitialize(Test1InitData &init)
    {
        ASSERT_EQ(onInitializeCount, init.data);
        ++init.data;
        if (++onInitializeCount == 2)
        {
            Test1StartSequence::Actor::onInitialize(init);
        }
        ASSERT_GT(Test1StartSequence::debugGetCoreInitializerSingletonCount(), 0);
    }
    virtual void onDestroyRequest() noexcept
    {
        if (Test1StartSequence::debugGetCoreInitializerSingletonCount() == 0)
        {
            Test1StartSequence::Actor::onDestroyRequest();
        }
        else
        {
            requestDestroy();
        }
    }
};

void test1()
{
    int data = 0;
    int onInitializeCount = 0;
    bool initDataDestroyedFlag = false;
    {
        Test1StartSequence test1StartSequence(std::make_pair(&data, &initDataDestroyedFlag));
        test1StartSequence.addActor<Test1Actor>(0, &onInitializeCount);
        Engine engine(test1StartSequence);
    }
    ASSERT_TRUE(initDataDestroyedFlag);
    ASSERT_EQ(onInitializeCount, data);
    ASSERT_EQ(2, data);
}

typedef Test1StartSequence Test2StartSequence;

struct Test2Actor : Test2StartSequence::Actor
{
    int &onInitializeCount;
    Test2Actor(int *ponInitializeCount) : onInitializeCount(*ponInitializeCount) {}
    virtual void onInitialize(Test1InitData &init)
    {
        ASSERT_EQ(onInitializeCount, init.data / 4);
        ++init.data;
        if (++onInitializeCount == 2)
        {
            Test2StartSequence::Actor::onInitialize(init);
        }
        ASSERT_GT(Test2StartSequence::debugGetCoreInitializerSingletonCount(), 0);
    }
    virtual void onDestroyRequest() noexcept
    {
        if (Test2StartSequence::debugGetCoreInitializerSingletonCount() == 0)
        {
            Test2StartSequence::Actor::onDestroyRequest();
        }
        else
        {
            requestDestroy();
        }
    }
};

void test2()
{
    int data = 0;
    int onInitializeCount1 = 0;
    int onInitializeCount2 = 0;
    int onInitializeCount3 = 0;
    int onInitializeCount4 = 0;
    bool initDataDestroyedFlag = false;
    {
        Test2StartSequence test2StartSequence(std::make_pair(&data, &initDataDestroyedFlag));
        test2StartSequence.addActor<Test2Actor>(0, &onInitializeCount1);
        test2StartSequence.addActor<Test2Actor>(1, &onInitializeCount2);
        test2StartSequence.addActor<Test2Actor>(1, &onInitializeCount3);
        test2StartSequence.addActor<Test2Actor>(0, &onInitializeCount4);
        Engine engine(test2StartSequence);
    }
    ASSERT_TRUE(initDataDestroyedFlag);
    ASSERT_EQ(2, onInitializeCount1);
    ASSERT_EQ(2, onInitializeCount2);
    ASSERT_EQ(2, onInitializeCount3);
    ASSERT_EQ(2, onInitializeCount4);
    ASSERT_EQ(8, data);
}

void test3()
{
    int data = 0;
    int onInitializeCount1 = 0;
    int onInitializeCount2 = 0;
    int onInitializeCount3 = 0;
    int onInitializeCount4 = 0;
    bool initDataDestroyedFlag = false;
    {
        Engine::CoreSet coreSet;
        coreSet.set(0);
        Test2StartSequence test2StartSequence(std::make_pair(&data, &initDataDestroyedFlag), coreSet);
        test2StartSequence.addActor<Test2Actor>(0, &onInitializeCount1);
        test2StartSequence.addActor<Test2Actor>(0, &onInitializeCount2);
        test2StartSequence.addActor<Test2Actor>(0, &onInitializeCount3);
        test2StartSequence.addActor<Test2Actor>(0, &onInitializeCount4);
        Engine engine(test2StartSequence);
    }
    ASSERT_TRUE(initDataDestroyedFlag);
    ASSERT_EQ(2, onInitializeCount1);
    ASSERT_EQ(2, onInitializeCount2);
    ASSERT_EQ(2, onInitializeCount3);
    ASSERT_EQ(2, onInitializeCount4);
    ASSERT_EQ(8, data);
}

TEST(AsyncInitializer, test1) { test1(); }
TEST(AsyncInitializer, test2) { test2(); }
TEST(AsyncInitializer, test3) { test3(); }

#endif