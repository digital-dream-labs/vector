# Victor Logging

Note: The best source of information about our various logging macros is [logging.h](/lib/util/source/anki/util/logging/logging.h) in Util.

## Logging isn't free

Writing a log statement costs system resources since it involves IPC messaging to a logging process. Excessive logging hogs resources and causes system-wide slowdown, resulting in subpar robot performance.

Log statements should thus never be spammy or repetitive, and should be used thoughtfully and sparingly. They should be well named so that anyone (developers or otherwise) can quickly understand what's happening and why it's worthy of a log statement.

## Log levels

It's important to select the appropriate log level for your message. Here are some rough guidelines:

- `ERROR` indicates that something has gone so wrong that your module cannot continue normal execution anymore. In [Webots tests](/simulator/controllers/webotsCtrlBuildServerTest/README.md), any error message appearing in the log will cause the test to fail.
- `WARNING` indicates a problem has occurred that users/developers/QA need to know about. A warning points to a situation that _should_ not happen and that should be fixed, but is not severe enough to warrant an error.
- `INFO` is for everyone to see. It should contain information useful to non-developers.
- `DEBUG` is for development. Debug statements include technical information or raw data not valuable to anyone except the developer of the module and other stakeholders in that module.

Note that `DEBUG` log statements get compiled out in `release` and `shipping` build configurations.

## Log channels

Log channels are simple strings that provide a facility to filter logs, enabling select channels and disabling others based on the type of problem you are interested in debugging. Each high-level code module will usually use its own log channel (e.g. `Behaviors`, `Actions`, `BlockWorld`, `Planner`).

`PRINT_NAMED_INFO` will automatically use the event as a channel for your convenience, normal filtering rules apply.

Note: Log channels apply only to debug and info levels. Warning and error levels are always `NAMED`.

## Best practices

The preferred way to write logs is to declare a log channel using `#define LOG_CHANNEL "MyLogChannel"`, and use the `LOG_DEBUG` and `LOG_INFO` macros. Here's an example:

```cpp
  // myFile.cpp

  #define LOG_CHANNEL "MyLogChannel"

  // ...

  // LOG_DEBUG will print a debug message using the defined LOG_CHANNEL above.
  // No duplication of channel name!
  LOG_DEBUG("SomeDebugStuff", "Some useful information for developers: ...");

  LOG_DEBUG("MoreDebugStuff", "More useful info");

  LOG_INFO("SomeInfo", "An event happened that everyone should know about");
```

## Enabling

To change which channels are logged at runtime use consolevars exposed through webservices:

[Channels console vars](images/HOW-channels.png)

Note: these can be persisted through the `LOAD console vars` and `SAVE console vars` buttons.

## Logging system details

### liblog

Anki processes (and many system processes) log messages using the Android log interface `liblog`.
On VicOS robots, the `liblog` library is modified to forward all messages to `syslog`. DAS messages
(anything with a leading '@') are also sent to `logd` to be read and processed by the `vic-dasmgr` application.

Log messages are sent over a domain socket to the userspace service where they are stored in memory much like `syslog`.

### syslog

Messages from `systemd` and the OS kernel are processed by `journald` and then forwarded to `syslog`.
Other native Linux applications use `syslog` directly.

Messages sent to `syslog` are processed by `syslog-ng` and stored in `/var/log/messages`.

When the size of `/var/log/messages` reaches 4MB, the `logrotate` service copies and compresses it to `/var/log/messages.1.gz`.

Only a single rotation is saved. Each rotation overwrites older data.  When the robot reboots, messages are lost.

## Related Pages

[Android Logging System](https://tinylab.gitbooks.io/elinux/content/en/android_portal/android_sys_info/Android_Logging_System/Android_Logging_System.html)

[Wikipedia syslog-ng](https://en.wikipedia.org/wiki/Syslog-ng)
