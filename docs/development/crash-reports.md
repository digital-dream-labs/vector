# Victor Crash Reports

Victor has crash reporting functionality that uses Google Breakpad.  The Google Breakpad client is built for VICOS and is integrated into vic-engine, vic-anim, vic-robot, vic-switchboard and vic-webserver.  We have NOT yet implemented crash reporting for vic-cloud as this requires Go language support.  We also don't use this for kernel crashes.

## Where to find crash report files

When a crash occurs you'll see something like this in the log:

```
05-16 17:31:26.051  1777  1810 I vic-engine: [@GoogleBreakpad] GoogleBreakpad.DumpCallback: (tc88469) : Dump path: '/data/data/com.anki.victor/cache/crashDumps/engine-1970-01-01T00-00-14-595.dmp', fd = 3, context = (nil), succeeded = true
05-16 17:31:26.084  1776  1776 I vic-engine: vic-engine terminated by signal 11
```

The crash report file is found on the robot at `/data/data/com.anki.victor/cache/crashDumps/` and has a name like `engine-1970-01-01T00-00-14-595.dmp` where the prefix is the name of the process that crashed, and the date/time stamp is when the process *started*.  The file is in standard "minidump" format.

When Victor is running you may see some of these files with a size of zero.  That's because we open the file when the process starts.  When Victor shuts down normally those files should disappear.

## Automatic uploading/deletion

When victor is running on the robot, every 60 seconds a process called vic-crashuploader runs.  It looks for files ending in ".dmp" in the crash folder that have a non-zero size and are not held by any process.  If running a local victor build, the file is simply renamed to append ".local" to the filename.  If not (e.g. running a build that came from TeamCity), it first attempts to upload the crash dump file to Backtrace IO, our third-party crash reporting service.  If the upload succeeds the file is then renamed to append ".uploaded" to the filename. vic-crashuploader also deletes all but the newest 50 ".uploaded" and/or ".local" files on the robot to give developers a chance to examine them, without filling up robot storage.

## Backtrace IO and symbolication of call stacks (WIP)

The URL for our Backtrace IO account is https://anki.sp.backtrace.io/dashboard/anki and the project name is 'victor'.  See Jane Fraser for an account if you need one.

On the TeamCity build server for victor builds, a build step automatically generates the symbols for each build in the proper format, and uploads them to Backtrace IO.  You can then see symbolicated call stacks on Backtrace IO for any crash that occurs on a build for which the symbols have been generated.

Instructions below are for generating symbolicated call stacks for local builds.

## Developer builds and symbolication of call stacks on OSX

To generate call stacks from a developer build you will need to:

1. Install noah
1. Symbol files in breakpad format
1. A crash dump file
1. Execute `minidump_stackwalk`

To install noah on OSX type:

```
brew install linux-noah/noah/noah
```
The first time noah runs, it'll need to prompt you as to whether or not you want to allow it to run as root, so run it manually before ever calling victor-show-minidump.py, or the minidump process will freeze silently.

To generate symbol files, copy the crash dump file and execute `minidump_stackwalk` type:

```
./project/victor/scripts/victor-show-minidump.py -d -c Debug vic-anim-V0-2018-11-14T14-27-50-459.dmp
```

`-d` instructs `victor-show-minidump.py` to generate the symbol files by executing `dump_syms`

`-c Debug` indicates a `Debug` build, switch to `Release` as necessary

`vic-anim-V0-2018-11-14T14-27-50-459.dmp` is the name of a dump file, if it has been handled on the robot it may have the extension `.local` or `.uploaded`, `/data/data/com.anki.victor/cache/crashDumps/` will be prepended automatically.

If you have already copied the dump file from your robot use `-s` to skip the copy.
