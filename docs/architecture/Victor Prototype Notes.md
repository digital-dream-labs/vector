# Victor Prototype Notes

Created by Daniel Casner Last updated Jan 04, 2018

The next generation of Cozmo will be largely similar to the existing Cozmo but we're putting an application processor roughly equivalent to a 5th generation Kindle fire, in the robot's head to allow it to function independently and connect to WiFi infrastructure to get to Anki servers directly.

## System Architecture
### Ecosystem
The Victor ecosystem consists of the robot, accessories (connected over BLE) and the charging base. The robot connects to user supplied devices (tablet, phone, etc.) via BLE or WiFi and can optionally connect to the internet through a user supplied wireless router.

In factory or service settings, a test fixture can communicate with the robot via the charge contacts in place of charge current.

![](victor%20ecosystem.png)

### Hardware Architecture
Victor has two processors, an application processor located in the robot's head running Embedded Linux and an embedded microprocessor in the robot's body controlling the majority of the sensors and actuators.

All the systems connected to the hardware process are accessed in the head over the spine serial protocol.

![](victor%20hardware%20architecture.png)

### Sensors
* Cliff sensors: LiteOn LTR676
* Time of Flight distance sensor: ST VL53L0X
    * Initial evaluation
    * Usable range: about 30 mm to 1200 mm
    * "Field of view": 25 degrees
* Camera
    * Resolution: 1280x720
    * Color
    * Field of view: TBD but diagonal FOV in range 88º to 120º


### [Face](V2%20Face.md)

### Software Architecture

![](Victor%20software%20architecture.png)
