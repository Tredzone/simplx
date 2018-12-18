/**
 * @file timerproxy.h
 * @brief Simplx timer proxy
 * @copyright 2013-2018 Tredzone (www.tredzone.com). All rights reserved.
 * Please see accompanying LICENSE file for licensing terms.
 */

#pragma once

#include "trz/util/timer/timerevent.h"

namespace tredzone
{
namespace timer
{

//---- Timer Proxy -------------------------------------------------------------

class TimerProxy: private MultiDoubleChainLink<TimerProxy>
{
public:
	struct NoServiceException: std::exception
    {
		virtual const char* what() const noexcept
        {
			return "tredzone::timer::TimerProxy::NoServiceException";
		}
	};

	/**
	 * @brief Constructor
	 * @param actor used to reference a TimerClient core Singleton
	 * @param serviceIndex the engine Services table
	 * @return
	 * throw (std::bad_alloc, Actor::ShutdownException, NoServiceException)
	 */
	inline TimerProxy(Actor& actor) :
			singletonActorEventHandler(actor.newReferencedSingletonActor<SingletonActorEventHandler>(
							&actor.getEngine().getServiceIndex())), repeatFlag(false), chain(0) {
	}
	/**
	 * throw (std::bad_alloc, Actor::ShutdownException, NoServiceException)
	 */
	inline TimerProxy(Actor& actor, const Engine::ServiceIndex& serviceIndex) :
			singletonActorEventHandler(actor.newReferencedSingletonActor<SingletonActorEventHandler>(
							&serviceIndex)), repeatFlag(false), chain(0) {
	}
	/**
	 * @brief Default destructor, delete timer client request
	 */
	virtual ~TimerProxy() noexcept {
		unset();
	}
	/**
     * Set the delay before the next onTimeout() callback.
     * note that a set() call erase automatically the last one
     * @param duration the waiting period before a new {@link #onTimeout(long) } callback
     */
	inline void set(const Time& duration) noexcept {
		singletonActorEventHandler->set(*this, duration);
	}
	/**
	 * @brief calls the {@link  #set(long)} method with 0 as delay
	 */
	inline void setNow() noexcept {
		singletonActorEventHandler->set(*this, Time());
	}
	/**
     * the same functional as the {@link  #set(long)} method, but the {@link #onTimeout(long) } callback will be repeated until an {@link  #unset() }is done
     * @param duration the delay between successive {@link #onTimeout(long) } callback
     */
	inline void setRepeat(const Time& duration) noexcept {
		singletonActorEventHandler->setRepeat(*this, duration);
	}
	/**
     * @brief reset the Timer client, delete last {@link  #set(long)} and setRepeat() calls
     */
	inline void unset() noexcept {
		singletonActorEventHandler->unset(*this);
	}
    
    inline
    void stop(void) noexcept
    {
        unset();
    }
    
	/**
	 *
	 * @return true if a set() or setRepeat() is done, false otherwise
	 */
	inline bool isSet() const noexcept {
		return chain != 0;
	}
	/**
	 *
	 * @param serviceIndex the engine service table
	 * @return TimerService ActorId
	 * @throw (NoServiceException) if the TimerService was not started
	 */
	inline static const Actor::ActorId& getTimerServiceActorId(const Engine::ServiceIndex& serviceIndex) {
		const Actor::ActorId& ret = serviceIndex.getServiceActorId<
				service::Timer>();
		if (ret == tredzone::null) {
			throw NoServiceException();
		}
		return ret;
	}

private:
	struct Chain: DoubleChain<0u, Chain> {
		inline static TimerProxy* getItem(
				MultiDoubleChainLink<TimerProxy>* link) noexcept {
			return static_cast<TimerProxy*>(link);
		}
		inline static const TimerProxy* getItem(
				const MultiDoubleChainLink<TimerProxy>* link) noexcept {
			return static_cast<const TimerProxy*>(link);
		}
		inline static MultiDoubleChainLink<TimerProxy>* getLink(
				TimerProxy* item) noexcept {
			return static_cast<MultiDoubleChainLink<TimerProxy>*>(item);
		}
		inline static const MultiDoubleChainLink<TimerProxy>* getLink(
				const TimerProxy* item) noexcept {
			return static_cast<const MultiDoubleChainLink<TimerProxy>*>(item);
		}
	};
    
	class SingletonActorEventHandler: public Actor {
	public:
		/**
		 * throw (ShutdownException, NoServiceException)
		 */
		inline SingletonActorEventHandler(const Engine::ServiceIndex* serviceIndex) :
		sendGetEventTryCallback(*this, getTimerServiceActorId(*serviceIndex)), getNowTimeInProgressFlag(false) {
			registerEventHandler<TimeOutEvent>(*this);
			registerUndeliveredEventHandler<GetEvent>(*this);
		}
		inline void set(TimerProxy& timerProxy, const Time& duration) noexcept {
			unset(timerProxy);
			timerProxy.startTime = Time();
			timerProxy.duration = duration;
			timerProxy.repeatFlag = false;
			(timerProxy.chain = &timerProxyChain)->push_front(&timerProxy);
			sendGetEventTryCallback.getTime(*this, Time());
		}
		inline void setRepeat(TimerProxy& timerProxy,
				const Time& duration) noexcept {
			set(timerProxy, duration);
			timerProxy.repeatFlag = true;
		}
		inline void unset(TimerProxy& timerProxy) noexcept {
			if (timerProxy.chain != 0) {
				timerProxy.chain->remove(&timerProxy);
				timerProxy.chain = 0;
			}
		}
		void onEvent(const TimeOutEvent&);
		void onUndeliveredEvent(const GetEvent&);

    private:
    
		class SendGetEventTryCallback: public Callback
        {
		public:
			inline SendGetEventTryCallback(SingletonActorEventHandler& handler,
					const ActorId& timerServiceActorId) noexcept :
					timerServicePipe(handler, timerServiceActorId), singletonActorEventHandler(handler)
            {
			}
			inline void getTime(SingletonActorEventHandler& handler,
					const Time& pduration) noexcept
            {
				bool getNowTimeFlag = pduration == Time();
				if (getNowTimeFlag && handler.getNowTimeInProgressFlag) {
					return;
				}
				handler.getNowTimeInProgressFlag = getNowTimeFlag;
				duration = pduration;
				onCallback();
			}
			inline const ActorId& getTimerServiceActorId() const noexcept {
				return timerServicePipe.getDestinationActorId();
			}
			inline void onCallback() noexcept
            {
				try
                {
					timerServicePipe.push<GetEvent>(duration);
				} catch (std::bad_alloc&)
                {
					singletonActorEventHandler.registerCallback(*this);
				}
			}

        private:
        
			Time                        duration;
			Event::Pipe                 timerServicePipe;
			SingletonActorEventHandler  &singletonActorEventHandler;
		};

		SendGetEventTryCallback sendGetEventTryCallback;
		TimerProxy::Chain       timerProxyChain;
		bool                    getNowTimeInProgressFlag;
	};

	Actor::ActorReference<SingletonActorEventHandler>   singletonActorEventHandler;
	Time                                                startTime;
	Time                                                duration;
	bool                                                repeatFlag;
	TimerProxy::Chain                                   *chain;

	virtual void onTimeout(const DateTime&) noexcept {}
	inline bool timedOut(const Time& t, Time& minRemainToTimeOut) noexcept;
};

} // namespace timer

} // namespace tredzone

