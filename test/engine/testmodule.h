/**
 * @file testmodule.h
 * @brief testing class
 * @copyright 2013-2019 Tredzone (www.tredzone.com). All rights reserved.
 * Please see accompanying LICENSE file for licensing terms.
 */

#pragma once

#include "testutil.h"

#include "trz/engine/engine.h"

class TestModule: public tredzone::Actor, private tredzone::Actor::Callback
{
public:
	class TestMilestone: public tredzone::MultiForwardChainLink<TestMilestone>
    {
	public:
		TestMilestone() :
				startedFlag(false) {
		}
		virtual ~TestMilestone() {
		}

	private:
		friend class TestModule;
		bool startedFlag;

		virtual void start() = 0;
		virtual bool processNext() = 0;
	};

	inline TestModule()
    {
		registerCallback(*this);
	}

protected:

	TestMilestone::ForwardChain<> testMilestoneChain;

private:
	friend class tredzone::Actor;
	virtual void onDestroyRequest() noexcept {
		if (testMilestoneChain.empty())
        {
			acceptDestroy();
		}
	}
	void onCallback() noexcept {
		_TREDZONE_TEST_EXIT_EXCEPTION_CATCH_BEGIN_

			registerCallback(*this);
			if (!testMilestoneChain.front()->startedFlag) {
				testMilestoneChain.front()->startedFlag = true;
				testMilestoneChain.front()->start();
			} else if (testMilestoneChain.front()->processNext()) {
				delete testMilestoneChain.pop_front();
				if (testMilestoneChain.empty())
                {
					requestDestroy();
				}
			}

		_TREDZONE_TEST_EXIT_EXCEPTION_CATCH_END_
	}
};

