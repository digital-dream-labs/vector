# Victor OTA process

Created by Daniel Casner Last updated Mar 08, 2018

Victor has considerably more software that runs on the robot than Cozmo did, including a Linux based operating system and multiple robotics processes (robot supervisor, animation process, engine, etc.) and all the assets for this software (sound, animations, etc.) are also stored directly on the robot. However, for purposes of OTA, we bundle everything together into signed system updates which contain all changes to any part of the software or assets and updates are thus atomic, there is no question of having an "older OS" with "newer engine" etc.

## A / B (/ F) Updates

![OTA Examples](images/ota%20slot%20examples.png)

Victor uses A / B style updates inspired by the system recent Android devices use. In this system, the robot has two "slots" for the operating system called A and B. One contains the software the robot is currently running and the other contains the previous software if no new OTA has been started or the new software as it is downloaded / assembled during an OTA.

Victor has one additional slot "F" which is the only populated slot in the factory, contains the "factory" or "0.9" software, and is never rewritten. If for whatever reason, neither the A or B slot is considered usable, the F slot will be booted. Additionally a "unbrick" feature can allow a user (under direction from customer care) to force the robot to boot into the F slot.

## Delta Updates
When Victor downloads updated software, it does not download a complete new set of software and assets which could be almost a gigabyte and would be silly for small bugfix updates. Instead only the block level difference (delta) from the currently active slot is downloaded. As the OTA process downloads the update, it combines the data from the actuve slot with the downloaded delta to write into the alternate slot.

The update a robot needs to download depends on it’s current version and the new version

1.0 → 2.0  ≠  1.5 → 2.0  ≠  0.9 → 2.

Because robots which aren't used very often may fall more than one software version behind, Anki's update servers will need to provide deltas from all (or the subset we decide to support) previous software revisions to the current version. A robot updating from 1.0 to 2.0 would download a different update than a robot updating from 1.5 to 2.0. In particular, since robots may revert to 0.9 software at any time, there must always be a delta available from 0.9 to current. An alternative way to support very old software is to use the F (0.9) slot as a basis to apply a delta skipping the active slot.

## Lifecycle of an update

The OTA process is well described in the Android Documentation but our's is a slight variant.

1. OTA is initiated by [TBD] and meta-data is downloaded
2. Target slot is marked unbootable
3. Delta update is downloaded, uncompressed and combined with active-slot to write target slot
4. Target slot is validated
5. Target slot is marked bootable and active
6. When appropriate the robot reboots into the new active slot and is now running the new software
7. Once the robot verifies the new software is running as expected, it marks the new slot as successful
8. If the robot reboots [7] times without marking the slot successful, it will roll back to the previous slot

## Booting the new software
Because the entire software update has been downloaded and installed in the other slot in the background, switching to the new slot is a simple reboot of the robot with minimal downtime.

## Check for success and rollback
Once the robot has booted into the new software, it must communicate that it has successfully booted the new software to the bootloader or else on the next reboot it will be "rolled back" to the previous software. This ensures that any issue, corrupted image, bad code signature, bad update from Anki, that appears in the new software can be undone with minimal user impact.