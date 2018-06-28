/**
 * @file timeractor.h
 * @brief Simplx timer actor
 * @copyright 2013-2018 Tredzone (www.tredzone.com). All rights reserved.
 * Please see accompanying LICENSE file for licensing terms.
 */

#pragma once

#include "trz/util/timer/timerevent.h"

namespace tredzone
{

namespace timer
{

//---- Timer Actor -------------------------------------------------------------
    
class TimerActor: public Actor, private Actor::Callback
{
public:
	TimerActor();	// throws (std::bad_alloc, ShutdownException)

protected:

    inline
    void onCallback(const DateTime& utcTime) noexcept
    {   
        // protected for unit testing override [PL bs]
		eventHandler.onCallback(utcTime);
	}

private:

	friend class ::tredzone::Actor;

	struct EventHandler
    {
		struct Client: ::tredzone::MultiDoubleChainLink<Client>, Actor::Callback
        {
			TimerActor* timerActor;
			ActorId actorId;
			Time startTime;
			Time duration;
			bool inProgressFlag;

			inline
            Client() noexcept :
				timerActor(0), inProgressFlag(false)
            {
			}
			inline
            void pushTimeOut() noexcept
            {
				try
                {
					Event::Pipe(*timerActor, actorId).push<TimeOutEvent>(timerActor->eventHandler.currentUtcDateTime);
				} catch (std::bad_alloc&)
                {
					timerActor->registerCallback(*this);
				}
			}

			inline void onCallback() noexcept {
				pushTimeOut();
			}
		};

		typedef ::tredzone::MultiDoubleChainLink<Client>::DoubleChain<> ClientChain;
		TimerActor& timerActor;
		Client client[MAX_NODE_COUNT];
		ClientChain inProgressClientChain;
		DateTime currentUtcDateTime;

		inline EventHandler(TimerActor& ptimerActor) noexcept :
				timerActor(ptimerActor)
        {
			for(int i = 0; i < MAX_NODE_COUNT; ++i) {
				client[i].timerActor = &timerActor;
			}
		}
		void onEvent(const GetEvent&);
		void onCallback(const DateTime&) noexcept;
	};

	EventHandler eventHandler;

	virtual void onCallback() noexcept; // override to use testing custom utc time, by enclosing call to protected onCallback(custom utc time).
};

} // namespace

} // namespace

