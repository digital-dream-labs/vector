# Victor Fault Reports

Created by David Mudie Last updated Mar 01, 2019

This page provides a brief description of Victor's fault reporting.

## Fault Code Handler
Robot fault codes are handled by a custom script 'fault-code-handler':

  https://github.com/anki/vicos-oelinux/tree/master/anki/fault-code

The behavior of this script is controlled by configuration flags set in /anki/etc/fault-code-handler.env on the robot.

If log and/or trace uploads are enabled, the handler script will capture the tail of /var/log/messages and/or recent trace events and bundle them into a gzipped tar file.

The file will be named 'fault-nnn-yyyymmdd-hhmmss.tar.gz' where nnn is the fault code being handled, yyyymmdd is the current local date, and hhmmss is the current local time on the robot.

Note that date and time are determined by the robot's system clock! This doesn't always match up with UTC or NTP time servers.

## Fault Report Upload
Report files will be placed in /data/data/com.anki.victor/cache/outgoing to be uploaded by vic-log-uploader when services are restarted. 

Note that the upload service requires a working internet connection and valid authorization to transfer files from the robot to Anki cloud storage!

If the robot is unable to upload a fault report, the report will stay in the outgoing directory until it is removed to free space.

## Fault Report Storage
Fault reports are stored in AWS S3 buckets as described here:

Service: Device Log Storage

Developers can use aws-okta and sai-go-cli to retrieve specific report files from AWS S3 storage:

```
#!/usr/bin/env bash
$ aws-okta exec vector-logs -- sai-go-cli device-logs download-url <Insert URL Here>
 
INFO[0001] Requesting MFA. Please complete two-factor authentication with your second device
INFO[0001] Select a MFA from the following list        
INFO[0001] 0: OKTA (push)                              
INFO[0001] 1: OKTA (token:software:totp)               
Select MFA method: 0
Downloading 2019-03-01-00-54-09-fault-917-20190228-165047.tar.gz: Done.
```

### Fault Report Format
The tar.gz format can be unpacked with a command like

```
#!/usr/bin/env bash
$ cd ~/Downloads
$ tar xfzp fault-914-20190208-172020.tar.gz
```

This will create a directory containing log and trace files captured by the handler:

```
#!/usr/bin/env bash
$ cd fault-914-20190208-172020
$ ls
ankitrace-20190208-172009-0
messages.log
```

The first item (ankitrace-20190208-172009-0) is a directory containing LTTNG trace files. These files can be viewed with an LTTNG viewer such as babeltrace or Trace Compass.

The second item (messages.log) is a plain-text dump of /var/log/messages. This file can be viewed with standard utilities such more, vim, and the OSX console app.

## LTTNG
LTTNG is an open-source tracing framework for Linux, as described here:

https://lttng.org/

For victor, non-shipping builds have LTTNG enabled to capture kernel and user-space trace events. These events provide a window to show what was happening on the robot at the time a fault code was reported.

## babeltrace
Babeltrace is a plain-text LTTNG viewer, as described here:

https://diamon.org/babeltrace/

Babeltrace is not distributed for OSX at this time. You can download the source and build it yourself:

```
#!/usr/bin/env bash
# Fetch build requirements
$ brew install wget
$ brew install popt
 
# Fetch source
$ wget https://www.efficios.com/files/babeltrace/babeltrace-1.5.3.tar.bz2
$ tar xfjp babeltrace-1.5.3.tar.bz2
$ cd babeltrace-1.5.3
 
# Build package
$ ./configure
$ make
$ sudo make install
```

## Trace Compass
Trace Compass is a GUI LTTNG viewer, as described here:

https://www.eclipse.org/tracecompass/

You can download a prebuilt package for OSX from this web site.