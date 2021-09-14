# Frequently Asked Questions

If you have a question that you get answered (e.g. in a Slack channel) which might plague others, consider creating an entry here for it.

Additional troubleshooting can be found on [Confluence](https://ankiinc.atlassian.net/wiki/spaces/ATT/pages/362807304/Victor+Build+Troubleshooting).

### How do I see what's happening on my robot?

* Run `victor_log` from a terminal. If you would like to simultaneously save the log output to file, run `victor_log | tee log.txt`.
* Try using the webserver on the robot. In a browser, type `<my_victor_ip_address>:8888` in the address bar, where `<my_victor_ip_address>` is the IP address of your robot. Change the `8888` to an `8889` to see anim process information. You can open webviz, an interactive status page, by clicking the `WebViz` link on the web server landing page. More information [here](/docs/development/web-server.md).

### I can't connect or ping Victor, he doesn't seem to be connected to wifi
  - Check your laptop's internet connection and wifi network.
  - Use the companion app, Chewie, to configure Victor's WiFi.
  - Use the mac client to troubleshoot wifi. Information [here](https://ankiinc.atlassian.net/wiki/spaces/ATT/pages/323321886/Victor+OTA+update+using+Mac-Client+tool#VictorOSand%5COTAupdateusingMac-Clienttool-OTA) and [here](/docs/mac-client-setup.md).

### Deploying is super slow
  - Try turning off wifi on your laptop and use ethernet instead.
  - Try moving to somewhere else in the office, a conference room or the kitchen. Might be an access point/network problem.
  - If nothing seems to help, consider bringing the robot to the hardware team. He might have a bad antenna.

### My Victor won't turn on/stay on
  - If only the top backpack light blinks when on the charger then the robot is low on battery and will not turn on by just being placed on the charger
  - First turn the robot on with the backpack button and all the lights should turn on as normal
  - Quickly place the robot on the charger. It should stay on at this point and begin charging
  - Leave the robot on the charger for ~30 minutes

### How do I set a console vars and set console functions on my robot?

Open the web server (see above) and look for "console vars/funcs".

### `victor_stop`/`victor_restart` hangs with `stopping <number>...` when trying to kill one of the processes
  - Manually kill the process with `ssh root@<robot-ip> kill -9 <number>`
  - Run `victor_stop` to ensure the rest of the processes are stopped
  
### One of the processes crashed/aborted and the stack trace has no symbols, how do I get symbols for the trace?

  - Manually deploy the full libraries (with symbols) to the robot; they are located in `_build/vicos/<Debug/Release>/lib/*.so.full`
  - `scp _build/vicos/<Debug/Release>/lib/<one_or_more_of_the_libraries>.so.full /anki/lib/<one_or_more_of_the_libraries>.so`
  - Reproduce the crash and you should now see symbols
  - [Follow these detailed instructions for using the Tombstone file (with stack trace)](https://ankiinc.atlassian.net/wiki/spaces/VD/pages/404652111/Victor+Crash+Reports)
  
### How do I do performance analysis/benchmarking on robot?
  - Use simpleperf to generate list of highest overhead functions in a process by running [`project/victor/simpleperf/HOW-simpleperf.sh`](/project/victor/simpleperf/HOW-simpleperf.sh)
  - If the script fails with the error: `[native_lib_dir] "./project/victor/simpleperf/symbol_cache" is not a dir`, cd into `./project/victor/simpleperf/` and run `make_symbol_cache.sh`
  - Use inferno to generate a flame graph of call hierarchies by running [`project/victor/simpleperf/HOW-inferno.sh`](/project/victor/simpleperf/HOW-inferno.sh)
  - By default both scripts run on the engine process, `cozmoengined`. Change this by prepending `ANKI_PROFILE_PROCNAME="<name_of_process>"` to the command to run the script. The other two process names are `victor_animator` and `robot_supervisor`
  - To see overall cpu load per process run `top -m 10`
  
### `Sending Mode Change ...` or `spine_header ...` errors continually spam in the logs
  - Ensure you have deployed correctly built code otherwise Syscon and robot process are likely incompatible, give to [Al](https://ankiinc.atlassian.net/wiki/display/~Al.Chaussee) or [Kevin](https://ankiinc.atlassian.net/wiki/display/~kevin) to have firmware flashed
  
### Deploying to the robot fails (possibly complains about out of disk space?)
  - You can safely remove the cache and persistent folders with `ssh <robot-ip> rm -rf /data/data/com.anki.victor/cache /data/data/com.anki.victor/persistent`
  - If this isn't enough, in the shell you can check disk usage with `df` to list percentage of space each section has free, you should only care about the `/` and `/data` sections. The percentage indicates how much space the section is using out of how much total space it has.
  - You can then inspect space usage with `du -h /anki` which will list disk usage of all directories in `/anki`. Most things that get deployed live in `/anki`. Files that get generated at run-time may be stored in `/data/data/com.anki.victor`.
  - You can also delete the `/anki` folder with `rm -rf /anki`, but in general you should never have to do. It may be useful when the you get an error like this.
  
```
$ victor_deploy
INSTALL_ROOT: /anki
rsync: failed to connect to 192.168.42.251: Connection refused (61)
rsync error: error in socket IO (code 10) at /BuildRoot/Library/Caches/com.apple.xbs/Sources/rsync/rsync-51/rsync/clientserver.c(106) [sender=2.6.9]
```
  
### How do I run unit tests?
  - https://ankiinc.atlassian.net/wiki/spaces/VD/pages/149363555/Victor+Unit+Tests

### Unit tests are failing locally

For example failing with these error messages:

```
2: (t:01) [Error] TFLiteLogReporter.Report Model provided has model identifier 'ion ', should be 'TFL3'
2:  
2: (t:01) [Error] TFLiteModel.LoadModelInternal.FailedToBuildFromFile /Users/arjun/Code/victor/_build/mac/Debug/test/engine/resources/config/engine/vision/dnn_models/dfp_victor_6x6_tiny_128x128_36b906234ae4405dbf479d42d87787da.tflite 
2: (t:01) [Error] NeuralNetRunner.Init.LoadModelFailed  
2: (t:01) [Error] VisionSystem.Init.NeuralNetInitFailed Name: person_detector 
```

*Resolution*

This can be caused by a bad checkout by git lfs, so the fix is to either:

- delete the offending folder but from the *resources* directory (NOT build)
- check that folder out again
- double check the file sizes match what is on github to be certain

Importantly, no need to clean and build again. Tests should run fine after this.

Alternatively these steps should work with getting a good checkout.

- git lfs uninstall
- rm  *that file*
- git reset --hard
- git lfs install && git lfs pull

### I get permission denied during build `error: can't exec 'victor/_build/mac/Debug-Xcode/launch-c' (Permission denied)`
  - `chmod u=rwx victor/_build/mac/Debug-Xcode/launch-c`
  - `chmod u=rwx victor/_build/mac/Debug-Xcode/launch-cxx`

### To check the battery level:

Open the engine (8888) web server (see above instructions) and click the `ENGINE` button.

Or: 
   1. Place Victor on charger
   1. Double click his back button
   1. Raise and lower the lift the full way to enter the debug screen
   1. Move his head all the way down and back up
   1. A tiny pixel will appear in the upper right of the face
   1. Use the button to cycle through the various face screens
   1. The second screen has battery like `BATT: 4.18V`

### Webots Firewall Connection issues?
  - [Create a code signing certificate](/project/build-scripts/webots/FirewallCertificateInstructions.md)
  - [Run a sample script to initially sign](/simulator/README.md#firewall)

### Can't generate Xcode project using `build-victor.sh -p mac -g Xcode -C` and getting error output like this
```
-- The C compiler identification is unknown
-- The CXX compiler identification is unknown
CMake Error at CMakeLists.txt:9 (project):
  No CMAKE_C_COMPILER could be found.
CMake Error at CMakeLists.txt:9 (project):
  No CMAKE_CXX_COMPILER could be found.
```

  * ... then perform these steps (continued from above)
   	* Install command line tools
    * `sudo xcode-select -s /Applications/Xcode.app/Contents/Developer`# New Document
   
### Can't generate Xcode project using `build-victor.sh -g Xcode -C` and getting error output like this
```
CMake Error at lib/util/source/3rd/civetweb/cmake/FindWinSock.cmake:77 (if):
  if given arguments:

    "x86_64" "STREQUAL" "AMD64" "AND" "EQUAL" "4"

  Unknown arguments specified
Call Stack (most recent call first):
  lib/util/source/3rd/civetweb/src/CMakeLists.txt:26 (find_package)
```

  * you forgot `-p mac`
  
### CMake configuration runs twice in the same build
```
-- Configuring done
-- Generating done
-- Build files have been written to: /my_victor_dir/_build/vicos/Release
[0/1] Re-running CMake...
-- The C compiler identification is Clang 5.0.1
-- The CXX compiler identification is Clang 5.0.1
-- CMAKE_C_COMPILER=/Users/username/.anki/vicos-sdk/dist/0.9-r03/prebuilt/bin/arm-oe-linux-gnueabi-clang
-- CMAKE_CXX_COMPILER=/Users/username/.anki/vicos-sdk/dist/0.9-r03/prebuilt/bin/arm-oe-linux-gnueabi-clang++
```
This can cause unpredictable build behavior, and can be fixed by deleting `_build/<platform>/<configuration>/.ninja_log`

### When profiling I see "...doesn't contain symbol table"
  - This is just a warning, there are no symbol tables for the .so files on the device, instead we use symbols from the symbol cache

### How do I decipher this crash in the victor log

```
12-31 16:22:17.366  2534  2733 F libc    : Fatal signal 11 (SIGSEGV), code 1, fault addr 0x0 in tid 2733 (CozmoRunner)
12-31 16:22:17.367  1848  1848 W         : debuggerd: handling request: pid=2534 uid=0 gid=0 tid=2733
12-31 16:22:17.413  2884  2884 E         : debuggerd: Unable to connect to activity manager (connect failed: No such file or directory)
12-31 16:22:17.465  2884  2884 F DEBUG   : *** *** *** *** *** *** *** *** *** *** *** *** *** *** *** ***
12-31 16:22:17.465  2884  2884 F DEBUG   : Build fingerprint: 'qcom/msm8909/msm8909:7.1.1/NMF26F/andbui11141045:eng/test-keys'
12-31 16:22:17.465  2884  2884 F DEBUG   : Revision: '0'
12-31 16:22:17.466  2884  2884 F DEBUG   : ABI: 'arm'
12-31 16:22:17.466  2884  2884 F DEBUG   : pid: 2534, tid: 2733, name: CozmoRunner  >>> /data/data/com.anki.cozmoengine/bin/cozmoengined <<<
12-31 16:22:17.467  2884  2884 F DEBUG   : signal 11 (SIGSEGV), code 1 (SEGV_MAPERR), fault addr 0x0
12-31 16:22:17.467  2884  2884 F DEBUG   :     r0 ffffffff  r1 ffffffff  r2 00000001  r3 00000051
12-31 16:22:17.468  2884  2884 F DEBUG   :     r4 add70e80  r5 00000000  r6 00000000  r7 ae67f1a0
12-31 16:22:17.468  2884  2884 F DEBUG   :     r8 00000000  r9 aeaf4760  sl add780d0  fp b1938690
12-31 16:22:17.468  2884  2884 F DEBUG   :     ip b0b75870  sp ae67f158  lr afd6e92b  pc afd6e960  cpsr a00f0030
12-31 16:22:17.512  2884  2884 F DEBUG   : 
12-31 16:22:17.512  2884  2884 F DEBUG   : backtrace:
12-31 16:22:17.512  2884  2884 F DEBUG   :     #00 pc 00287960  /data/data/com.anki.cozmoengine/lib/libcozmo_engine.so
12-31 16:22:17.513  2884  2884 F DEBUG   :     #01 pc 002c34a7  /data/data/com.anki.cozmoengine/lib/libcozmo_engine.so
12-31 16:22:17.513  2884  2884 F DEBUG   :     #02 pc 00273793  /data/data/com.anki.cozmoengine/lib/libcozmo_engine.so
12-31 16:22:17.513  2884  2884 F DEBUG   :     #03 pc 002751ed  /data/data/com.anki.cozmoengine/lib/libcozmo_engine.so
```

Looking at the first four lines after `backtrace:` you can see the top four addresses on the stack and the shared library they reside.

  - `victor_addr2line libcozmo_engine.so 00287960 002c34a7 00273793 002751ed`

to get:

```
0x00287960: .anki/vicos-sdk/dist/0.9-r03/sysroot/usr/include/c++/v1/memorymemory:4041
0x002c34a7: projects/victor/_build/vicos/Release/../../../engine/aiComponent/behaviorComponent/behaviors/iCozmoBehavior.cpp:714
0x00273793: projects/victor/_build/vicos/Release/../../../engine/aiComponent/behaviorComponent/behaviorStack.cpp:123
0x002751ed: projects/victor/_build/vicos/Release/../../../engine/aiComponent/behaviorComponent/behaviorSystemManager.cpp:154
```
### How do I increase the max files limit on macos Sierra and greater? (when seeing an error like `[Errno 24] Too many open files`)
  - Follow instructions at https://gist.github.com/tombigel/d503800a282fcadbee14b537735d202c

### Is this Open Source license ok for use?
  - https://ankiinc.atlassian.net/wiki/spaces/ET/pages/380436502/Open+Source+Software

### I just downloaded this library from github, can I use it?
  - https://ankiinc.atlassian.net/wiki/spaces/ET/pages/380436502/Open+Source+Software

### How do I add licensing information to a library I just added?
  More information [here](/docs/development/licenses.md)

### BaseStationTimer, WallTime, UniversalTime, I'm confused!? Which timer should I use?
  - See the [Time, Clocks, and Timers](/docs/development/time.md) doc
  
### I want to use Linux, instead of macOS.  Which version should I use?
  - Use Ubuntu 16.04

### Why did I get a 915 error?

  - There is a client/server connection between `vic-engine` and `vic-anim` through [`LocalUdpServer.cpp`](/coretech/messaging/shared/LocalUdpServer.cpp) and [`LocalUdpClient.cpp`](/coretech/messaging/shared/LocalUdpClient.cpp) and sockets `/dev/socket/_engine_anim_server_0` and `/dev/socket/_engine_anim_client_0`
  - `vic-engine` sends enough data to `vic-anim` that it fills the queue (256KB) which causes function `LocalUdpClient.Send` to get a `Resource temporarily unavailable` error from the socket which in turn disconnects. All future attempts to send from `vic-engine` fail.
  - `vic-anim` tries to send to `vic-engine` but the function `LocalUdpServer.Send` gets a `Connection refused` because it was disconnected, `vic-anim` also disconnects and issues a 915 error.
