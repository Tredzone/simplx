/**
 * @file timerevent.h
 * @brief Simplx timer event
 * @copyright 2013-2019 Tredzone (www.tredzone.com). All rights reserved.
 * Please see accompanying LICENSE file for licensing terms.
 */

#pragma once

#include <ctime>

#include "trz/engine/internal/service.h"
#include "trz/engine/actor.h"

namespace tredzone
{

namespace service
{
// import into namespace

struct Timer : public Service
{
};

} // namespace service

namespace timer
{

#pragma pack(push)
#pragma pack(1)
struct GetEvent : public Actor::Event
{
	/**
	 * From the client, request utctime when duration is elapsed, or duration is nul.
	 * One request at a time by source actor id.
	 * Any new request from a source actor will overwrite any existing one from the same source.
	 * This event is intended to be implemented by a singleton actor.
	 */
	GetEvent() noexcept
    {
	}
    
	GetEvent(const Time& pduration) noexcept
        : duration(pduration)
    {
	}
    
	static void nameToOStream(std::ostream& s, const Event&)
    {
		s << "tredzone::timer::GetEvent";
	}
    
	static void contentToOStream(std::ostream& s, const Event& event)
    {
		s << "duration=" << Time::Millisecond( static_cast<const GetEvent&>(event).duration);
	}
    
    Time duration;
};

struct TimeOutEvent: Actor::Event {
	/**
	 * From the server, response to GetEvent to the request source actor id.
	 */

	DateTime utcDateTime;
	inline TimeOutEvent(const DateTime& utcDateTime) noexcept : utcDateTime(utcDateTime) {
	}
	inline static void nameToOStream(std::ostream& s, const Event&) {
		s << "tredzone::timer::TimeOutEvent";
	}
	inline static void contentToOStream(std::ostream& s, const Event& event)
    {
		time_t tsec = static_cast<const TimeOutEvent&>(event).utcDateTime.extractSeconds();
		struct tm t;

        gmtime_r(&tsec, &t);        // (safe version w/out static var)
        
		s << "utcTime=" << t.tm_mday << '/' << (t.tm_mon + 1) << '/' << (t.tm_year + 1900)
				<< '-' << t.tm_hour << ':' << t.tm_min << ':' << t.tm_sec << '.';
		char cfill = s.fill('0');
		s.width(3);
		s << std::right << (static_cast<const TimeOutEvent&>(event).utcDateTime.extractNanoseconds() / 1000000);
		s.fill(cfill);
	}
};
#pragma pack(pop)

}
}

