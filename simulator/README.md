# Webots Simulator Setup

We are currently using [Webots version R2018a rev 2](https://cyberbotics.com/archive/mac/webots-R2018a-rev2.dmg) (a list of versions we currently support is available [here](supportedWebotsVersions.txt)). A license is required to run the Webots simulator. Submit a ticket to the Helpdesk to request a license. 

## Setup

Once Webots is installed, test it out by trying opening `simulator/worlds/cozmo2World.wbt`.

1. Go to the menu item Webots->Preferences. Select OpenGL and check the 'Disable shadows' box. Shadows can particularly interfere with the downward facing camera. We can assume this area will be actively lit on the robot.

1. Add `WEBOTS_HOME` environment variable by adding the following line to your `~/.bash_profile`:

    ```
    export WEBOTS_HOME=/Applications/Webots.app
    ```

    This is necessary to build controller or supervisor projects from the command line, which seems necessary when you have more than one controller or supervisor in your world.

1. Create a file called `console_vars.ini` in the directory `simulator/controllers/webotsCtrlGameEngine2`. Add the following lines to the file:

    ```
    ; Console Var Ini Settings
    ; Edit Manually or Save/Load from Menu
    
    [Channels]
    unnamed = false
    
    [Logging]
    EnableCladLogger = false
    ```

    This disables some default logging settings that can be problematic.

### Building webots

You don't have to, but if you want to...

The source code to webots is available here: https://github.com/omichel/webots

And setup instructions here: https://github.com/omichel/webots/wiki/macOS-installation
with some optional setup that is not optional for Mac here: https://github.com/omichel/webots/wiki/macOS-Optional-Dependencies

To build:

```
make debug -j4
```

which takes less than 10 minutes, an internet connection is required as additional dependencies are downloaded.

I had difficulties building for anything other than Python 2.7 and needed to make the following changes for it to succeed:

```
diff --git a/projects/default/Makefile b/projects/default/Makefile
index 77b59f79..7ac48f88 100644
--- a/projects/default/Makefile
+++ b/projects/default/Makefile
@@ -28,11 +28,11 @@ else
 ifeq ($(MAKECMDGOALS),clean)
 TARGETS += $(ROS_MAKEFILE)
 else
-ifneq (,$(wildcard /mingw64/include/boost))
-TARGETS += $(ROS_MAKEFILE)
-else
-MESSAGE = '\043 \033[0;33mboost not installed, skipping ros controller\033[0m'
-endif
+# ifneq (,$(wildcard /mingw64/include/boost))
+# TARGETS += $(ROS_MAKEFILE)
+# else
+# MESSAGE = '\043 \033[0;33mboost not installed, skipping ros controller\033[0m'
+# endif
 endif
 endif
 
diff --git a/projects/default/libraries/vehicle/Makefile b/projects/default/libraries/vehicle/Makefile
index d8017bca..77431a72 100644
--- a/projects/default/libraries/vehicle/Makefile
+++ b/projects/default/libraries/vehicle/Makefile
@@ -43,8 +43,8 @@ ifeq ($(UBUNTU_VERSION), 16.04)
 endif
 endif
 ifeq ($(OSTYPE),darwin)
-       +@echo "# make" $(MAKECMDGOALS) python 3.7
-       +@PYTHON_COMMAND=python3.7 make -s -C python $(MAKECMDGOALS)
+       # +@echo "# make" $(MAKECMDGOALS) python 3.7
+       # +@PYTHON_COMMAND=python3.7 make -s -C python $(MAKECMDGOALS)
        @echo "# make" $(MAKECMDGOALS) python 2.7;
        +@PYTHON_COMMAND=python2.7 make -s -C python $(MAKECMDGOALS)
 endif
diff --git a/resources/languages/Makefile b/resources/languages/Makefile
index 30e1cfa4..8e14c0af 100644
--- a/resources/languages/Makefile
+++ b/resources/languages/Makefile
@@ -41,6 +41,6 @@ endif
 ifeq ($(OSTYPE),darwin)
        @echo "# make" $@ python2.7;
        +make -s -C python $@
-       @echo "# make" $@ python3.7;
-       +PYTHON_COMMAND=python3.7 make -s -C python $@
+       # @echo "# make" $@ python3.7;
+       # +PYTHON_COMMAND=python3.7 make -s -C python $@
 endif
```

## Overview

Webots runs concurrent processes for each active simulation object via "controllers". Cozmo simulations consist of the following controllers which can be found in [simulator/controllers](./controllers). In order to debug with breakpoints you'll need to first attach to the appropriate process (In Xcode, go to Debug > Attach To Process).

* `webotsCtrlGameEngine2` Engine
* `webotsCtrlRobot2` Simulated Robot
* `webotsCtrlAnim` Animation process
* `webotsCtrlLightCube` Simulated LightCube
* `webotsCtrlViz` Visualization overlays and display windows
* `webotsCtrlKeyboard` Keyboard interface for communicating with engine in place of the Unity app. Useful for quick headless (i.e. no Unity UI/Game) development.
* `webotsCtrlDevLog` For visualizing logs recorded from an application run. (Only used in `devLogViz.wbt`)
* `webotsCtrlWebServer` Victor web server process
* `webotsCtrlBuildServerTest` For Webots-based tests (that are executed on the build server with every PR and master build, as well as in nightly tests)

## Troubleshooting

### Firewall

When running webots, if you get pop-ups asking "Do you want the application 'webotsCtrl...' to accept incoming network connections?", then you have to set up a firewall certificate.

1. Close Webots.
1. Follow the instructions in [FirewallCertificateInstructions.md](/project/build-scripts/webots/FirewallCertificateInstructions.md).
1. Run the Webots test script (which will automatically set up firewall exceptions for you) with the following command: `project/build-scripts/webots/webotsTest.py --setupFirewall`. You will need to enter your password.

### Crash in webotsCtrlGameEngine2

If you get a crash in  `webotsCtrlGameEngine2` process, and an error message mentioning "shared memory" appears in the console log for Webots (towards the beginning), then it may be because simulating the Cozmo2 camera requires a larger memory budget than allocated. To increase it, edit the `/etc/sysctl.conf` file to have the following contents:

```
# Modified setup (currently not set)
kern.sysv.shmmax=67108864
kern.sysv.shmmin=1
kern.sysv.shmmni=256
kern.sysv.shmseg=64
kern.sysv.shmall=4096
```

And then restart OSX, for the changes to take effect. The shared-memory crash should no longer occur.

### Orphaned Processes

It's common to have orphaned webots controller processes hanging around after a simulation crashes. If you suspect that's the case, run

```
ps -ef | grep simulator\/controllers | cut -d ' ' -f 4 | xargs kill
```
 
You can also search for orphaned processes in Activity Monitor.

### Connecting to a Physical Robot

It's often useful to connect to a physical robot with a Webots world running only the UI (i.e. `webotsCtrlKeyboard`) and Viz (i.e. `webotsCtrlViz`) controllers in order to view the pose of the robot and various other debug information that is streamed from the engine in real-time. You can do so by opening  `cozmo2Viz.wbt` and entering the IP of the robot you want to connect to under the `engineIP` field of the `WebotsKeyboardController` found in the Webots Scene Tree Viewer.

