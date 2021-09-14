# Victor Build Troubleshooting

Created by Paul Dimailig Last updated Mar 21, 2019

Some additional troubleshooting can be found here: https://github.com/anki/victor/blob/master/docs/FAQ.md

Also do not spill water or any liquids on your robot... or laptop...

## General
* If you run into an issue where your Victor is stuck on the SN screen, do the following
    * ssh root@robotIP rm /usr/bin/rsync.bin
    * exit
* When attempting to run any of our Victor aliases but you get a command not found error, run the below.
    * Go to your Victor repo
    * source ./project/victor/scripts/usefulALiases.sh

## Viewing Debug Screens
1. Set your Victor on the Charger
2. Press the backpack button twice then lift up his lift and down. You should see a screen with Customer Care Info. At this point, Victor no longer needs to stay on the charger. 
3. To cycle through screens: Move head up and down till you see small white dot in upper right of screen - then you can tap backpack button to get to new screens

## Updating Your Build/Branch
* If you run into errors around using 'git sync'
    * Do a git reset --hard origin/master
        * Note: The above command reverts any changes you made in the repo, including when adding your robot's IP into the Cozmo2Viz.wbt file.

## Connecting
* ADB
    * If you see an error saying the device is offline after you adb connect to the robot, just power cycle Victor and adb connect to him again.
    * If you run into "Connection Refused" error when doing ''adb connect <IP address in inet addr field>'
        * cd tools/victor-ble-cli
        * node index.js
        * scan
        * connect VICTOR_<your robot's sn>
        * restart-adb
        * Press Ctrl+C TWICE and try to 'adb connect <robot IP address>' again
* SSH
    * If you see an error saying the Operation Timed Out after you attempt to SSH into the robot, it may be having trouble connecting to the robot. Either a power cycle and retrying or doing the victor_set_robot_key VICTOR_<serial number on Victor's screen> should solve this.
    * If you run into "Connection Refused" error or getting asked for a password when attempting to SSH into the robot, be sure to run victor_set_robot_key VICTOR_<serial number on Victor's screen> alias for that robot.
* When using victor-ble-cli's node tool, if you're getting any error saying Error: Cannot find module ‘noble’ try the below
    * Go to your base victor folder
    * cd tools/victor-ble-cli
    * ./install.sh
* If you're not sure which robot you're connected to:
    * SSH allows you to be connected to multiple robots at the same time. So if you want to deploy or get logs, make sure you're connected to the right robot by:
        1. go to your victor folder
        2. go to tools
        3. go to victor-ble-cli
        4. open the robot_ip.txt file
        5. if needed, change the IP that of the robot you want to connect to
        6. save the .txt file
        7. run the desired command again in Terminal

## Deploying
* If you run into an issue where you run into the below errors after trying to do a victor_deploy or using deployables, they're usually a sign that the robot is out of space, which can be resolved by erasing victor's contents.
    * rsync: failed to connect to 192.168.42.251: Connection refused (61)
    * rsync error: error in socket IO (code 10) at /BuildRoot/Library/Caches/com.apple.xbs/Sources/rsync/rsync-51/rsync/clientserver.c(106) [sender=2.6.9]
    * rsync error: some files could not be transferred (code 23) at /BuildRoot/Library/Caches/com.apple.xbs/Sources/rsync/rsync-51/rsync/main.c(996) [sender=2.6.9] 
    * rsync error: unexplained error (code 255) at /BuildRoot/Library/Caches/com.apple.xbs/Sources/rsync/rsync-51/rsync/rsync.c(244) [sender=2.6.9]

    * Follow the below steps to clear up your Victor some space. NOTE: Following these steps will clear your robot of any logs. Be sure to check if there are any logs (micdata recordings, ) you wish to salvage before running.

        * ssh root@robotIP
        * cd ../..
        * rm -rf /data/data/com.anki.victor 
        * Attempt to do the deploy again.

    * Alternately, you can also "Clear User Data" from the Customer Care Screen on Victor's screen.
        * Place Victor on charger
        * Double tap on backpack button
        * Push lift up and down. You should then see the Customer Care Screen. 
        * Pick up Victor and turn his left tread (your right when he's facing you) until you see the carrot pointing at "Clear User Data"
    * If you get the following error when trying to use deployables from Team:
        * rsync: --chown=:2901: unknown option
        * rsync error: syntax or usage error (code 1) at /BuildRoot/Library/Caches/com.apple.xbs/Sources/rsync/rsync-52.200.1/rsync/main.c(1337) [client=2.6.9]
            * Run brew install rsync
            * Attempt to run the deployables again

## Webots
* If Webots cannot detect your robot, make sure to read the console if it's mentioning what IP it's attempting to connect to.
    * If it does see your IP, check to make sure it's not also including a quotation mark - e.g. [webotsCtrlKeyboard] getaddrinfo error: nodename nor servname provided, or not known (host 192.168.40.236":5103). Certain text editors may auto-correct the quotation marks when editing the engineIP in the cozmo2Viz.wbt file and make it unreadable.
    * If it is seeing a completely different IP, make sure that your IP is correctly set in the cozmo2Viz.wbt file in the engineIP field and that the line is uncommented (remove the # in the same line).
* If you see this getting spammed in Webots: WARNING: ‘/Users/pauldimailig/AnkiDeveloper/release/victor/simulator/plugins/physics/cozmo_physics/libcozmo_physics.dylib’: file does not exist.
    * Make sure you had ran this command: xcode-select --install
    * Then run the build script: project/victor/build-victor.sh -p mac -f

## Building
* If running into an issue during the build step where it’s stuck at step longer than 15 secs ‘[####/####] Linking CXX executable bin/cozmoengined’
    * Open the Activity Monitor app in the Mac and kill the ‘ninja’ process to stop the process (as Ctrl+C will not work).
    * Make sure in the Terminal you have done the ssh-add -K ~/.ssh/id_rsa
* If a build ends with a './lib/audio/configure.py: No such file or directory'
    * Make sure you had run: git submodule update --init --recursive
* If there are "CMake" non-webot-related errors occurring during your victor_build_release -f (like too many errors or FAILED: build.ninja), in the base Victor folder, run the following
    * rm -rf _build generated EXTERNALS
    * Attempt the build again.
* If there are "CMake" webot-related errors occurring, ensure that you have Webots installed (regardless if you have license).
* If you get an error Error: Command failed: go env GOPATH then you either do not have GO installed or the GOPATH is not properly configured.
    * Just follow the below steps to ensure a fresh install of GO
    * brew uninstall go
    * brew install go
    * Then attempt to build again.
* If you are getting an error when fetching and updating brew:
    * ssh-add -A
        * this will update/add your keys for the ssh agent
* If while 


If you run into a 'build ninja error', such as :

```
FAILED: build.ninja

/Users/rachel/.anki/cmake/dist/3.9.6/CMake.app/Contents/bin/cmake -H/Users/rachel/src/victor/victor -B/Users/rachel/src/victor/victor/_build/android/Release

ninja: error: rebuilding 'build.ninja': subcommand failed
```


Do the following : 

In your command line type < open ./ > (this opens your Victor folder) 

Delete the  _build and generated folders 



Now try again.