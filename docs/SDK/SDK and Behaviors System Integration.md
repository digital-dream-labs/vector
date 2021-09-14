# SDK and Behaviors System Integration
Created by Michelle Sintov Last updated Aug 06, 2018

## Summary
The behavior system coordinates overall robot activity, enabling the robot to act like he is in freeplay, stop when he is about to drive off a cliff, return to his charger when he is running out of battery and receive voice commands (https://github.com/anki/victor/blob/master/docs/architecture/behaviors.md).

With Cozmo, both the Python SDK and C# app did not have clear handoff of control with the behavior system. This resulted in bugs where the C# code would be trying to control the robot, then the robot would encounter a cliff and not handle the situation cleanly. Avoiding this situation is one of the goals of integrating the SDK layer with the behavior system.

## Behavior Hooks
The behavior system has a list of hooks ordered from highest to lowest priority, defined in globalInterruptions.json. At the time of this writing, the top of this list is as follows:

```
"behaviors": [
   "MandatoryPhysicalReactions",
   "LowBatteryFindAndGoToHome",
   "SDK0",
   "TriggerWordDetected",
   "TimerUtilityCoordinator",
   ...
```

At the highest level are “MandatoryPhysicalReactions” to, for instance, protect the robot from driving off a cliff, or flip down from being placed on his back. This hook is followed by the "LowBatteryFindAndGoToHome" behavior to instruct the robot to recharge when necessary.

One hook will be active at a time. That behavior then will be in control and can run actions or another behavior to control the robot. The hooks above the currently active behavior can monitor state, cancel behaviors below them or cancel themselves. If a higher priority hook wants control, the currently running behavior gets notified and deactivated. When the higher level behavior is done running, the previously stopped behavior may get reactivated.

The behavior system has an update loop during whic it asks each behavior if it wants to be activated.

All behaviors and hooks are defined in JSON.

## SDK hooks
As shown in the globalInterruptions list above, at the time of this writing we have SDK0 hook only. However, we plan to add additional SDK hooks, such as the following:

```
"SDK0",
"MandatoryPhysicalReactions",
"LowBatteryFindAndGoToHome",
"SDK1",
"TriggerWordDetected",
"SDK2",
```

By having SDK0 at the top of the list, then the user’s code will never be interrupted, and, for instance, you could drive your robot off a cliff. Level SDK1 above prevents the robot from driving off a cliff but since this is above `TriggerWordDetected`, the SDK user could pipe the audio stream through a voice processing service and make their own voice intents. Level SDK2 allows the existing voice intents to work but is above everything else. A much lower priority could be used to show a screensaver on Vector’s face when he is sleeping in an idle state.

## Architecture of SDK Integration with Behavior System
External SDK users (Vector Python SDK and, in the future, the companion app) will integrate with the behavior system in the following way. The Python SDK and slot SDK0 are used as an example, but the companion app and other slots could be used as well.

1. Behaviors system must be activated through shaking the robot.
2. User wants to run a Python script.
3. The Python SDK, as part of the connection process, sends SDKActivationRequest message to robot saying it wants to run at slot SDK0.
4. SDK robot component (SDKComponent) receives the SDKActivationRequest Message. This message indicates that the Python SDK wants the SDK behavior instance for slot SDK0 to be activated.
5. The SDK behavior instance for slot SDK0 regularly checks the SDK robot component to see  if the SDK wants control, and if so, the SDK behavior instance will say it wants to be activated.
6. The behavior system regularly checks which behaviors want to be activated and depending on their priority level will activate the behavior. In this case, the SDK0 behavior instance gets activated.
7. The SDK behavior instance informs the SDK robot component that the SDK behavior instance has control.
8. The sdk robot component sends message SDKActivationResult to the external SDK to inform that it has control.
9. The Python script can now run actions, low level motor commands, and behaviors.
10. When the Python script has completed, if it exits cleanly, `self.release_control()` in `disconnect` will cause the sdk behavior to deactivate. Need to future proof if the Python script does not end cleanly so that the SDK behavior doesn't remain in control. This is now handled by the Behavior Control Section below.

Any other state changes from the SDK behavior (e.g., lost control, etc.) will also need to be communicated (though this is not yet implemented).

When the SDK behavior is activated, then low level motor controls and actions will be allowed to run. Otherwise, those messages to control the robot will fail. These bools, both named _isAllowedToHandleActions  in movementComponent and robotEventHandler, are set to true when behavior is activated and to false when behavior is deactivated.

Note than in the behavior system, an activation request doesn't fail; it just waits until it gets activated. Later I will be adding a configurable timeout on the Python side and C# side so that if the SDK behavior doesn't get activated within the default time period, then the the SDK will cancel the activation request.

When an SDK behavior instance is activated, you can observe (like all other behaviors) that it is running on the behaviors tab in WebViz.

## Allowing External SDK to Run Behaviors (SDK behavior delegating to other behaviors requested by external SDK)
The flow is follows. A subset of behaviors is exposed to SDK users.

1. Python calls behavior interface to run a behavior like "go to charger".
2. Proto message goes through gateway
3. Gateway message is received by the SDK behavior.
5. The SDK behavior instance will delegate to the requested behavior.
6. When the behavior is completed, need to send a response back to the sdk to say the thing is done
7. Then, the sdk behavior will regain control

## Allowing External SDK to Run Existing Actions with CLAD

1. py send play anim message to gRPC
2. gateway sends play_anim as a clad msg
3. play anim clad action is requested
4. SDK behavior, which is listening for action completed messages, gets callback that play anim action has been completed
5. play anim proto response is sent back to gateway, which is then received by Python

## Behavior Control Flow
There are three different flows that can happen in the events of a behavior control: the sdk has control throughout, the sdk loses control and the sdk releases control intentionally.

### Normal Full Control
This is the case where the SDK has control throughout the connection.

They can always send a command, and the command is expected function as expected.

~[](images/SDK%20Behavior%20Control%20-%20normal.png)

websequencediagrams.com code
```
SDK->+vic-gateway: open stream rpc BehaviorControl
vic-gateway-->SDK: KeepAlive once a second
vic-gateway-->SDK:
SDK->vic-gateway: BehaviorControlRequest
vic-gateway->+vic-engine: ControlRequest(level)
vic-engine->vic-engine: Behavior Tree Actions
vic-engine->vic-gateway: ControlGrantedResponse
vic-gateway->SDK: BehaviorControlResponse
 
# typical flow
SDK->vic-gateway: other SDK commands
vic-gateway->vic-engine: other SDK commands
SDK-->vic-gateway:
vic-gateway-->vic-engine:
SDK-->vic-gateway:
vic-gateway-->vic-engine:
#
 
vic-gateway->vic-gateway: Detect disconnect
vic-gateway->vic-engine: ControlRelease
vic-engine->-vic-engine: Release SDK Control
vic-gateway->-SDK: stream closes on disconnect
```

SDK opens a stream to vic-gateway

1. vic-gateway begins sending keep-alive pings to check the health of the SDK connection
2. SDK sends BehaviorControlRequest to ask for control. This contains the desired behavior level (see SDK hooks).
3. vic-gateway sends control request to vic-engine
4. Here, there may be an indeterminate amount of time until control is granted based on the level in the behavior tree. Control will only be granted once the behavior tree trickles down to the level at which control was requested.
5. vic-engine responds through gateway to SDK with control granted (or nothing until control is acquired)

From then on the SDK has control until it disconnects

When vic-gateway detects a disconnect,

1. vic-gateway should send a ControlRelease to vic-engine
2. vic-engine will release control
3. vic-gateway terminates the stream

### Control Loss
In the event that control is lost mid-sdk execution, there will be a response telling the sdk that control is lost.

Optionally, they can request control again, and will be notified when it is available.

![](images/SDK%20Behavior%20Control%20-%20control%20loss.png)

websequencediagrams.com code
```
SDK->+vic-gateway: open stream rpc BehaviorControl
vic-gateway-->SDK: KeepAlive once a second
vic-gateway-->SDK:
SDK->vic-gateway: BehaviorControlRequest
vic-gateway->+vic-engine: ControlRequest(level)
vic-engine->vic-engine: Behavior Tree Actions
vic-engine->vic-gateway: ControlGrantedResponse
vic-gateway->SDK: BehaviorControlResponse
 
# control loss
SDK->vic-gateway: other SDK commands
vic-gateway->vic-engine: other SDK commands
SDK-->vic-gateway:
vic-gateway-->vic-engine:
vic-engine->vic-gateway: ControlLostResponse
vic-gateway->SDK: BehaviorControlResponse
opt re-request control
SDK->vic-gateway: BehaviorControlRequest
vic-gateway->+vic-engine: ControlRequest(level)
vic-engine->vic-engine: Behavior Tree Actions
vic-engine->vic-gateway: ControlGrantedResponse
vic-gateway->SDK: BehaviorControlResponse
 
# resume control
SDK->vic-gateway: resume SDK commands
vic-gateway->vic-engine:
SDK-->vic-gateway:
vic-gateway-->vic-engine:
end
#
 
vic-gateway->vic-gateway: Detect disconnect
vic-gateway->vic-engine: ControlRelease
vic-engine->-vic-engine: Release SDK Control
vic-gateway->-SDK: stream closes on disconnect
```

SDK opens a stream to vic-gateway

1. vic-gateway begins sending keep-alive pings to check the health of the SDK connection
2. SDK sends BehaviorControlRequest to ask for control. This contains the desired behavior level (see SDK hooks).
3. vic-gateway sends control request to vic-engine
4. Here, there may be an indeterminate amount of time until control is granted based on the level in the behavior tree. Control will only be granted once the behavior tree trickles down to the level at which control was requested.
5. vic-engine responds through gateway to SDK with control granted (or nothing until control is acquired)

When a higher priority behavior takes control:

1. vic-engine will send a ControlLostResponse through gateway to SDK
2. SDK can optionally choose to reconnect:
    a. SDK sends a control request through vic-gateway to vic-engine
    b. Again, there may be an indeterminate amount of time until control is granted based on the level in the behavior tree. (see step 4 above)
    c. vic-engine responds through gateway to SDK with control granted (or nothing until control is acquired)
    d. Normal SDK execution may continue

When vic-gateway detects a disconnect:

1. vic-gateway should send a ControlRelease to vic-engine
2. vic-engine will release control
3. vic-gateway terminates the stream

### Control Release (Free-Play Mode)
The SDK can also choose to explicitly release control. This would allow the robot to keep playing while connected, and the SDK can choose to take control when desired.

![](images/SDK%20Behavior%20Control%20-%20release%20control.png)

websequencediagrams.com code

```
SDK->+vic-gateway: open stream rpc BehaviorControl
vic-gateway-->SDK: KeepAlive once a second
vic-gateway-->SDK:
SDK->vic-gateway: BehaviorControlRequest
vic-gateway->+vic-engine: ControlRequest(level)
vic-engine->vic-engine: Behavior Tree Actions
vic-engine->vic-gateway: ControlGrantedResponse
vic-gateway->SDK: BehaviorControlResponse
 
# control loss (no handling)
SDK->vic-gateway: other SDK commands
vic-gateway->vic-engine: other SDK commands
SDK-->vic-gateway:
vic-gateway-->vic-engine:
SDK->vic-gateway: BehaviorControlRequest(release)
vic-gateway->vic-engine: ControlRelease
vic-engine->-vic-gateway: ControlLostResponse
vic-gateway->SDK: BehaviorControlResponse
loop
SDK->SDK: do other things
vic-engine->vic-engine: do other things
end
SDK->vic-gateway: BehaviorControlRequest
vic-gateway->+vic-engine: ControlRequest(level)
vic-engine->vic-engine: Behavior Tree Actions
vic-engine->vic-gateway: ControlGrantedResponse
vic-gateway->SDK: BehaviorControlResponse
SDK->vic-gateway: more SDK commands
vic-gateway->vic-engine: more SDK commands
SDK-->vic-gateway:
vic-gateway-->vic-engine:
#
 
vic-gateway->vic-gateway: Detect disconnect
vic-gateway->vic-engine: ControlRelease
vic-engine->-vic-engine: Release SDK Control
vic-gateway->-SDK: stream closes on disconnect
```

SDK opens a stream to vic-gateway

1. vic-gateway begins sending keep-alive pings to check the health of the SDK connection
2. SDK sends BehaviorControlRequest to ask for control. This contains the desired behavior level (see SDK hooks).
3. vic-gateway sends control request to vic-engine
4. Here, there may be an indeterminate amount of time until control is granted based on the level in the behavior tree. Control will only be granted once the behavior tree trickles down to the level at which control was requested.
5. vic-engine responds through gateway to SDK with control granted (or nothing until control is acquired)

When SDK decides to disconnect:

1. SDK sends a BehaviorControlRequest containing a release message inside
2. vic-gateway forwards the release request to vic-engine
3. vic-engine responds through vic-gateway to SDK that control is lost

When SDK desires control:

1. SDK sends another BehaviorControlRequest through vic-gateway
2. vic-engine responds through gateway to SDK with control granted (or nothing until control is acquired see above)

SDK execution may continue as desired.

When vic-gateway detects a disconnect:

1. vic-gateway should send a ControlRelease to vic-engine
2. vic-engine will release control
3. vic-gateway terminates the stream

### Conditional Request
As a future improvement, it may make sense to conditionally release control of the SDK, or (more accurately) request conditional control. In this case, the SDK will be told when it is activated, and may release control at it's leisure.

I haven't documented this flow at this time because there are still some unknowns. The most obvious of which is: how are conditions defined in the proto file specifications?