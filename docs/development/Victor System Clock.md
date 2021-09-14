# Victor System Clock

Created by David Mudie Last updated Nov 17, 2018

This page is a random collection of information about Victor's system clock. Information presented is neither complete nor authoritative.

## Hardware Clock
Victor's hardware clock does not have battery backup. It resets to 0 (aka unix epoch) after the robot's battery is discharged.

## /etc/timestamp
A marker file /etc/timestamp is created on the root partition as part of the OS build process.  This is the date/time that the OS build was made.

## /data/etc/localtime
This file points to the current time zone.

## chronyd
When Victor has network access, a service process (chronyd) attempts to sync system time with NTP time servers 4 or 5 times per hour.

NTP servers are configured in /etc/chrony.conf. We use a pool of 4 NTP servers in anki.pool.ntp.org.

## Boot Sequence -> Shutdown
When the robot boots, the system clock is loaded from the robot's Real Time Clock (RTC).  The RTC, however, is just a count of seconds that the battery has been active.  It is not the real time at all.  It is something close to 0 (or Jan 1, 1970 (the unix epoch)).

The fake-hwclock service advances the clock to match /etc/timestamp.  At a minimum, we know that the valid time is at least this date/time stamp.

This gives the robot a relatively recent notion of time to enable SSL certificate authentication and such.

The fake-hwclock service then looks in  /data/opt/fake-hwclock/fake-hwclock.data for time information saved during the last boot and, if available, uses it to restore the clock to what should be a relatively sane value.  To note, this file has the last value of the RTC before shutdown.  If the value of the RTC is now greater than this saved value, the difference will be added to the current clock moving it forward and closer to accurate time.  However, if there was a power outage to the RTC, the value will be less than the saved value.  In that case, the value of the RTC is added to the current clock.  However, we have no idea how long the power was out.  Our clock could be 1 minute behind or 1 year.

Shortly after the WiFi interface gets an IP address (about 11 or 12 seconds in my testing), chronyd will get a current timestamp from NTP servers and update the clock.  The clock is now synchronized and considered to be accurate.  Thereafter, chronyd will check the NTP servers 4 or 5 times per hour.

Every 15 minutes, the fake-hwclock service will run and store the current time information and the value of the RTC into /data/opt/fake-hwclock/fake-hwclock.data

At shutdown, the fake-hwclock service will store the current time information and the value of the RTC into /data/opt/fake-hwclock/fake-hwclock.data one last time

As long as the robot has power, the RTC will keep ticking seconds even if the operating system is shutdown.
