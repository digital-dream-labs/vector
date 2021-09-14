# ARCHIVE: Behavior System Checklist

Created by Kevin Karol Oct 04, 2017 

**THIS PAGE IS NO LONGER ACCURATE/MAINTAINED. PLEASE SEE VICTOR BEHAVIOR SYSTEM HOME FOR UPDATED PAGES.**

## Summarizing Common Behavior System Bugs
There are implicit assumptions within the behavior system which are extremely hard to write tests for.  This document attempts to enumerate the common bugs that result from these implicit assumptions so that they can be proactively planned for as new features are introduced.  Before finalizing a new feature in the behavior system please run through the full checklist to make sure you've considered common edge cases.  The document is broken into sections based on where the bug originates, not based on the type of activity/chooser/behavior you're writing so be sure to review all parts of the checklist to make sure your feature is not susceptible to issues anywhere in the system.

## Activity Considerations
* Consider backpack lights/driving animations if the activity can be sparked.  For complex "behaviors" like BuildPyramid which are implemented as activities, what happens if that "behavior" needs to be sparked?  Activities generally assume they have persistent control over backpack lights or cube lights, but if an activity is being nested within another activity, make sure they're not waring for control.

* Make explicit decisions to exclude common behaviors only when they've been accommodated for in other ways.  Almost every activity should include DriveOffCharger, CantHandleTallStack and KnockOverCubes - without these behaviors Cozmo can get stuck with nothing to do.  If not included in a chooser make sure it's for very good reason (e.g. Cozmo doesn't need cubes for any behavior in the chooser)
* Do the available behaviors handle Cozmo carrying an object appropriately? If every behavior that can be chosen has CarryingObjectHandledInternally as false, and Cozmo switches activities while carrying an object he'll get stuck carrying an object with nothing to do.  Consider introducing a PutDownBlock behavior with low priority/score.

## Behavior Considerations
* What happens if Cozmo's carrying an object, is on the charger, or is off his treads? IBehavior manages all three of these cases in isRunnable - only CarryingObjectHandledInternally is a pure virtual function.  If you want to run while Cozmo is on the charger or is in the air, be sure that you override the appropriate functions.

* What happens when a behavior is interrupted by a resumable interrupt.  If a behavior has not implemented Resume it will be re-initialized after any interrupt - if there is a context dependent animation that shouldn't be repeated, ensure that there is some method of skipping it

* What happens when a behavior is interrupted by a non-resumable interrupt. If Cozmo is picked up or is interrupted for a while a behavior may be re-selected without the option to resume.  In this case if there is a particularly distinct animation that would be replayed, it might be necessary to separate out the animation into its own behavior and move logic up into an activity to maintain character continuity

* Passing data in to IsRunnable?  What happens on a resume? By default ResumeInternal calls IsRunnable with a robot - if this version of IsRunnableInternal isn't implemented for your behavior you'll hit a crash.  Be sure to override ResumeInternal to avoid this crash.

## Interaction with other behaviors
* Which reaction triggers should be disabled?  Ideally as few as possible to keep the behavior consistent. Many behaviors may want to disable some things, such as hiccups
* Should success of this behavior trigger a fist bump? If it the behavior has a BehaviorObjective, you can add an entry to reactionTrigger_behavior_map.json to add a probability that Cozmo will want to do a fist bump after successfully completing the behavior

