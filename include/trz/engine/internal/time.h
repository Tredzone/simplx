/**
 * @file time.h
 * @brief custom time-related classes
 * @copyright 2013-2019 Tredzone (www.tredzone.com). All rights reserved.
 * Please see accompanying LICENSE file for licensing terms.
 */

#pragma once

#include <ctime>

// #include "trz/engine/platform.h"

namespace tredzone
{

enum Null
{
    null = 0
};

#pragma pack(push)
#pragma pack(1)
/**
 * @brief The main class for Time.
 * @note Smallest resolution is the nanosecond
 */
class Time
{
  public:
    struct Second;
    struct Millisecond;
    struct Microsecond;
    struct Nanosecond;

    /**
     * @brief Default constructor that sets the Time to 0
     */
    inline Time() noexcept : nanoseconds(0) {}
    /**
     * @brief Default copy constructor
     */
    inline Time(const Time &t) noexcept : nanoseconds(t.nanoseconds) {}
    /**
     * @brief Basic initializer constructor
     * @param nanoseconds value to be set to Time instance expressed in nanoseconds
     */
    inline Time(int64_t ns) noexcept : nanoseconds(ns) {}
    /**
     * @brief Basic assignment operator
     */
    inline Time &operator=(const Time &t) noexcept
    {
        nanoseconds = t.nanoseconds;
        return *this;
    }
    /**
     * @brief Basic assignment operator
     */
    inline Time &operator=(const Null &) noexcept
    {
        nanoseconds = 0;
        return *this;
    }
    /**
     * @brief Basic comparison operator
     * @param t value to be compared to
     * @return true if equal, false otherwise
     */
    inline bool operator==(const Time &t) const noexcept { return nanoseconds == t.nanoseconds; }
    /**
     * @brief Basic comparison operator with Null
     * @param Null value
     * @return true if equal to Null (0), false otherwise
     */
    inline bool operator==(const Null &) const noexcept { return nanoseconds == 0; }
    /**
     * @brief Basic inequality operator
     * @param t value to be compared to
     * @return true of not equal, false otherwise
     */
    inline bool operator!=(const Time &t) const noexcept { return nanoseconds != t.nanoseconds; }
    /**
     * @brief Basic inequality operator with Null
     * @param Null value
     * @return true of not equal to Null (0), false otherwise
     */
    inline bool operator!=(const Null &) const noexcept { return nanoseconds != 0; }
    /**
     * @brief Basic greater than operator
     * @param t value to be compared to
     * @return true if strictly greater than t, false otherwise
     */
    inline bool operator>(const Time &t) const noexcept { return nanoseconds > t.nanoseconds; }
    /**
     * @brief Basic less than operator
     * @param t value to be compared to
     * @return true if strictly less than t, false otherwise
     */
    inline bool operator<(const Time &t) const noexcept { return nanoseconds < t.nanoseconds; }
    /**
     * @brief Basic greater or equal than operator
     * @param t value to be compared to
     * @return true if greater or equal than t, false otherwise
     */
    inline bool operator>=(const Time &t) const noexcept { return operator==(t) || operator>(t); }
    /**
     * @brief Basic less or equal than operator
     * @param t value to be compared to
     * @return true if less or equal than t, false otherwise
     */
    inline bool operator<=(const Time &t) const noexcept { return operator==(t) || operator<(t); }
    /**
     * @brief Basic additive operator
     * @param t value to be added
     * @return Addition result
     */
    inline Time operator+(const Time &t) const noexcept { return Time(nanoseconds + t.nanoseconds); }
    /**
     * @brief Basic substraction operator
     * @param t value to be removed
     * @return Substraction result
     */
    inline Time operator-(const Time &t) const noexcept { return Time(nanoseconds - t.nanoseconds); }
    /**
     * @brief Get the second part of the time
     * Example: if toNanosecond() returns 1477560958714123456, getSeconds() will return 1477560958
     * @return second part of the time
     */
    inline int32_t extractSeconds() const noexcept { return (int32_t)(nanoseconds / (int64_t)1000000000); }
    /**
     * @brief Get the milliseconds part of the time
     * Example: if toNanosecond() returns 1477560958714123456, getMilliseconds() will return 714
     * @return millisecond part of the time
     */
    inline int32_t extractMilliseconds() const noexcept
    {
        return (int32_t)((nanoseconds % (int64_t)1000000000) / 1000000);
    }
    /**
     * @brief Get the microseconds part of the time
     * Example: if toNanosecond() returns 1477560958714123456, getMicroseconds() will return 123
     * @return microsecond part of the time
     */
    inline int32_t extractMicroseconds() const noexcept { return (int32_t)((nanoseconds % (int64_t)1000000) / 1000); }
    /**
     * @brief Get the nanoseconds part of the time
     * Example: if toNanosecond() returns 1477560958714123456, getNanoseconds() will return 456
     * @return nanoseconds part of the time
     */
    inline int32_t extractNanoseconds() const noexcept { return (int32_t)(nanoseconds % (int64_t)1000); }
    /**
     * @brief Get time expressed as a 64 bytes integer representing nanoseconds
     * @return int64_t nanoseconds
     */
    inline int64_t toNanosecond() const noexcept { return nanoseconds; }

  protected:
    int64_t nanoseconds;
};

/**
 * @brief Date and Time representation
 * Represents time, day of the week and date
 *
 * <b>localtime_r system-primitive is used, it is recommended to call tzset first in your application (see localtime_r
 * man-page).</b>
 */
class DateTime : public Time
{
  public:
    enum DayOfTheWeekEnum
    {
        SUNDAY = 0,
        MONDAY,
        TUESDAY,
        WEDNESDAY,
        THURSDAY,
        FRIDAY,
        SATURDAY
    };
    /**
     * @brief Default constructor that sets date to 0
     */
    inline DateTime() noexcept : utcYear(0),
                                 utcMonth(0),
                                 utcDayOfTheWeek(0),
                                 utcDayOfTheMonth(0),
                                 utcDayOfTheYear(0),
                                 utcHour(0),
                                 utcMinute(0),
                                 utcSecond(0),
                                 localYear(0),
                                 localMonth(0),
                                 localDayOfTheWeek(0),
                                 localDayOfTheMonth(0),
                                 localDayOfTheYear(0),
                                 localHour(0),
                                 localMinute(0)
    {
    }
    
    DateTime &operator=(const DateTime &t) noexcept
    {
        Time::operator=(t);
        utcYear = t.utcYear;
        utcMonth = t.utcMonth;
        utcDayOfTheWeek = t.utcDayOfTheWeek;
        utcDayOfTheMonth = t.utcDayOfTheMonth;
        utcDayOfTheYear = t.utcDayOfTheYear;
        utcHour = t.utcHour;
        utcMinute = t.utcMinute;
        utcSecond = t.utcSecond;
        localYear = t.localYear;
        localMonth = t.localMonth;
        localDayOfTheWeek = t.localDayOfTheWeek;
        localDayOfTheMonth = t.localDayOfTheMonth;
        localDayOfTheYear = t.localDayOfTheYear;
        localHour = t.localHour;
        localMinute = t.localMinute;
        return *this;
    }
    
    /**
     * @brief Get utc year expressed in unsigned int year
     * @return year in unsigned 32 bytes
     */
    inline uint32_t getUTCYear() const noexcept { return utcYear; }
    /**
     * @brief Get utc month expressed in unsigned int year
     * @return month in unsigned 8 bytes
     */
    inline uint8_t getUTCMonth() const noexcept { return utcMonth; }
    /**
     * @brief Get utc day of the week
     * @return DayOfTheWeekEnum
     */
    inline DayOfTheWeekEnum getUTCDayOfTheWeek() const noexcept { return (DayOfTheWeekEnum)utcDayOfTheWeek; }
    /**
     * @brief Get utc day of the month
     * @return the day of the month in unsigned 8 bytes
     */
    inline uint8_t getUTCDayOfTheMonth() const noexcept { return utcDayOfTheMonth; }
    /**
     * @brief Get utc day of the year
     * @return the day of the week in unsigned 16 bytes
     */
    inline uint16_t getUTCDayOfTheYear() const noexcept { return utcDayOfTheYear; }
    /**
     * @brief Get utc hour
     * @return hour in unsigned 8 bytes
     */
    inline uint8_t getUTCHour() const noexcept { return utcHour; }
    /**
     * @brief Get utc minute
     * @return minute in unsigned 8 bytes
     */
    inline uint8_t getUTCMinute() const noexcept { return utcMinute; }
    /**
     * @brief Get utc second
     * @return second in unsigned 8 bytes
     */
    inline uint8_t getUTCSecond() const noexcept { return utcSecond; }
    /**
     * @brief Get local year
     * @return year in unsigned 32 bytes
     */
    inline uint32_t getLocalYear() const noexcept { return localYear; }
    /**
     * @brief Get local month
     * @return month in unsigned 8 bytes
     */
    inline uint8_t getLocalMonth() const noexcept { return localMonth; }
    /**
     * @brief Get local day of the week
     * @return day of the week in DayOfTheWeekEnum
     */
    inline DayOfTheWeekEnum getLocalDayOfTheWeek() const noexcept { return (DayOfTheWeekEnum)localDayOfTheWeek; }
    /**
     * @brief Get local day of the month
     * @return day of the month in unsigned 8 bytes
     */
    inline uint8_t getLocalDayOfTheMonth() const noexcept { return localDayOfTheMonth; }
    /**
     * @brief Get local day of the year
     * @return day of the year in unsigned 16 bytes
     */
    inline uint16_t getLocalDayOfTheYear() const noexcept { return localDayOfTheYear; }
    /**
     * @brief Get local hour
     * @return Hour in unsigned 8 bytes
     */
    inline uint8_t getLocalHour() const noexcept { return localHour; }
    /**
     * @brief Get local minute
     * @return Minute in unsigned 8 bytes
     */
    inline uint8_t getLocalMinute() const noexcept { return localMinute; }
    /**
     * @brief Get local second
     * @return Second in unsigned 8 bytes
     */
    inline uint8_t getLocalSecond() const noexcept { return utcSecond; }

  private:
    friend DateTime timeGetEpoch();
    template <class> friend class Accessor;

    uint32_t utcYear;
    uint8_t utcMonth;
    uint8_t utcDayOfTheWeek;
    uint8_t utcDayOfTheMonth;
    uint16_t utcDayOfTheYear;
    uint8_t utcHour;
    uint8_t utcMinute;
    uint8_t utcSecond;
    uint32_t localYear;
    uint8_t localMonth;
    uint8_t localDayOfTheWeek;
    uint8_t localDayOfTheMonth;
    uint16_t localDayOfTheYear;
    uint8_t localHour;
    uint8_t localMinute;

    inline DateTime(time_t epochSecond, uint32_t millisecond)
        : Time((int64_t)epochSecond * (int64_t)1000000000 + (int64_t)millisecond * (int64_t)1000000)
    {
        struct tm vtm;
        struct tm *ptm;
        ptm = gmtime_r(&epochSecond, &vtm);
        assert(ptm == &vtm);
        utcYear = (uint32_t)(ptm->tm_year + 1900);
        utcMonth = (uint8_t)(ptm->tm_mon + 1);
        utcDayOfTheWeek = (uint8_t)ptm->tm_wday;
        utcDayOfTheMonth = (uint8_t)ptm->tm_mday;
        utcDayOfTheYear = (uint16_t)(ptm->tm_yday + 1);
        utcHour = (uint8_t)ptm->tm_hour;
        utcMinute = (uint8_t)ptm->tm_min;
        utcSecond = (uint8_t)ptm->tm_sec;
        ptm = localtime_r(&epochSecond, &vtm);
        assert(ptm == &vtm);
        localYear = (uint32_t)(ptm->tm_year + 1900);
        localMonth = (uint8_t)(ptm->tm_mon + 1);
        localDayOfTheWeek = (uint8_t)ptm->tm_wday;
        localDayOfTheMonth = (uint8_t)ptm->tm_mday;
        localDayOfTheYear = (uint16_t)(ptm->tm_yday + 1);
        localHour = (uint8_t)ptm->tm_hour;
        localMinute = (uint8_t)ptm->tm_min;
        assert(utcSecond == (uint8_t)ptm->tm_sec);
    }
};
#pragma pack(pop)
/**
 * 	@brief Struct used to store the second value
 */
struct Time::Second : Time
{
    inline Second(const Time &other) noexcept : Time(other) {}
    inline Second(int32_t pSeconds) noexcept : Time((int64_t)pSeconds *(int64_t)1000000000) {}
};
/**
 * 	@brief Struct used to store the millisecond value
 */
struct Time::Millisecond : Time
{
    inline Millisecond(const Time &other) noexcept : Time(other) {}
    inline Millisecond(int32_t pMilliseconds) noexcept : Time((int64_t)pMilliseconds *(int64_t)1000000) {}
};
/**
 * 	@brief Struct used to store the microsecond value
 */
struct Time::Microsecond : Time
{
    inline Microsecond(const Time &other) noexcept : Time(other) {}
    inline Microsecond(int64_t pMicroseconds) noexcept : Time(pMicroseconds *(int64_t)1000) {}
};
/**
 * 	@brief Struct used to store the nanosecond value
 */
struct Time::Nanosecond : Time
{
    inline Nanosecond(const Time &other) noexcept : Time(other) {}
    inline Nanosecond(int64_t pNanoseconds) noexcept : Time(pNanoseconds) {}
};
/**
 * @brief Basic insertion operator <<
 * @param s ostream, t Second
 * @return ostream containing number of seconds
 */
inline std::ostream &operator<<(std::ostream &s, const Time::Second &t) { return s << t.extractSeconds() << 's'; }
/**
 * @brief Basic insertion operator <<
 * @param s ostream, t Millisecond
 * @return ostream containing number of millisecond
 */
inline std::ostream &operator<<(std::ostream &s, const Time::Millisecond &t)
{
    return s << (t.toNanosecond() / 1000000) << "ms";
}
/**
 * @brief Basic insertion operator <<
 * @param s ostream, t Microsecond
 * @return ostream containing number of microsecond
 */
inline std::ostream &operator<<(std::ostream &s, const Time::Microsecond &t)
{
    return s << (t.toNanosecond() / 1000) << "micros";
}
/**
 * @brief Basic insertion operator <<
 * @param s ostream, t Nanosecond
 * @return ostream containing number of nanosecond
 */
inline std::ostream &operator<<(std::ostream &s, const Time::Nanosecond &t) { return s << t.toNanosecond() << "ns"; }

} // namespace