# Partitions

Created by Daniel Casner Last updated Apr 27, 2018

There are a total of 30 partitions on Victors eMMC flash memory but most of them are dedicated to SoC firmware functions rather than the higher level OS and applications so they aren't covered in detail in this document.

|Label      |Mount Point|Mode| Size | Purpose  |
|-----------|-----------|----|------|----------|
|recovery   |           | RO | 32MB | Factory OS kernel and initram|
|recoveryfs | /         | RO | 640MB| Factory OS root filesystem|
|boot_{a,b} |           | RO (except for OTA)|32MB |Slot A and Slot B kernels and initrams|
system_{a,b}| /         | RO (except for OTA)|896MB|Root file systems for slots A and B|
|persist	|/persist   | RW | 64MB | Reserved for future use if necessary.  Persistent robot specific data written after factory|
|oem        | /factory  | RO (except at factory) | 4MB	Robot specific data written at the factory|
|userdata   | /data     | RW | 768MB | User data stored on the robot|
|tmpfs      | /data (0.9 OS)|RW|~70MB| Temporary user data before update to 1.0|
|tmpfs      | /run      | RW | ~70MB | Temporary FS in RAM for scratch pad|

Note that on all the root file system partitions, approximately 300MB is consumed by the OS