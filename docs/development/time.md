# Time, Clocks, and Timers on Victor

There are several different concepts of time that are useful on Victor. This page is written from an engine
perspective, and doesn't get too far into the details of how things work at the OS level

## Background

The robot runs embedded linux and has a clock, but lacks the ability to power that clock when the robot is
off. This means that, on boot, the time and date will be wrong (too old) until the robot has a chance to
connect to wifi and perform an NTP sync. This sync can cause the wall time to jump. The expected use case
means that this should happen early in the boot, but we still need to be robust to edge cases where the robot
boots without wifi.

Before any of the vic-* processes are running, the OS will set the time to something relatively recent (2018
at least).

The OS also has the notion of a timezone, which can be set via settings in the app, or via the engine
webserver (8888), consolevars -> RobotSettings -> DebugSetTimeZone. Enter the timezone with no spaces, in
the standard format, e.g. America/Los_Angeles.  You can also set it manually by ssh'ing into the robot and 
running:
```
timedatectl set-timezone America/Los_Angeles
```

## Definitions

* _wall time_ - The current time and date. This could be in UTC or local time and requires a time sync to be
  accurate. Named as such because it should match the time on the calendar / clock on a wall in the real world
* _steady time_ - A timer that continually counts up, but may reset on boot
* _basestation time_ / _tick time_ - A monotonic timer that returns the same value for any call during a tick
* _timestamp_ - We often use timestamp to refer to a time in milliseconds sent from robot firmware

## Which kind of time do I want?

You can probably use [`BaseStationTimer`](../../coretech/common/engine/utils/timer.h) for the majority of your
needs if you just need to count seconds since things happened. If not, peruse the following:

* Do you need the time to be consistent across boots (it can't reset)? E.g. you are tracking how many days
  it's been since you've seen a face
    * Do you care about local time of day? If yes, use [`WallTime::GetLocalTime()`](../../osState/wallTime.h)
    * Otherwise, prefer [`WallTime::GetUTCTime()`](../../osState/wallTime.h) to get UTC time so that you don't
      have to worry about time zone jumps causing issues
    * If you want wall time but are OK with it being too old (because it couldn't sync) you can use the
      approximate versions in [wallTime.h](../../osState/wallTime.h)
    * If you want wall time in an std::chrono format (convenient for comparisons and storage), you can use
      [`WallTime::GetTime()`](../../osState/wallTime.h)
    * *Warning*: time and timezone can change out from under you during operation, so be careful. Local time
      can jump backwards if the user changes timezone!

* Do you plan to compare this time to an image or robot pose history? E.g. you need to know exactly where the
  robot was at this time to work out some pose math
    * Use [`Robot::GetLastImageTimeStamp()`](../../engine/robot.h),
      [`Robot::GetLastMsgTimestamp()`](../../engine/robot.h) or equivalent

* Do you plan to check this value multiple times in a tick and expect a consistent result? E.g. you want
  timeout logic checked at the beginning of the tick to match when it's checked at the end, or you don't want
  your code to be tick-order dependent
    * Use
      [`BaseStationTimer::getInstance()->GetCurrentTimeInSeconds()`](../../coretech/common/engine/utils/timer.h)
      or
      [`BaseStationTimer::getInstance()->GetCurrentTimeInNanoSeconds()`](../../coretech/common/engine/utils/timer.h)
    * This uses `steady_clock` behind the scenes, so it should reflect the 'real' time, but it is only the real time at the beginning of the tick. So within a single tick, this may lag behind real time if engine is running slow
    * This is likely the most _efficient_ timer to use because it only calculates anything once per tick, so it a good default

* Do you want a monotonic timer that tells you the time _right now_ within a tick? E.g. if you are computing
  exact timing on when a light should turn on
    * Use [`UniversalTime`](../../lib/util/source/anki/util/time/universalTime.h) (for traditional clock
      values in seconds, millis, or nanoseconds) or
      [`std::chrono::steady_clock`](https://en.cppreference.com/w/cpp/chrono/steady_clock) (for the more
      modern C++ chrono code)

* Do you need highly accurate time to measure performance? Use
  [`UniversalTime::GetNanosecondsElapsedSince`](../../lib/util/source/anki/util/time/universalTime.h) or
  [`std::chrono::high_resolution_clock`](https://en.cppreference.com/w/cpp/chrono/high_resolution_clock)

* Do none of these timers meet your use case? Figure out which timer you should use and update this readme!
