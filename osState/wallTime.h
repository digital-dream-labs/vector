/**
 * File: wallTime.h
 *
 * Author: Brad Neuman
 * Created: 2018-06-11
 *
 * Description: Utilities for getting wall time
 *
 * Copyright: Anki, Inc. 2018
 *
 **/

#ifndef __Engine_WallTime_H__
#define __Engine_WallTime_H__

#include "util/singleton/dynamicSingleton.h"

#include <chrono>

// forward declare time struct (see <ctime>)
struct tm;

namespace Anki {
namespace Vector {

class WallTime : public Util::DynamicSingleton<WallTime>
{
  ANKIUTIL_FRIEND_SINGLETON(WallTime);

public:
  ~WallTime();

  using TimePoint_t = std::chrono::time_point<std::chrono::system_clock>;

  // NOTE: None of these timers are monotonic or steady. They are all based on system time which can be set via
  // NTP or changed (potentially by the user)

  // If time is synchronized (reasonably accurate), fill the passed in unix time struct with the current local
  // time and return true. Otherwise, return false. Note that if a timezone is not set (see
  // OSState::HasTimezone()), UTC is the default on vic-os
  bool GetLocalTime(struct tm& localTime);

  // If the time is synchronized (reasonably accurate), fill the passed in unix time struct with the current
  // time in UTC and return true. Otherwise, return false.
  bool GetUTCTime(struct tm& utcTime);

  // If the time is synchronized, set the chrono timepoint and return true, otherwise return false
  bool GetTime(TimePoint_t& time);

  // If the time is _not_ synchronized since boot (e.g. we aren't on wifi) and/or we don't know the timezone,
  // we can still get an approximate UTC time. Note that this may be arbitrarily behind the real time, e.g. if
  // the robot has been off wifi (or the NTP servers are down for some reason) for a year, this time may be a
  // year behind. Returns false if there's an internal error, true if it set the time
  bool GetApproximateUTCTime(struct tm& utcTime);

  // Set the approximate local time regardless of synchronization and return true (false if error). Note that
  // similat to GetLocalTime(), vicos will default to UTC if no timezone is set (see OSState::HasTimezone())
  bool GetApproximateLocalTime(struct tm& localTime);

  // Get the chrono timepoint regardless of sync (may be inaccurate, as above)
  TimePoint_t GetApproximateTime();

  // return the epoch time (for comparison with other TimePoint_t times).
  // Note: This is _not_ time since epoch, but rather the TimePoint_t corresponding to time 0 of the Unix epoch (i.e.
  // 00:00:00 Jan 1 1970)
  TimePoint_t GetEpochTime();

  ////////////////////////////////////////////////////////////////////////////////
  // Helpers for dealing with time points
  ////////////////////////////////////////////////////////////////////////////////

  // uses local time if possible, otherwise falls back to UTC. Checks if the time points are in the same day
  // or different days (rolling over at midnight)
  static bool AreTimePointsInSameDay(const TimePoint_t& a, const TimePoint_t& b);

private:

  WallTime();

  bool IsTimeSynced();
  
  // checking for time sync is a syscall, so avoid doing it too often by keeping a cache and refreshing based
  // on a different timer
  float _lastSyncCheckTime = -1.0f;
  bool _wasSynced = false;

};

}
}


#endif
