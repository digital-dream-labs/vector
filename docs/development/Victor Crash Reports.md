# Victor Crash Reports

Created by David Mudie Feb 05, 2019

This page provides an overview of Victor crash report facilities.

## Victor Crash Reporter

Robot services link with a support library https://github.com/anki/victor/tree/master/platform/victorCrashReports to enable crash reports.

## Victor Crash Signals
The crash report library installs handlers for the following signals: SIGILL, SIGABRT, SIGBUS, SIGSEGV, SIGFPE, and SIGQUIT.

If a service process receives one of these signals, it will create a crash dump in /data/data/com.anki.victor/cache/crashDumps on the robot.

The type of signal indicates the type of crash:

* SIGILL - illegal instruction
* SIGABRT - voluntary abort, assertion failure, or unhandled exception
* SIGBUS - memory bus or alignment error
* SIGSEGV - segmentation violation, e.g. invalid memory reference or null pointer crash
* SIGFPE - floating point exception, e.g. math error or division by zero
* SIGQUIT - application quit requested by controlling terminal, e.g. control-backslash

Note that Victor makes special use of SIGILL and SIGQUIT to force crash dumps when a fault code occurs.

## Symbolication
Crash stacks will contain a mix of hex addresses, function names, and function offsets because debug symbols are stripped when executables are built.

Victor cmake projects use a helper macro 'anki_build_strip' that creates a '.full' file containing debug symbols from each executable. You can comment out these calls for specific executables, but this will increase the size of files that must be deployed to the robot.

You can decorate a crash stack with symbolic names IF you have access to the '.full' files produced from your executables.

See Victor FAQ for more information.

## Debugger Tombstones
Debugger tombstones are created by a helper process (Android Debugger Daemon) on the robot.

Tombstone files are created in /data/tombstones on the robot.

A summary of each crash appears in logcat when a tombstone is created.

## victor-show-tombstone
Use victor-show-tombstone.py to fetch and symbolicate a tombstone report:

```
#!/usr/bin/env bash
cd project/victor/scripts
./victor-show-tombstone.py -c Release tombstone_00
```

### No symbols in your tombstone for the lib or bin in question?
Copy your local build's version of the library file that you want symbols on (referred to as libNAME) from build host to robot:



ON THE ROBOT:

```
/usr/bin/env bash
# mount -o rw,remount /
```

ON THE BUILD HOST:

```
/usr/bin/env bash
cd _build/vicos/Release/lib/
scp ./libNAME.so.full root@ROBOTIP:/anki/lib/libNAME.so
```

Now reproduce the crash, and inspect the new tombstone.

Note: change lib to bin in the above steps to copy over a binary with symbols (e.g. vic-engine or vic-anim)

Important: if copying over a binary, there won't be symbols for the shared libraries

(See also how-do-i-get-symbols-for-the-trace in Victor FAQ.)

## Google Breakpad
Crash dumps are created by an exception handler from Google Breakpad.

Crash dumps are created in /data/data/com.anki.victor/cache/crashDumps on the robot.

A summary of each crash appears in logcat when a dump file is created.

## victor-show-minidump
Use victor-show-minidump.py to fetch and symbolicate a minidump:

```
#!/usr/bin/env bash
cd project/victor/scripts
./victor-show-minidump.py -c Release vic-dasmgr-2018-06-13T06-12-22-425.dmp.uploaded
```

See https://github.com/anki/victor/blob/master/docs/development/crash-reports.md for more information.

## Backtrace
Crash reports from TeamCity builds (build ID > 0) are automatically uploaded to  Backtrace.

Crash reports are tagged with ESN, hostname, OS version, and software version.

You can identify the owner of a robot by searching for the ESN on EZ Office Inventory.

## Related Pages
https://github.com/anki/victor/blob/master/docs/development/crash-reports.md 