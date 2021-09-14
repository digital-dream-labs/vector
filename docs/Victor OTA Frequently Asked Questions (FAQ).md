# Victor OTA Frequently Asked Questions (FAQ)

## Disclaimer

Information on this page may be out of date. Actual behavior is determined by source code in /anki/victor/platform/update-engine and /anki/vicos-oelinux/external/rebooter.

## What is needed for the Victor robot to update Over The Air (OTA)?

The update process involves several steps:

1.	Download of the OTA file to the robot
2.	Install the OTA file to the "other" boot slot
3.	Restart of the robot to boot into the "other" boot slot 

## What are "boot slots"?  How many does Victor have?  What are they used for?

Victor has 3 boot slots named a, b, and f.  

* f is the "recovery" boot slot and it is what Victor boots up into when you take him fresh out of the box or hold down the backpack button for approximately 15 seconds while he is on the charger (hold until he turns off, keep holding until his backpack lights come back on).  The "recovery" boot slot has version 0.9 of the Victor OS.  0.9 doesn't do much other than wait for the user to configure WiFi and install the latest version of Victor OS via the "Vector Robot" app (fka Chewie) into slot a or b. This slot is NEVER overwritten for the life of the robot.  
* a and b are "normal" boot slots.  Only one of these slots can be booted into at a time.  The other slot can be overwritten with a new version of the Victor OS.  On the next reboot, the robot switches to the other boot slot.
* A "normal" boot slot has 2 partitions : boot and system .  The boot partition holds the Linux kernel.  The system partition holds the root filesystem.  When we install an OTA update to a boot slot, both of these partitions are updated.

A little more info on how a boot slot gets marked successful (from Daniel Casner (Unlicensed))

Each slot has 3 flags on it,

1. An active flag (is this the one we should try to boot) 
2. A boot try count 
3. A boot successful flag. 

When the "other" slot is marked active, it's boot try count and boot successful flags are cleared. 
The bootloader will try booting the active slot up to 7 times. If the successful flag isn't set in 7 boot attempts, it will boot the other slot if it is marked successful or the F slot if not. 
The OS in the slot is responsible for setting the successful flag. Right now we set it when we have successfully started the bluetooth daemon. This is actually something we should consider changing when we can but that's a larger conversation. 

## What are "full" OTAs? What are "delta" OTAs?  How do they work?

A "full" OTA file contains the entire boot and system images to install in a slot.  To date, these files are around 185 megabytes.  The OTA client installs them into a boot slot by directly copying them in.

A "delta" OTA file contains a much smaller delta.bin file that only has the differences between the currently running boot and system images and the new versions.  These files vary in size depending on how big of a difference the update it.  They could be as small as 10 megabytes in recent testing.  While a "delta" OTA will download much faster, it could take  upwards of 10 minutes to install.  During installation, a lot of CPU and memory are consumed.

"delta" OTAs are used for production and for development builds.

## When does the Victor robot automatically download an OTA file?

Short answer: Right after Victor gets on WiFi and after that every hour Victor will try to download a new OTA file to install.  If it downloads a new OTA file, it will be installed to the "other" boot slot.  Please note, you will not be running this new version until the robot is rebooted.

Long answer: As soon as Victor's WiFi comes online, he will try to download an OTA file.  After that, at a random time every hour, if Victor hasn't already downloaded a new OTA, the robot will attempt to download and install an OTA file. If an update is available, the download to the robot starts immediately, and installs into the "other" boot slot.  Please note, you will not be running this new version until the robot is rebooted.  If the download or installation fails, Victor will try again during the next hour.  If the robot is turned off or loses power during the download or installation, no problem.  The next time he is powered up and gets on WiFi or hits his hourly cycle he will start over and try to download the OTA file again.

## What HTTP URL does Victor request to get the new OTA file?

Every hour, a production Victor robot will make an HTTP request to https://ota.global.anki-services.com/vic/prod/diff/<current-os-version-number>.ota to see if there is an update from the current OS version that he is running.  For example, if Victor is running 1.0.1.1768, he will make an HTTP request to https://ota.global.anki-services.com/vic/prod/diff/1.0.1.1768.ota.  If Anki hasn't released an update to version 1.0.1.1768, then this request will fail.  As a result, the robot.ota_download_end DAS event will have s1='fail' and s3='Failed to open URL: HTTP Error 403: Forbidden'. 

The 403: Forbidden part of the string is very misleading.  Victor isn't forbidden from getting this file.  The file does not exist.  If you are interested in why we get a 403 instead of a 404, please read [AWS article : I’m using an S3 website endpoint as the origin of my CloudFront distribution. Why am I getting HTTP response code 403 (Access Denied)?](https://aws.amazon.com/premiumsupport/knowledge-center/s3-website-cloudfront-error-403/)


## I want to know more about the DAS events associated with OTA, where do I go?

See the DAS Event Proposal - OTA Client spreadsheet 

## When does the Victor robot restart overnight to boot into the new version of Victor OS?

Short answer : There is a nightly restart at a random time between 1am & 5am robot time (i.e. in the timezone you used to setup the robot).

Long answer : On a typical night, at 1am Victor's rebooter app will run.  The rebooter will do the following to make sure it is not rebooting at an inappropriate time:

1.	Check that the robot has a timezone set.  If the robot doesn't have a time zone, we don't really know that it is between 1am and 5am local time.  If no timezone is set, the rebooter app quits and will try again the next time the clock hits 1am.  This shouldn't be possible if the user set up the robot using the Vector Robot app on their smartphone.
2.	Check that the robot has been running for at least 4 hours.  We don't want to reboot again for no good reason.  If the robot has only been running for say 2 hours, we will wait an additional 2 hours before considering if we want to reboot again.
3.	Delay a random amount of time, but not past 5am, before considering if we should reboot.  We do this so that we don't have hundreds of thousands of robots rebooting and contacting our servers all at the same time.
4.	Check for special files /data/inhibit_reboot and /run/inhibit_reboot. If either file is present, we wait and try again up to 5am. We will give up at 5am.
5.	Check that we are in powersave or userspace mode.  This is our indication that the user is not interacting with the robot.  If they are, we wait and try again up to 5am. We will give up at 5am.
6.	Check that we are not right in the middle of downloading and install an OTA.  If we are downloading and installing an OTA file, we want that process to finish, so we wait and try again up to 5am. We will give up at 5am.

Pending OS Update Addendum:

If an OS update has been downloaded and installed and is just waiting for the robot to reboot, we will ignore all of the reasons not to reboot listed in the "Long Answer" section except for #4.  This change was introduced in version 1.3.0.

## What can the user do to check update status?

The user can use the Vector App to check if an update has completed download. Browsing to the Updates page of Settings in the app will show if an update has been downloaded, and display a button that, when pressed, restarts the robot. Once the robot has restarted, it will be running the new version.

## What if we don't want Victor to wait until overnight to reboot into the new version of Victor OS automatically?

It is possible to create an OTA file with the special flag, reboot_after_install=1, in its manifest.  This will tell the OTA engine on the robot to reboot immediately after successfully installing the update.  

## What about "development" robots?  Do they auto update?

Short answer: Yes, they will automatically update.

Long answer: They will automatically update unless you deploy "victor" software to them.  This is something that typically only developers do.  Sometimes QA.  The automatic updates are delta updates.  The delta updates assume that the boot and system partitions for the slot have not been touched.  Delta updates are small because they just capture the difference between the Victor OS version you are currently running and the new one.  However, when you deploy software, the system partition is mounted read/write and the files are changed.  When the robot attempts to install the delta update it will fail since your current system partition no longer matches what is expected.

## I am a developer. I deployed software onto my robot. Now I want to update the OS?

You have several options:
1. Use mac-client with the ota-start command and pass whatever URL you want
2. Use Chewie with a custom URL, or latest, or lkg.  Reboot your robot into factory (0.9) and install it that way.
3. If you are running 1.2.1.2220 or later, you can use the /sbin/update-os command.

update-os commands 
```
$ update-os -h   # Get help information
$ update-os lkg # Download and install the LKG OS
$ update-os latest # Download and install the latest version of the OS
$ update-os 1.2.1.2220 # Download and install version 1.2.1.2220
$ update-os http://mylaptop:5555/path/to/os.ota # Download and install from any url
$ update-os # Download and install the LKG OS
```

## How do I deploy a VicOS PR build?

VicOS PR builds produce an OTA artifact, but these artifacts are not automatically published to cloud storage.
Robots cannot download artifacts from TeamCity directly because they do not have appropriate authorization.
The easiest way to install the OTA image from a PR build is to download the artifact from TeamCity to your laptop, then start a local http server to get it from your laptop to the robot.

On your laptop:

Download OTA artifact (eg vicos-1.4.0.410d.ota) from TeamCity to your ~/Downloads, then
```
#!/usr/bin/env bash 
$ cd ~/Downloads
$ python -m SimpleHTTPServer 5555
python -m SimpleHTTPServer 5555
Serving HTTP on 0.0.0.0 port 5555 ...
```

On the robot:
```
#!/usr/bin/env bash 
# /sbin/update-os http://your.ip.here:5555/vicos-1.4.0.410d.ota
Current OS Version: 1.3.0.2507d
Downloading OS update from:
http://your.ip.here:5555/vicos-1.4.0.410d.ota
Updating to 1.4.0.410d ( 0% ) ...
```

Don't forget to ^C to terminate the HTTP server when you are done!

## What about the First Time User Experience (FTUE) or a factory reset?

During FTUE or factory reset, the robot boots into the f slot and runs Victor OS 0.9.  In Victor OS 0.9, there are NO automatic hourly updates.  In OS 0.9, the robot waits for the user to connect to it with Vector App and give it a URL to the latest Victor OS version.  After it installs that version, it will immediately reboot into the new version.

## What URL does Vector App send to the robot to install the latest OTA?

See Vector Build Configurations for more info on this.

## What happens if the download of an OTA is interrupted?

If the download of an OTA is interrupted it must be restarted from the beginning.  There is no resume capability at this time.

## When an OTA download is in progress, will a user see any performance issues?

Maybe.  Downloading an OTA consumes memory and CPU.  If the user is stressing Vector for other purposes, they may notice performance issues.  At worst, I am concerned that they may experience a crash if memory is too low.

## Why do I keep seeing the robot.ota_download_end DAS event say "Failed to open URL: HTTP Error 403: Forbidden"?

Because Victor is trying to download the OTA file every hour and it doesn't exist.  See prior question about what HTTP URL Victor uses for more details.


