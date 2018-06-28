/**
 * @file timeractor.cpp
 * @brief Simplx timer actor & proxy
 * @copyright 2013-2018 Tredzone (www.tredzone.com). All rights reserved.
 * Please see accompanying LICENSE file for licensing terms.
 */

#include "trz/util/timer/timeractor.h"
#include "trz/util/timer/timerproxy.h"

namespace tredzone
{

namespace timer
{

//---- CTOR --------------------------------------------------------------------

    TimerActor::TimerActor() :          // throws (std::bad_alloc, ShutdownException)
        eventHandler(*this)
{
	registerEventHandler<GetEvent>(eventHandler);
}

void TimerActor::EventHandler::onEvent(const GetEvent& event)
{
	if (event.isRouted())
    {
		throw ReturnToSenderException();
	}
	assert(event.getSourceActorId().getCoreIndex() < (size_t) MAX_NODE_COUNT);
	Client& client = this->client[event.getSourceActorId().getCoreIndex()];
	client.actorId = event.getSourceActorId();
	client.startTime = Time();
	client.duration = event.duration;
	if (inProgressClientChain.empty())
    {
		timerActor.registerPerformanceNeutralCallback(timerActor);
	}
	if (!client.inProgressFlag)
    {
		client.inProgressFlag = true;
		inProgressClientChain.push_back(&client);
	}
}

void TimerActor::onCallback() noexcept
{
	onCallback(timeGetEpoch());
}

void TimerActor::EventHandler::onCallback(const DateTime& pcurrentUtcDateTime) noexcept
{
	currentUtcDateTime = pcurrentUtcDateTime;
	bool activityFlag = false;
	for (ClientChain::iterator i = inProgressClientChain.begin(), endi = inProgressClientChain.end(); i != endi;)
    {
		Client& client = *i;
		assert(client.inProgressFlag);
		if (client.startTime == Time()) {
			client.startTime = currentUtcDateTime;
		}
		Time dt = currentUtcDateTime > client.startTime ? currentUtcDateTime - client.startTime : client.startTime - currentUtcDateTime;
		assert(dt.toNanosecond() >= 0);
		if (dt >= client.duration) {
			i = inProgressClientChain.erase(i);
			client.inProgressFlag = false;
			client.pushTimeOut();
			activityFlag = true;
		} else {
			++i;
		}
	}
	if (!inProgressClientChain.empty())
    {
		if (activityFlag) {
			timerActor.registerCallback(timerActor);
		} else {
			timerActor.registerPerformanceNeutralCallback(timerActor);
		}
	}
}

bool TimerProxy::timedOut(const Time& t, Time& minRemainToTimeOut) noexcept
{
	if (startTime == Time())
    {
		startTime = t;
	}
	Time elapsed = t > startTime ? t - startTime : startTime - t;
	assert(elapsed.toNanosecond() >= 0);
	if (elapsed < duration) {
		minRemainToTimeOut = std::min(minRemainToTimeOut, duration - elapsed);
		return false;
	}
	return true;
}

void TimerProxy::SingletonActorEventHandler::onEvent(const TimeOutEvent& e)
{
	assert(e.getSourceActorId() == sendGetEventTryCallback.getTimerServiceActorId());
	getNowTimeInProgressFlag = false;
	TimerProxy::Chain timedOutTimerProxyChain;
	Time minRemainToTimeOut(std::numeric_limits<int64_t>::max());
	for (TimerProxy::Chain::iterator i = timerProxyChain.begin(), endi = timerProxyChain.end(); i != endi;)
    {
		TimerProxy& timerProxy = *i;
		assert(timerProxy.chain == &timerProxyChain);
		if (timerProxy.timedOut(e.utcDateTime, minRemainToTimeOut)) {
			i = timerProxyChain.erase(i);
			(timerProxy.chain = &timedOutTimerProxyChain)->push_back(&timerProxy);
		} else {
			++i;
		}
	}
	while (!timedOutTimerProxyChain.empty())
    {
		TimerProxy& timerProxy = *timedOutTimerProxyChain.pop_front();
		if (timerProxy.repeatFlag)
        {
			timerProxy.startTime = e.utcDateTime;
			(timerProxy.chain = &timerProxyChain)->push_back(&timerProxy);
			minRemainToTimeOut = std::min(minRemainToTimeOut, timerProxy.duration);
		}
        else
        {
			timerProxy.chain = 0;
		}
		timerProxy.onTimeout(e.utcDateTime);
	}
    
	for (TimerProxy::Chain::iterator i = timerProxyChain.begin(), endi = timerProxyChain.end(); i != endi && i->startTime == Time(); ++i)
    {
		i->startTime = e.utcDateTime;
		minRemainToTimeOut = std::min(minRemainToTimeOut, i->duration);
	}
    
	if (!timerProxyChain.empty())
    {
		sendGetEventTryCallback.getTime(*this, minRemainToTimeOut);
	}
}

void TimerProxy::SingletonActorEventHandler::onUndeliveredEvent(const GetEvent&)
{
	throw NoServiceException();
}

} // namespace

} // namespace

