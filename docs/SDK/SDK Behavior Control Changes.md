# SDK Behavior Control Changes

Created by Bruce vonKugelgen Last updated Feb 12, 2019

## Overview
Two new variations of SDK robot behavior control have been requested;

1. an "Exclusive Operation" mode where the robot's normal idle animations and responses can be suppressed between SDK scripts to keep the robot from moving.  This will allow for developers to run multiple scripts without unexpected changes to the robot's position.
2/ a "Highest Priority" mode where the SDK behavior blocks low-level responses like falling, cliff detection, etc.  In this case, the user could drive the robot off of the table if desired.

## Exclusive Operation
VIC-12248 - Allow a way for the robot to remain stationary between running Python scripts CLOSED

### Usage
An SDK user will enter into Exclusive Operation by using a ReserveBehaviorControl object to initiate control using the existing BehaviorControl stream.  This can be done in its own script (executing from a separate terminal instance), or as part of a larger script that then takes control using Robot/AsyncRobot/Connection objects.  At this point the SDK behavior will block other idle behaviors, but still allow any SDK actions that require basic behavior control.  Upon completion of these action scripts the robot will stay in the SDK Behavior.  Finally, when the BehaviorControlLock is completed, the Robot will return to normal operation.  Any subsequent requests to reserve behavior control will take priority, cancelling any existing ReserveBehaviorControl, while staying in this state of exclusive operation.

### Architecture
Exclusive Operation involves two changes to the existing SDK behavior control logic.

1. The Exclusive Operation will be enabled via a separate, long-lasting Gateway connection using the existing ControlRequest rpc/stream.  The Gateway code will be modified to allow for multiple connections by assigning a connection ID to each rpc, and filtering responses by ID to ensure correct routing of messages.  
2. SDK Behavior on the robot will take control at the lowest priority SDK control level (anticipating the addition of "Highest Priority" access).  This will allow for breaking control between scripts for low battery and other exceptional situations.  This is considered OK, since the point of the feature is to keep the robot still--which eliminates many situations that would break control.  No actual behaviors can be triggered until real control is enabled.

![](images/Multiple%20SDK%20Connection%20Diagram.png)

websequencediagrams.com code
```
title Using Multiple SDK Connections for Exclusive SDK Control
participant "SDK Control App" as App1
participant Gateway
participant "SDK Script App" as App2
  
note over App1: "control" script
note over App2: "app" script
App1->+Gateway: ControlRequest(RESERVE_CONTROL)
activate App1
note over Gateway:
    Control App successful connection
    to reserve control (by connection_id)
end note
Gateway->App1: ControlGrantedResponse()
note over App1:  active and waiting
App2->Gateway: ControlRequest(TOP_PRIORITY_AI)
activate App2
note over Gateway:
    Only Script App messaged,
    Gateway filters outbound responses
    by connection
end note
Gateway->App2:  ControlGrantedResponse(ConnectionID)
note right of App2:
    Any additional App
    connection will fail now
end note
note left of App1:
    Additional attempts to
    reserve control will take
    precedence, sending new
    MasterControlLostResponse
end note
note over App2: App2 performs SDK actions
Gateway->App2:  stream closes on disconnect
deactivate App2
note overApp1:
    Will continue to run
    until stopped by user
end note
Gateway->App1:  stream closes on disconnect
deactivate App1
```


There are several related issues that may be affected by these changes, or may have a bearing on the solution:

|Key      |Summary	|T|	Status|Resolution|
|---------|---------|-|-------|----------|
|VIC-12559|control_stream_task does not recover after a network reconnection|Bug|OPEN|Unresolved|
|VIC-7258 |Handle multiple settings requests to gateway|Story|OPEN|Unresolved|
|VIC-5381 |Consider returning failure response to Python when a behavior is requested and another behavior is already running|Story|OPEN|Unresolved|
|VIC-4829 |vic-gateway: engine message tagging (prevent conflicting messages)|Story|OPEN|Unresolved|

## Highest Priority Operation
VIC-3793 - SDK Behavior hooks: support multiple hooks, and add and finalize SDK hooks naming CLOSED
The following behaviors will be suppressed/superseded in the high priority state:

* Cliff detection
* Stuck on an edge
* Low battery/return to base
* "Hey Vector" triggered behaviors
* Gyro-related states (on a slope, picked up, on his backside)
* In the habitat
* In darkness

The following behaviors will continue to work in the high priority state:

* Audio streaming from robot
* All SDK event subscriptions, including wake word

The existing interaction of SDK and behaviors will stay the same for the (renamed) base priority level.

This will be implemented to the SDK user as a new value in the CONTROL_PRIORITY_LEVEL enum.  A new SDK behavior ID will be installed in the behavior tree at the ModeSelector level, below Alexa but above EmergencyMode and SleepCycle (the root of most obvious interrupting behaviors).  The behavior class will reuse the existing behaviorSDKInterface to avoid code duplication.

### Future Changes 
It may be desired to create an SDK-only behavior tree.  Other features (e.g. self-test) utilize their own tree in order to avoid conflicts with development of other behaviors in the main tree.

### Notes/Potential Issues
* We may already be incorrectly handling some aspects of SDK behaviors.  Some things may be failing silently right now–for example, when existing SDK control denies Vector the opportunity to perform a voice-triggered intent, this may be sending "dropped" intents to DAS.  This would be worth clean up, as it could also be incurring expense through online voice processing.
* Implementing Skills could require other types of control and require further improvements.  This could require different a different collection of controls.
It is very likely that the behavior organization and priority levels will change through normal Vector development.  These could affect SDK performance and need to be considered.
* Alexa is currently non-interruptible, and high priority.  This could lead to edge cases as the SDK tries to take control while Alexa is still operating.  The suggested fix is to add checks for Alexa operation when considering SDK behavior enablement instead of failing.   VIC-13032 - SDK should fail gracefully when Alexa is running OPEN
* Adding some form of visual feedback on the Vector screen could be an easily seen and understood cue to the user to avoid confusion ("why isn't Vector responding?").  This could take the form of an inactivity timeout, or a constantly visible decoration/watermark.   VIC-13031 - Add visible indication of SDK operation OPEN
* Calibration would be blocked by this mode, which could put the robot in confusing states (e.g. thinking the lift is down, when it is still partly raised).  Current robot calibration code will not re-calibrate when finally given back control, there may be future changes required for that code to function better (queuing the calibration, or notifying the user that the calibration needs to happen).