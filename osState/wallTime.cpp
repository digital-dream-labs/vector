/**
 * File: wallTime.cpp
 *
 * Author: Brad Neuman
 * Created: 2018-06-11
 *
 * Description: Utilities for getting wall time
 *
 * Copyright: Anki, Inc. 2018
 *
 **/

#include "osState/wallTime.h"

#include "coretech/common/engine/utils/timer.h"
#include "osState/osState.h"
#include "util/console/consoleInterface.h"
#include "util/logging/logging.h"

#include <ctime>
#include <chrono>

namespace Anki {
namespace Vector {

namespace {

// while we think we're synced, re-check every so often
static const float kSyncCheckPeriodWhenSynced_s = 60.0f * 60.0f;

// check more often if we aren't synced (so we get the accurate time after a sync)
static const float kSyncCheckPeriodWhenNotSynced_s = 1.0f;

#if REMOTE_CONSOLE_ENABLED

void PrintWallTimeToLog(ConsoleFunctionContextRef context)
{
  auto* wt = WallTime::getInstance();

  PRINT_NAMED_INFO("WallTime.DEBUG.OSState.IsSynced",
                   "%s",
                   OSState::getInstance()->IsWallTimeSynced() ? "yes" : "no");
  PRINT_NAMED_INFO("WallTime.DEBUG.OSState.HasTimezone",
                   "%s",
                   OSState::getInstance()->HasTimezone() ? "yes" : "no");
  {
    struct tm time;
    bool got = wt->GetUTCTime(time);
    std::string t;
    if( got ) {
      t = "accurate";
    }
    else {
      got = wt->GetApproximateUTCTime(time);
      t = "approximate";
    }

    if( got ) {
      PRINT_NAMED_INFO("WallTime.DEBUG.OSState.UTCTime",
                       "%s: %s",
                       t.c_str(),
                       asctime(&time));
    }
    else {
      PRINT_NAMED_WARNING("WallTime.DEBUG.OSState.UTCTime.FAIL",
                          "could not get time");
    }
  }

  {
    struct tm time;
    bool got = wt->GetLocalTime(time);
    std::string t;
    if( got ) {
      t = "accurate";
    }
    else {
      got = wt->GetApproximateLocalTime(time);
      t = "approximate";
    }

    if( got ) {
      PRINT_NAMED_INFO("WallTime.DEBUG.OSState.LocalTime",
                       "%s: %s",
                       t.c_str(),
                       asctime(&time));
    }
    else {
      PRINT_NAMED_WARNING("WallTime.DEBUG.OSState.LocalTime.FAIL",
                          "could not get time");
    }
  }

}

static int sFakeWallTime = -1;

void SetFakeWallTime24HourUTC(ConsoleFunctionContextRef context)
{
  int time = ConsoleArg_Get_Int32(context, "fakeTime");
  if( time >= 0 &&
      time < 2400 &&
      time % 100 < 60 ) {
    sFakeWallTime = time;
  }
  else {
    PRINT_NAMED_WARNING("WallTime.SetFakeWallTime.InvalidTime",
                        "time %d is invalid, set in 24 hour format in UTC (e.g. 1830)",
                        time);
  }
}

void ClearFakeWallTime(ConsoleFunctionContextRef context)
{
  sFakeWallTime = -1;
}


#endif // REMOTE_CONSOLE_ENABLED

}

#define CONSOLE_GROUP "WallTime"

CONSOLE_FUNC( PrintWallTimeToLog, CONSOLE_GROUP );
CONSOLE_VAR(bool, kFakeWallTimeIsSynced, CONSOLE_GROUP, false);
CONSOLE_FUNC( SetFakeWallTime24HourUTC, CONSOLE_GROUP, int fakeTime );
CONSOLE_FUNC( ClearFakeWallTime, CONSOLE_GROUP );

WallTime::WallTime()
{
}

WallTime::~WallTime()
{
}

bool WallTime::GetLocalTime(struct tm& localTime)
{
  if( !IsTimeSynced() ) {
    return false;
  }

  const bool timeOK = WallTime::GetApproximateLocalTime(localTime);
  return timeOK;
}

bool WallTime::GetUTCTime(struct tm& utcTime)
{
  if( !IsTimeSynced() ) {
    return false;
  }

  const bool timeOK = WallTime::GetApproximateUTCTime(utcTime);
  return timeOK;
}

bool WallTime::GetApproximateUTCTime(struct tm& utcTime)
{
  using namespace std::chrono;
  const time_t now = system_clock::to_time_t(GetApproximateTime());
  tm* utc = gmtime(&now);
  if( nullptr != utc ) {
    utcTime = *utc;
    return true;
  }

  PRINT_NAMED_ERROR("WallTime.UTC.Invalid",
                    "gmtime returned null. Error: %s",
                    strerror(errno));
  return false;
}

bool WallTime::GetApproximateLocalTime(struct tm& localTime)
{
  using namespace std::chrono;
  const time_t now = system_clock::to_time_t(GetApproximateTime());
  tm* local = localtime(&now);
  if( nullptr != local ) {
    localTime = *local;
    return true;
  }

  PRINT_NAMED_ERROR("WallTime.Local.Invalid",
                    "localtime returned null. Erro`r: %s",
                    strerror(errno));
  return false;
}

bool WallTime::GetTime(TimePoint_t& time)
{
  if( !IsTimeSynced() ) {
    return false;
  }

  time = GetApproximateTime();
  return true;
}

WallTime::TimePoint_t WallTime::GetEpochTime()
{
  // default constructor uses epoch time
  return WallTime::TimePoint_t{};
}


WallTime::TimePoint_t WallTime::GetApproximateTime()
{

#if REMOTE_CONSOLE_ENABLED
  if( sFakeWallTime >= 0 ) {
    time_t now = time(nullptr);
    struct tm fake_tm = {0};
    (void) gmtime_r(&now, &fake_tm);
    fake_tm.tm_sec = 0;
    fake_tm.tm_hour = sFakeWallTime / 100;
    fake_tm.tm_min = sFakeWallTime % 100;
    time_t fake_now = timegm(&fake_tm);
    return std::chrono::system_clock::from_time_t(fake_now);
  }
#endif

  return std::chrono::system_clock::now();
}

bool WallTime::IsTimeSynced()
{
  if( kFakeWallTimeIsSynced ) {
    return true;
  }

  // Use base station timer because it's cheap and good enough here, goal is just to not hit the syscall too
  // frequently if this function is called often
  const float currTime_s = BaseStationTimer::getInstance()->GetCurrentTimeInSeconds();

  const float checkPeriod_s = _wasSynced ? kSyncCheckPeriodWhenSynced_s : kSyncCheckPeriodWhenNotSynced_s;
  if( _lastSyncCheckTime < 0 ||
      _lastSyncCheckTime + checkPeriod_s <= currTime_s ) {
    _wasSynced = OSState::getInstance()->IsWallTimeSynced();
    _lastSyncCheckTime = currTime_s;
  }

  return _wasSynced;
}

bool WallTime::AreTimePointsInSameDay(const TimePoint_t& a, const TimePoint_t& b)
{
  tm aTime;
  tm bTime;

  {
    const time_t a_tt = std::chrono::system_clock::to_time_t(a);
    tm* a_tm = localtime(&a_tt);
    if( a_tm == nullptr ) {
      PRINT_NAMED_ERROR("WallTime.AreTimePointsInSameDay.NoLocalTime.ArgA",
                        "Cant get local time for first argument");
      // need to return something, possibly better to assume it's the same day to avoid a big reaction or stats
      // bump
      return true;
    }
    else {
      aTime = *a_tm;
    }
  }

  {
    const time_t b_tt = std::chrono::system_clock::to_time_t(b);
    tm* b_tm = localtime(&b_tt);
    if( b_tm == nullptr ) {
      PRINT_NAMED_ERROR("WallTime.AreTimePointsInSameDay.NoLocalTime.ArgB",
                        "Cant get local time for second argument");
      // need to return something, possibly better to assume it's the same day to avoid a big reaction or stats
      // bump
      return true;
    }
    else {
      bTime = *b_tm;
    }
  }

  const bool sameDay = ( aTime.tm_yday == bTime.tm_yday );
  return sameDay;
}

}
}
