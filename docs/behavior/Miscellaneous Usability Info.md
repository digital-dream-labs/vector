# Miscellaneous Usability Info

Created by Kevin Karol Jan 11, 2018

The behavior system has too many specific components and functions that are constantly in flux to provide any sort of comprehensive written documentation. However, most components/functions should be reasonably self documenting. This page contains a scattershot explanation of some structural/navigational/legacy information which may not be immediately apparent when interacting with various ai/behavior components

## Following Folder Structure

A decent amount of work has gone into ensuring that when navigating the aiComponent folder structure it mirrors the layers laid out in Behavior Component Overview as closely as possible. Engine contains a folder called aiComponent which encompasses all functionality related to high level decision making. Within aiComponent is a behaviorComponent folder which encompasses functionality related to behavior selection, and within that folder is a behaviors folder that contains all the actual behavior definitions that the Behavior System Manager can run. This structure is mirrored within the engine resources folder. Being aware of this structure can make initial navigation of the ai/behavior system easier, and should be considered when introducing new components or behaviors into the system.

## Enumerated Entity/Component Throughout System

Time has also been invested recently to try and make it clearer what certain layers of the system do/contain/manage. Part of this effort has included converting core parts of aiComponent, behaviorComponent and behaviorExternalInterface over to an enumerated entity/component system. In each of these components there is an enum that defines all of the sub components they contain, and anyone with access to the component can then access its sub components by enumID and a type template. Thanks to a compile time requirement that each enum have a corresponding component declared it's easy to introduce new sub components into the system - just declare a new enum value (alphabetically please!), and then fix all the places that break. That's it!

## Where to Shove Things

The increased ease of adding sub-components throughout the system is intended to make code sharing across behaviors easier. Introducing some sort of selection logic or need storage that persists across multiple behavior instances? Go ahead and shove it in as a sub component so that work doesn't have to be duplicated across behaviors/programmers. Rule of thumb is to shove anything related to general decision making into aiComponent, anything that relates to behavior operation into behaviorComponent, anything that needs to be passed into behaviors but isn't managed by the ai/behavior component into the behaviorExternalInterface, and try not to put anything into behaviorSystemManager.

## BEIConditions: Making Data Driven Decisions Easier

BEIConditions are (currently) a simple way to encapsulate a high level concept about what state the robot is in. While at times these states can be programmatically accessed easily enough, one of the key utilities of BEIConditions is that they can be easily loaded from data. If you want to make a version of the play animation behavior that only runs when victor sees a face it's easy to add a wantsToBeActivated beiCondition entirely in data.  See Leveraging BEI Conditions for Data Driven Behaviors for a more in-depth overview.

## Behaviors Delegating to Behaviors

If a behavior wants to delegate to another behavior there are two ways that it can get a pointer to the other behavior. The first is through the behaviorExternalInterface's behaviorContainer component. The behaviorContainer maintains all behavior instances associated with a behaviorID (and by extension a resource file that provides parameters for the instance). Behaviors can also create an instance of the AnonymousBehaviorFactory if they want to make a runtime defined behavior which is inaccessible to any other parts of the system. Generally creating a behavior instance associated with an ID is encouraged so that the behavior can be re-used and updated in data rather than code, but AnonymousBehaviorFactory can be useful for quickly prototyping without having to make a bunch of new resource/class files.

## iCozmoBehavior "Operation" Functions

iCozmoBehavior provides a bunch of helper functions that make it easier to write behaviors quickly. Some of the more insideous helper functions iCozmoBehavior provides are virtual "operation" functions that return a bool defining certain properties about how a behavior should operate. E.g. Can this behavior run while Victor is on a charger? Can it run while he is off his treads? Can it run while he has a cube in his lift? Should iCozmoBehavior automatically cancel a behavior when it's not doing anything (default is yes it should since we assume we generally want victor doing something). Some of these functions are mandatory overrides, some assume default values.

These functions are likely to be condensed into a single function that receieves a "how to operate" struct behaviors can update soon, but for now if you're wondering why a behavior is doing something unexpected, check to see what the current bools returned by the "operation" functions are.

## BehaviorHelpers: Hanging on by a Thread

The Behavior Helper System has had its functionality totally subsumed be behaviors delegating to behaviors (which was not possible under the Cozmo behavior system), but is still useful enough in its current implementation that there hasn't been just cause to rip it out and re-implement it under the new BSM paradigm (yet).  Essentially Behavior Helpers are behaviors which will accomplish their goal (generally interacting with a cube) by ANY MEANS NECESSARY!!!! Practically speaking this means if you ask the behavior helper system to roll a cube that is underneath another cube, the helper system will identify this is an illegal request but instead of failling will actually pick up the cube on top, place it on the ground and then proceed to complete your intial request.

Feel free to use the Behavior Helper System - it works pretty much they way you'd expect the modern behavior system to work. However if substantial work is required to implement something in the behavior helper system it's probably preferable to implement it under the new BSM paradigm instead.

## Finally: Don't Blindly Trust Legacy Code

There have been a tone of changes to the behavior component over the past several months. While best efforts have been made to maintain legacy code, every behavior has not been tested and code reviews sometimes miss awful formatting or don't show practices which have been made obsolete but still compile and run just fine. If you see something that looks bizarre/cumbersome/out of step with this documentation (in a bad way not in a the code got so much better we decided explicit documentation was so 2017) don't assume it's right or should be copied. There's a good chance it's code that hasn't been touched in a year and a half and may lead you down a path of doing a lot more work than is required.