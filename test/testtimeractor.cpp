/**
 * @file testtimeractor.cpp
 * @brief test timer actor
 * @copyright 2013-2018 Tredzone (www.tredzone.com). All rights reserved.
 * Please see accompanying LICENSE file for licensing terms.
 */

#include "testmodule.h"

#include "gtest/gtest.h"

#include "trz/engine/actor.h"

#include "trz/util/timer/timerevent.h"
#include "trz/util/timer/timerproxy.h"
#include "trz/util/timer/timeractor.h"

using namespace tredzone;
using namespace tredzone::timer;

namespace tredzone
{

struct DateTimeAccessorTag
{
};

template<>
class Accessor<DateTimeAccessorTag> : public DateTime
{
public:
	Accessor(const Time& t) noexcept
    {
		Time::operator=(t);
	}
};

} // namespace


class TestTimerModule: public TestModule
{
public:
	typedef Actor::property_type::Collection Parameters;

	TestTimerModule(const Parameters&);
	virtual ~TestTimerModule() noexcept
    {
		_TREDZONE_TEST_EXIT_EXCEPTION_CATCH_BEGIN_

			std::cout << "TestTimerModule::~TestTimerModule()" << std::endl;

		_TREDZONE_TEST_EXIT_EXCEPTION_CATCH_END_
	}

private:
	struct TestMilestone1;
	
    struct ClientStub: Actor {
		unsigned onTimeOutEventCount;
		DateTime onTimeOutEventUTCTime;

		ClientStub() :
				onTimeOutEventCount(0) {
			std::cout << "TestTimerModule::ClientStub::ClientStub()"
					<< std::endl;

			registerEventHandler<TimeOutEvent>(*this);
		}
		virtual ~ClientStub() noexcept {
			std::cout << "TestTimerModule::ClientStub::~ClientStub()"
					<< std::endl;
		}
		void getTime(const ActorId& timerModuleActorId,
				const Time& duration = Time()) {
			Event::Pipe(*this, timerModuleActorId).push<GetEvent>(duration);
		}
		void onEvent(const TimeOutEvent& event) {
			std::cout << "TestTimerModule::ClientStub::onEvent(" << event << ')'
					<< std::endl;

			++onTimeOutEventCount;
			onTimeOutEventUTCTime = event.utcDateTime;
		}
	};

	Actor::ActorReference<ClientStub> clientStub;
};

struct TestTimerModule::TestMilestone1: public TestMilestone
{
	struct TimerModule1: TimerActor
    {
		DateTime currentUtcDateTime;
		virtual void onCallback() noexcept
        {
			TimerActor::onCallback(currentUtcDateTime);
		}
	};

	TestTimerModule& testTimerModule;
	ActorReference<TimerModule1> timerModule1;
	TestMilestone1(TestTimerModule& ptestTimerModule) :
			testTimerModule(ptestTimerModule), timerModule1(
					testTimerModule.newReferencedActor<TimerModule1>()) {
		std::cout << "TestTimerModule::TestMilestone1::TestMilestone1()" << std::endl;
	}
	virtual ~TestMilestone1()
    {
		std::cout << "TestTimerModule::TestMilestone1::~TestMilestone1()" << std::endl;

        EXPECT_EQ(testTimerModule.clientStub->onTimeOutEventCount, 1u);
		EXPECT_EQ(testTimerModule.clientStub->onTimeOutEventUTCTime, timerModule1->currentUtcDateTime);
	}
	virtual void start()
    {
		std::cout << "TestTimerModule::TestMilestone1::start()" << std::endl;

		testTimerModule.clientStub->onTimeOutEventCount = 0;
		timerModule1->currentUtcDateTime = Accessor<DateTimeAccessorTag>(Time::Second(1));
		testTimerModule.clientStub->getTime(timerModule1->getActorId());
	}
	virtual bool processNext()
    {
		return testTimerModule.clientStub->onTimeOutEventCount == 1;
	}
};

TestTimerModule::TestTimerModule(const Parameters&) :
		clientStub(newReferencedActor<ClientStub>()) {
	std::cout << "TestTimerModule::TestTimerModule()" << std::endl;

	testMilestoneChain.push_back(new TestMilestone1(*this));
}

struct TestCoreSet: tredzone::Engine::CoreSet {
	TestCoreSet() {
		for(int i = 0, coreCount = std::min(16, (int)tredzone::cpuGetCount()); i < coreCount; ++i) {
			set((tredzone::Engine::CoreId)i);
		}
	}
};

void testTimerModule(const char*) {
	TestCoreSet coreSet;
	Engine::StartSequence startSequence(coreSet);
	startSequence.addActor<TestTimerModule>(0, Actor::property_type::Collection());
	Engine engine(startSequence);
}

#define ONTIMEOUT_MAX_COUNT 5

struct TimerEventTestActor : Actor, timer::TimerProxy
{
	TimerEventTestActor() : TimerProxy(static_cast<tredzone::Actor&>(*this)), count(0) {
		std::cout << "TimerEventTestActor::TimerEventTestActor()" << std::endl;
		setRepeat(tredzone::Time::Millisecond(100));
	}
	~TimerEventTestActor() noexcept {
		std::cout << "~TimerEventTestActor::TimerEventTestActor()" << std::endl;
	}
	void onTimeout(const DateTime& dt) noexcept override
    {
		ASSERT_LT(count, ONTIMEOUT_MAX_COUNT);
		++count;
		std::cout << "TimerEventTestActor::onTimeOut() " << (int)count << " : "
				<< "Local: " << (int)dt.getLocalHour() << ":"
				<< (int)dt.getLocalMinute() << ":"
				<< (int)dt.getLocalSecond() << ":"
				<< dt.extractMilliseconds() << ":"
				<< dt.extractMicroseconds() << ":"
				<< dt.extractNanoseconds() << " "
				<< dt.getLocalYear() << "/" << (int)dt.getLocalMonth() << "/" << (int)dt.getLocalDayOfTheMonth() << " "
				<< dt.toNanosecond() << "ns "
				<< (int)dt.getLocalDayOfTheYear() << "th day of year "
				<< "day " << (int)dt.getLocalDayOfTheWeek() << " of week "
				<< "UTC: " << (int)dt.getUTCHour() << ":"
				<< (int)dt.getUTCMinute() << ":"
				<< (int)dt.getUTCSecond() << " "
				<< dt.getUTCYear() << "/" << (int)dt.getUTCMonth() << "/" << (int)dt.getUTCDayOfTheMonth()
				<< std::endl;
	}
	
    void onDestroyRequest() noexcept override
    {
		ASSERT_LE(count, ONTIMEOUT_MAX_COUNT);
		if (count == ONTIMEOUT_MAX_COUNT)
        {
			acceptDestroy();
		}
        else
        {
			requestDestroy();
		}
	}

private:

	uint8_t count;
};

void testDateTimeClass(const char*) {
	Engine::StartSequence startSequence;
	startSequence.addServiceActor<service::Timer, timer::TimerActor>(0);
	startSequence.addActor<TimerEventTestActor>(0);
	Engine engine(startSequence);
}

TEST(Timer, module) { testTimerModule("testTimerModule"); }
TEST(Timer, DateTime) { testDateTimeClass("testDateTimeClass"); }
