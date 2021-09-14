# Victor Developer Tips

Created by David Mudie

Random stuff that may be useful for Victor developers.  Note:  Some of the 'adb' commands below won't work on newer robots.

## FAQ
There is an FAQ for victor developers in the source repo:
https://github.com/anki/victor/blob/master/docs/FAQ.md

## Static IP Assignment
The OS already has a hack in it to make it easy to configure static IP address. Just create a file `/data/local/ipconf.sh` which sets the IP and it will use that instead of DHCP:

```
/data/local/ipconf.sh
ifconfig wlan0 192.168.40.xxx netmask 255.255.255.0
```

(where xxx is chosen outside the DHCP range 100-149 and does not conflict with other robots)

If you do this, update Victor Prototype Tracking Sheet to show 'static' next to the IP assignment.


Note that if you ever reflash your data partition, the static IP will be lost.

## /etc/hosts

For people who primarily use a single victor: add a line like `192.168.40.xxx victor vic` to the end of your `/etc/hosts` file and you don’t have to remember the IP all the time.

```
/etc/hosts
192.168.40.xxx victor vic
```

```
#!/usr/bin/env bash
adb connect victor
```

## System Clock
You can use adb to set the system clock:

```
#!/usr/bin/env bash
adb shell date `date +%m%d%H%M%G.%S`
```

## Serial Number
Victor's serial number is needed to establish a wifi connection.  It is usually displayed on his face.

If you can't see the number on his face, you can get it with adb:

```
#!/usr/bin/env bash
$ adb shell getprop ro.serialno
```

Victor will also advertise his serial number as part of his BLE name.
if you use `victor-ble-cli`, you will see the serial number after `VICTOR_` in the BLE name.

## OS Version
You can check Victor's OS version with 'getprop' ON THE ROBOT:

```
#!/usr/bin/env bash
# getprop ro.build.fingerprint
0.8.0-0
# getprop ro.revision
c4256dd
# getprop ro.build.version.release
201803231434
# getprop ro.anki.version
0.10.1151d
```

## Log Navigator
Setup instructions are here: https://github.com/digital-dream-labs/lnav-configuration

You can fetch setup files from the repository like this:

```
#!/usr/bin/env bash
mkdir -p ~/.lnav/formats
cd ~/.lnav/formats
git clone git@github.com:digital-dream-labs/lnav-configuration.git victor
```

## Recovery Mode (DVT4)
You can boot into recovery mode as follows:

1. Place Victor on charger
2. Hold down backpack button until Victor shuts off

Victor will reboot from his recovery partition, aka "factory settings".

## Debug Menu (DVT4)
You can access Victor's debug menu as follows:

1. Place Victor on charger
2. Double-press backpack button to show pairing menu
3. Raise and lower lift to show debug menu
4. Raise and lower head to enable debug pages
5. Press backpack button to cycle between debug pages

Rotate treads back and forth to move the menu cursor.

Raise and lower lift to select a menu option.

## Boot Images
Boot and system images are built on TeamCity:

https://build.ankicore.com/viewType.html?buildTypeId=VictorOs_LinuxPullRequest

The artifacts of the build include binary images of each partition.

Victor partition tables are described here: Partitions

## Flash A New Boot Image
You must connect to victor with USB (not wifi) to install a new boot image!

You can update victor's boot image or system image as follows:

```
#!/usr/bin/env bash
adb reboot bootloader
fastboot flash boot <boot.img>
fastboot flash system <system.img>
fastboot reboot
```

## Victor + Webots
You can use webots to control a physical robot using 'cozmo2Viz.wbt'.

1. Edit victor/simulator/worlds/cozmo2Viz.wbt
2. Uncomment line for engineIP. Edit value to show IP of your victor.
3. Start victor processes (eg victor_restart). Wait a few seconds for processes to stabilize.
4. Start webots
5. Webots → File → Open World → victor/simulator/worlds/cozmo2Viz.wbt
6. Webots → Simulation → Run

## Connect to robot at home using mac-client tool (DVT3/DVT4)

1. First, build the mac-client:
    a. pushd apps/demos/ble-pairing/mac-client
    b. ./build_client.sh
    c. popd
2. Make sure your laptop's bluetooth is on
3. Put victor on charger; double-click backpack button
4. Run the mac-client with:  ./apps/demos/ble-pairing/mac-client/_build/mac-client --filter XXXX
    a. (replace XXXX with last part of Vector name, e.g. V8D3)
5. After connecting, you'll get a prompt in the tool which is the name of the robot, e.g. vector-V8D3
6. Enter:  wifi-connect <ssid> <password>
    a. ...where ssid is the name of your home network, and password is the password to that network
    b. On success, the tool should say "Vector is connected to the internet."
7. Enter:  wifi-ip
    a. ...to get the robot's IP address
    b. Other commands:  help, status, etc.
8. Enter:  exit
    c. ...to exit the tool
9. Write the IP address to a file on laptop with:  echo "10.0.0.20" > tools/victor-ble-cli/robot_ip.txt
   a. (replace 10.0.0.20 with your robot's assigned IP)

## Connect to robot with ssh (DVT3/DVT4)
See instructions here: Victor DVT3 ssh connection

## Enable ADB over TCP (DVT3/DVT4)
DVT3/DVT4 factory builds do not allow ADB over TCP.

You can enable ADB over TCP by running the following commands ON THE ROBOT:

```
#!/usr/bin/env bash
# setprop service.adb.tcp.port 5555
# systemctl restart adbd
```

You can make this change permanent by running the following commands ON THE ROBOT:

```
#!/usr/bin/env bash
# setprop persist.adb.tcp.port 5555
# systemctl restart adbd
```

The persistent setting is stored in /data/persist.  It will be lost if you reformat /data.

