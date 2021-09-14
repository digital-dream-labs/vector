# What's in Victor?

### Processing
* Application processor: APQ8009
  * 4x ARM Cortex-A7 CPU cores
  * Embedded Linux
* System controller (aka syscon): STM32F0XX (ARM)
  * OS-less
  * Low-power

### Motors
* Tracked wheels
* Lift
* Head

### Displays
* 4x RGB LEDs in backpack
  * One dedicated "system" light
* 1x LCD face
  * 184 x 96 color (RGB565) pixels
  * Active area: 23.2mm x 12.1mm
* 1x speaker

### Sensors
* Encoders (wheels, lift, head)
* 1x inertial measurement unit (IMU)
  * Accelerometer + Gyro
* 1x camera
  * Resolution: 1280 x 720
  * Color
  * Field-of-view: 90 deg (H) x 50 deg (V)
* 4x optical cliff sensors
  * [LiteOn LTR676](https://ankiinc.atlassian.net/wiki/download/attachments/146148158/LTR-676PS-01_FINAL_DS_V1.1.pdf?version=1&modificationDate=1498149170522&cacheVersion=1&api=v2)
* 1x time-of-flight distance sensor
  * [ST VL53L0X](https://ankiinc.atlassian.net/wiki/download/attachments/146148158/VL53L0X.pdf?version=1&modificationDate=1498149210803&cacheVersion=1&api=v2)
  * Usable range: about 30 mm to 1200 mm ( Max useful range closer to 300mm for Victor )
  * "Field-of-view": 25 degrees
* 1x touch sensor
  * Embedded in backpack for detecting petting/holding
* 1x on/off button
* 4x microphones
  * Can detect direction of sound

### Communications
* WiFi
  * App/SDK, cloud services, OTA updates
* BLE
  * App/SDK, light cube comms

### Peripherals
* 1-2x light cube(s)
  * 4x RGB lights
  * 1x accelerometer
  * BLE communications
* 1x charger
  * Visual marker for self-docking
