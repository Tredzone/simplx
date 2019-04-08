/**
 * @file testtime.cpp
 * @brief test custom time class
 * @copyright 2013-2019 Tredzone (www.tredzone.com). All rights reserved.
 * Please see accompanying LICENSE file for licensing terms.
 */

#include <limits>

#include "gtest/gtest.h"

#include "trz/engine/internal/time.h"
#include "trz/engine/platform.h"

#include "testutil.h"

using namespace tredzone;

TEST(Time, testAll)
{
    tzset();
    tredzone::DateTime dateTime = tredzone::timeGetEpoch();
    time_t epochSecond = dateTime.extractSeconds();

    struct tm *ptm;
    ptm = gmtime(&epochSecond);
    ASSERT_EQ(dateTime.getUTCYear(), (uint32_t)(ptm->tm_year + 1900));
    ASSERT_EQ(dateTime.getUTCMonth(), (uint8_t)(ptm->tm_mon + 1));
    ASSERT_EQ(dateTime.getUTCDayOfTheWeek(), (uint8_t)ptm->tm_wday);
    ASSERT_EQ(dateTime.getUTCDayOfTheMonth(), (uint8_t)ptm->tm_mday);
    ASSERT_EQ(dateTime.getUTCDayOfTheYear(), (uint16_t)(ptm->tm_yday + 1));
    ASSERT_EQ(dateTime.getUTCHour(), (uint8_t)ptm->tm_hour);
    ASSERT_EQ(dateTime.getUTCMinute(), (uint8_t)ptm->tm_min);
    ASSERT_EQ(dateTime.getUTCSecond(), (uint8_t)ptm->tm_sec);

    ptm = localtime(&epochSecond);
    ASSERT_EQ(dateTime.getLocalYear(), (uint32_t)(ptm->tm_year + 1900));
    ASSERT_EQ(dateTime.getLocalMonth(), (uint8_t)(ptm->tm_mon + 1));
    ASSERT_EQ(dateTime.getLocalDayOfTheWeek(), (uint8_t)ptm->tm_wday);
    ASSERT_EQ(dateTime.getLocalDayOfTheMonth(), (uint8_t)ptm->tm_mday);
    ASSERT_EQ(dateTime.getLocalDayOfTheYear(), (uint16_t)(ptm->tm_yday + 1));
    ASSERT_EQ(dateTime.getLocalHour(), (uint8_t)ptm->tm_hour);
    ASSERT_EQ(dateTime.getLocalMinute(), (uint8_t)ptm->tm_min);
}