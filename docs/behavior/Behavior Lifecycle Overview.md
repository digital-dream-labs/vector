# Behavior Lifecycle Overview

Created by Kevin Karol Oct 18, 2017

There are two key classes that define the lifecycle function calls that behaviors are guaranteed to receive within the behavior system. *iBehaviorRunner* calls the *iBehavior* lifecycle functions as it activates/deactivates behaviors. However, iBehaviorRunner doesn't make decisions about when to activate/deactivate behaviors, those requests are sent through the iBehaviorRunner interface. Most people interacting with the behavior system will never have to worry about ensuring that these two classes call each others functions appropriately and can just assume that the lifecycle function calls operate as specified below.

## IBehavior

The backbone of the behavior system, the IBehavior class provides the interface functions that

  1) A behavior is guaranteed will be called at the appropriate times by the higher level behavior system (OnEnteredActivatableScope, OnActivated, etc.)

  2) External utilities can be built on top of data returned by the behavior (e.g. GetAllDelegates for test frameworks/visualizers/debuggers etc)

Currently IBehavior functions that control behavior lifecycle are:

* Init: Called shortly after the behavior is created - allows the behavior to set up message subscriptions and grab properties it needs to track
* OnEnteredActivatableScope: Called when the behavior becomes a possible delegate of an active behavior. This function should be used to start any processes that WantsToBeActivated relies on (e.g. AIInformationAnalyzer process) or update robot properties necessary for the behavior to determine runnability (e.g. increasing the vision system's frame processing for face detection). Note: This function may be called multiple times if a behavior can be delegated to at multiple layers of the behavior stack, but it will only call OnEnteredActivatableScopeInternal once.
* OnActivated: Called when a behavior is given control of the robot. Once activated a behavior can delegate to other behaviors or actions.
* OnDeactivated: Called when a behavior loses control of the robot as the result of either canceling itself or being canceled by a behavior above it in the stack.  Note: This function is not called when a behavior delegates control to another behavior - until a behavior is canceled it is still considered activated since at any point in time it could cancel its delegates and perform a different action. Alternative checks must be employed to determine whether or not you are active and in control of the robot.
* OnLeftActivatableScope: Called when the behavior is not a possible delegate of any active behaviors. Be sure to shut down any processes set up in OnEnteredActivatableScope.

Within the behavior lifecycle there will also be regular calls to:

* WantsToBeActivated: When a behavior is within activatable scope calls may be made to WantsToBeActivated. This function should return whether or not the behavior wants to take over control of the robot. It's asserted in the behavior lifecycle that a behavior must check its delegates WantsToBeActivated function before delegating to a behavior.  Note: A call to WantsToBeActivated which returns true doesn't guarantee the behavior will actually be activated, so please don't start any processes within this function under that assumption.
* Update: Every behavior gets one and only one update tick per basestation tick while it is within the activatable/activated scope. This is a chance for the behavior to check any properties it is tracking and to receive any events that have occurred within the last basestation tick. Note: there is no property inherent to update that distinguishes between current behavior scope. If an activatable/activated distinction is important to a behavior's update behavior that must be tracked by the behavior itself.

Finally, all behaviors must implement the following utility functions

* GetAllDelegates: Behaviors must return a full set of behaviors that they might delegate to. This is essential for the iBehaviorRunner class to ensure that behavior lifecycle is appropriately maintained. It also allows a full behavior tree to be built in order to check designed asserts and tree walks.

![](High%20Level%20Behavior%20Function%20Calls.png)

Figure 1: Lifecycle of an iBehavior

Most people interacting with the behavior system will rely on the key lifetime functions of iBehavior, but shouldn't make any changes to the iBehavior class.

## IBehaviorRunner

A class that implements iBehaviorRunner is one which promises to abide by the lifecycle and function protocols laid out above when running behaviors. It must also implement an interface that IBehaviors can utilize to perform actions and request information about their current state.

Currently IBehaviorRunner functions that allow behaviors to perform actions are:

* Delegate: The behavior runner should activate behaviors which are passed to this function.
* CancelDelegates: All delegates which have been set by the behavior calling this function should be deactivated
* CancelSelf: The behavior which calls this function and all of its delegates should be deactivated.

IBehaviorRunner functions that allow behaviors to request information about their current state are:

* IsControlDelegated: Returns true if the behavior has delegated control to another behavior.
* CanDelegate: Returns true if the behavior has control and can delegate to other behaviors/actions.


Product specific implementaitons of each of these interfaces will be laid out in a later section.