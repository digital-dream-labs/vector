# ARCHIVE: System Refactor: Goals -> Freeplay Activities

Created by Kevin Karol Oct 04, 2017

** THIS PAGE IS NO LONGER ACCURATE/MAINTAINED. PLEASE SEE VICTOR BEHAVIOR SYSTEM HOME FOR UPDATED PAGES.**

This page is for individuals who have interacted with the behavior system's previous "goal" or "behaviorChooser" systems.  These systems are being renamed and refactored for a clearer delineation of duty and greater clarity when interacting with the behavior system for the first time.  However, for those of us used to interacting with the previous system, this page should serve as a transition guide to find what you're looking for.

## Goals are now called Activities

We'll get into the specifics of why after discussing the change in relationship between Activities and BehaviorChoosers, but for clarity sake from hereon in there shall be no mention of goals ever again. (or else!)

## What belongs in an Activity, what belongs in a behavior chooser?

At the core of the behavior system are, logically, behaviors.  A behavior defines the actions cozmo should take, and can alter cube/backpack lights and user facing world state.  The problem is that behaviors have no guarantee of persistence - they can be interrupted at any time.  As a result, when we started building behaviors that required persistent state over longer periods of time, such as pyramid, we needed to move control of lights to something which was more persistent.  At the time all that we had were behavior choosers, so we started cramming more and more functionality into behavior choosers.  The result?  Something named a "behavior chooser" was now not only choosing behaviors, but also changing music state and monitoring lights and block world configurations.  Pretty confusing right?

So we'd like to move this sort of persistent world state maintenance out of the choosers.  That's where Activities come in.  Formerly, saying that a goal was running didn't communicate that cozmo was engaging in something that persisted longer than a behavior, it sounded as though he was working towards some specific end.  With an Activity it becomes clearer that cozmo is engaging in a longer term task and it makes sense that anything which should persist for that same time period lives within the activity, not the chooser.

As a result, when making a larger activity for cozmo to engage in an Activity should contain:

    1) Any world state/music/cube light/backpack light maintenance which spans across behaviors and/or shouldn't be interrupted

    2) If custom chooseNextBehavior logic logic is necessary, it should be implemented by the activity directly, as opposed to being delegated to a chooser

And a chooser should be:

   1) A generic method of choosing which behavior comes next (e.g. The highest scoring behavior, one at a time in an order until a BehaviorObjectiveAchieved is received etc).  

No custom logic should be put into a behavior chooser - it should only be generic strategies for choosing between behaviors.

## Implementing a new behavior?

A couple of major changes have happened to the flow of creating a new behavior:

1) Behavior groups are no more - Behaviors are now included directly in behavior choosers/activity json files by name rather than being included in a group that the chooser loads in

2) Behavior scoring now lives in the chooser - Along with behavior names being used to include behaviors in choosers, scoring is now specified in the chooser rather than the behavior's data file.  Note: Due to implementation details even though this change appears to allow behaviors to have different scores in different choosers, this is not the case.  All scored choosers that include a behavior by name must have the same score or an assert will trigger.

3) Behavior names have been replaced with BehaviorIDs which must match file names - All behavior json files must now declare a behaviorClass which indicates the behavior they are instantiating with the data the file contains and a BehaviorID which is a unique identifier of that instance.  When creating a new instance introduce the BehaviorID in clad, create a new file with a matching name and specify the BehaviorID.  This makes the process of FindingBehaviorsByID, ExecutingBehaviorsByID and choosers/activities including behaviors by ID much more stable.

## But where's that file I'm looking for?

Major restructuring/renaming as part of this refactor:

   1) behavior_config.json is now activities_config.json.  This is where activity priority is specified, but actual activity parameters should be specified in their own files within the new activities folder.

   2) AIGoalEvaluator is now called ActivityFreeplay - this is the activity that manages all of the sub-activities within freeplay

   3) All of what used to be called "goals" are now called activities/activityStrategies and they have their own folder in the behaviorSystem instead of living with the behaviorChoosers