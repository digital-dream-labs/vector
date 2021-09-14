# ARCHIVE: Cozmo -> Victor Behaviors: Major Changes Overview

Created by Kevin Karol Mar 08, 2018

If you're used to interacting with Cozmo's behavior system, you may find a couple of new functions, classes, and paradigms that don't jive with your current mental model in the Victor behavior system. Overall these changes provide more stability, power and safety checks to the behavior system, but they also place a couple of additional constraints or demands on individuals working within the system. Note that the changes outlined here are all written as though the change over to the new system was a clean cut. For more information about what you'll see within the actual code base please see the companion page ARCHIVE: The State of the Transition.

## The Good: Behaviors are basically the same
If you're used to writing behaviors that receive update calls and then start actions the good news is that all of that basically works the same way. If you're used to writing higher level state machines for activities, see the next section.

## The Really Good: Behaviors are also much more powerful
Activities have gone away because Behaviors now have the power to do all of the work that activities could and then some. This is because behaviors can now delegate to other behaviors, not just actions and helpers. An "activity" as it was used in the old behavior system is therefore just a behavior that controls other behaviors. Another huge advantage of this is that work which was done to support behaviors previously such as BehaviorChoosers can now be used at any level of the behavior system. As a result you can now have an activity with sub choosers that chooses activities that chooses behaviors that sometimes queues actions and sometimes sets up a queue of other behaviors to run. All of these are simply "behaviors" in the new system and the behavior tree/nesting behaviors is the preferred solution to many problems within the new system.

## The New: Lifecycle functions
One of the great benefits of the new behavior system is that behaviors get consistent and assured lifecycle function calls. Gone are the days of being unable to track certain pieces of information because you only get update calls when active or being unable to initialize systems you rely on for determining WantsToBeActivated. For a full overview of this new functionality see the Behavior Lifecycle Overview.

## The New: Delegation Instead of Pass Through
Within the Cozmo behavior system the behavior manager requested only the top of the behavior stack which it then ran functions on. The result was that the function call for ChooseNextBehavior passed through an arbitrary and unknowable number of levels in order to determine what behavior should be running at the top level, and then this "active" behavior was the only one which the Behavior Manager managed. Under the new system behaviors that are part of the process of determining what the robot should be doing must all be explicitly set within the Behavior System Manager (replacement for the Behavior Manager) through a process of delegation. A behavior says "My decision is, let this behavior decide" and then that behavior gets an update tick in which it can say "My decision is, perform this action or delegate to something else".


![](Delegation%20vs%20Pass%20Through.png)

Figure 1: Previous "Pass Through" system vs new "Delegation" system

As a result the Behavior System Manager can now 1) provide assurances that the Behavior Lifecycle Functions are called appropriately 2) Make it clear in debugging how decisions are being made and at what level behaviors are changing what's actively happening and 3) provide just one consistent function call instead of two similar but distinct calls (choose next behavior and update).

## The New: The Behavior External Interface
Robot is no longer passed into behavior functions. Instead there's a new class called the Behavior External Interface (BEI). Any time a behavior wants to interact with something outside of itself it must be through the BEI, whether that be delegating to a behavior/action or accessing information about current robot/world state. See the documentation Introduction to the Behavior External Interface for more background on this change and its benefits.


## The Gone But Not Forgotten: Global Reactions

Reactions don't exist in the new system. Yes, yes, I know that to most people the reaction (bah dum) to that statement is a great disturbance in the force, but I promise you there are still ways to do what you want to do, you just need to ask the question differently. Now that the full behavior tree is walkable and WantsToRunStrategies are easy to create and include in behaviors, it's possible to provide assurances of Victor's responses in many different scenarios without resorting to putting the leaves in charge of all reactions in the behavior tree through a complex and time consuming process of enabling/disabling which is still heavily error prone. 