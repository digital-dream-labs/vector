# Hardware Power Mode Measurements
Created by Nathaniel Monson Last updated Sep 21, 2018

Hardware supports one of three power modes:
* ACTIVE - 40 minutes battery (480mA)
    * Victor can play, move, see, hear, stream, and dock with cubes and charger
    * Sensors, WiFi, Cubes, and CPU are running at full power

* CALM - 4 hours battery (70mA)
    * Victor's face is lit, but only occasionally moving or speaking, with only slight motions and quiet sounds
    * Victor can react to sensors (sight, sound, motion, touch) but reaction time is slowed to 1 second
	* Victor might be associated to WiFi but is not transferring or receiving any data.
	*Sensors, WiFi, Cubes, and CPU are running at 5% power - with severely reduced processing/frame rate

* OFF
    * Victor's face and power indicator are not lit.
	* Victor can't sense anything and only the power button can wake Victor
	* The CPU and Sensors are offline
	* Charging is still possible if Victor is left on the charger

It is the engine's responsibility to determine when to enter and exit these modes to maximize battery life.
Mode changes require a coordinated system-wide change in CPU consumption, behavior, and power settings.
The hardware (firmware) will cut power if the button is held for 2 seconds, if the battery runs out, or in case of severe hardware and software failures.

Hardware (DVT1) uses the following settings and switches to support the above modes:
* VEXT setting
    * This setting should only be entered after VEXT power has been above 4.5V for at least 100 drops (0.5s)
    * The above rule allows charge contact communication - since the robot tends to boot into VBAT and (properly implemented) charge comms will not hold VEXT high
    * In this setting, motors, charging, and sensors are OK to use, although tread motors interrupt charging (see charge switch, below)

    * To enter:  Disable IRQ, PWR_EN=PU, BAT_EN=0, MicroWait 10uS, #VEXT_EN=0, Enable IRQ

* VBAT setting
    * This setting should only be entered the instant the analog watchdog notes that VBATs is below 4.0V
    * In this setting, motors, charging, and sensors are OK to use (although charging would quickly switch to VEXT since VEXT will be stable)

    * To enter:  Use top priority IRQ so IRQs effectively disabled, PWR_EN=PU, #VEXT_EN=Z, BAT_EN=1

* OFF setting
    * In this setting, the "main power rail" (VBATs) is off, disabling the main CPU, the power LED, motors, and most systems
    * The robot is in this setting during startup and after power off - on charger (or stuck power button), syscon may remain powered for weeks or months
    * In DVT1, charging should not be used while OFF - this is due to a complicated muxing issue we'll try to fix for DVT2

    * To enter:  VEXT_EN=Z, BAT_EN=Z, VDDS_EN=Z, PWR_EN=0, LTNx=0, RTNx=0, MicroWait 5ms, LTP1=0, RTP1=0, MICMOSI1=0, I2C disable ToF/Cliff, etc

The hardware has a few peripheral switches:

* Charge switch
    * To enter:  Similar to P2 - first, stop driving tread motors (set LTNx and RTNx to 0), then wait 5ms, then set LTP/RTP to Z to enable charging
    * To exit:  After 30 minutes, set LTP/RTP to 0
    * To safely drive tread motors:  No special rules - just let the motor driver retake control of all LTxx/RTxx I/Os, which may or may not stop charging

* Power saving switches
    * These are enabled when OFF and in CALM mode on a 1 second interval (keeping all devices in power saving except for quick polling)
    * Each sensor has its own power saving switch - encoders (VDDs), cliffs and ToF (I2C), mics (clock)
    * The head exposes even more switches - separate power rails for sensors, Linux configuration, wakelocks, etc

In testing, the hardware team found that they exceeded expectations for the active state. 40 - 45 minutes is what they saw in testing.

The second spec - calm mode - is around 2 hours today - we've checked the hardware and it could reach 6 hours, but it requires OS development.