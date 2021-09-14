# ARCHIVE: So You Want To Write A Behavior?

Created by Kevin Karol Oct 04, 2017 

**THIS PAGE IS NO LONGER ACCURATE/MAINTAINED. PLEASE SEE VICTOR BEHAVIOR SYSTEM HOME FOR UPDATED PAGES.**

If this is your first time interacting with the behavior system, be sure to start out by reading the page ARCHIVE: Intro to the Behavior System Hierarchy. 

## Are you sure it's a behavior you want to write?
Behaviors bind together actions and light states which can be interrupted at any point in time and should be able to resume/fast forward/respond to whatever the world state is when they say they can run and are selected by an activity.  If you want something with greater persistence or which manages continuity across a larger timespan, consider implementing an activity.

## The Logistics of Behaviors
### How data drives the behavior system
Although behaviors and activities are implemented in C++, they are generated and driven by json defined data.  This allows variants of behaviors (which differ only in the animation to play, driving speed, etc) to be instances of the same class with data defined differences.  All data for the behavior system lives within `resources/config/engine`.  `activities_config.json` is loaded in by the behaviorManager and defines the high level activities.  Most of the time Cozmo will either be in Selection (where games can directly call behaviors/animations) or Freeplay (where he decides what to do himself).

The freeplay high level activity has a strict priority list of sub-activities which it selects between.  These activityIDs link to files of the same name within the activities subdirectory.  Some of these activity files specify behavior choosers that will then directly include behavior by ID.  All of these behaviors are defined in the behaviors subdirectory.  So the overall resource hierarchy of includes is activities_config → activities → behaviors

### How behaviors are created
Behaviors are read into the BehaviorManager and created using a behavior factory at the start of the app run.  Each behavior json is required to have two fields specified: the behaviorClass which identifies the class that the behavior should be an instance of, and a behaviorID which is a unique identifier for this instance of the behavior.  All other fields specified within the json file are the data that iBehavior or the child behavior class rely on to set their internal parameters. 

### How behaviors are loaded into activities/choosers
Once all behaviors have been created by loading in the json files and passing them to the behavior factory, the factory maintains a map of the behaviorID to the behavior pointer.  Any other parts of the system which want to access the behavior should access the factory and request the behavior by ID to receive the behavior pointer.

Behavior choosers specified in activity files will automatically grab and manage the behaviors for the activity.  However, if an activity needs to manage a behavior itself (in order to pass in data/access behavior specific functions) it can use the behavior factory's FindBehaviorByID function.



## Writing your first Behavior
Behavior classes should be subclassed from the iBehavior class and defined within the engine/behaviorSystem folder.  

### Key Functions
iBehavior has a number of key methods which behavior classes can use or have to override which when understood make the process of writing a new behavior extremely straight forward.

#### Constructor
The constructor for all behaviors receives all the json data read in from the behavior's json file.  It's good to have:

1) A single param struct that contains all data driven parameters that will be read in.  This helps make clear the behavior's properties that are essentially const vs member variables that are actively maintained throughout the behavior's runtime.

2) Use json parsing which asserts when a property is undefined in most cases.  Unless there's a strong default option (in which case should it maybe be a const instead of loaded from json?), it's better to let someone know about a property the've forgotten then to let the property go unset accidentally the next time an instance is created.

#### IsRunnableInternal

Within iBehavior there are multiple instances of the IsRunnableInternal function.  While not pure virtual, every behavior must implement at least one of the variants.  Most behaviors will want to override the version which passes in no preReqs or a robot, but if an activity needs to load special data into a function one of the other IsRunnables may be used to pass in the necessary data.  The key is that if an activity or chooser tries to use an IsRunnableInternal that the behavior doesn't support a runtime assert will be hit.

IsRunnable can be ticked every tick, so it's important to keep it as efficient as possible.  If high computational costs are necessary, it's good to restrict it to a custom timestep as opposed to performing the operation with the frequency at which IsRunnable is ticked.  If IsRunnable is ticked and returns true, the behavior will be Initialized and start running that same tick.

Finally, one of the dirty secrets of the behavior system is that although IsRunnableInternal is a const function, it's also the only way that we have to pass information directly into a behavior.  So many behaviors have mutable member variables (which is admittedly awful).  If you need to store data passed in to IsRunnable follow this paradigm for the time being since you have a guarantee of being run immediately, and then that information can be used as part of InitInternal .

#### InitInternal
The init function provides behaviors the opportunity to set up any lights they need and start their first action with the StartActing call (see below).  Note that the behavior's first action should be started from this function (or by calling the behaviors first function that contains an action), not from the first tick of UpdateInternal.

Most behaviors support the idea of "fast forwarding" i.e. when the behavior is initialized they should identify the furthest point in the behavior that they have the preconditions met for, and jump immediately to that part of the behavior's logic.  For example, in the stack blocks behavior, if the behavior starts and Cozmo already has the first block in his lift the behavior should "fast forward" to the action that drives him over to the block he's going to stack on top of.  Behaviors which are designed to be a state machine instead of a linear series of actions should contain all transition/fast forwarding logic within a single function that checks state and determines what function to call next rather than maintaining the state machine within the update function.  For a deeper explanation of why/how a state machine based behavior should operate see the StartActing/Update function below.

#### ResumeInternal
When a behavior is interrupted by a reaction or voice command which is short and/or doesn't change Cozmo's world state the behavior will be "resumed" instead of "initialized".  The default implementation of Resume is just to call init, and if possible it's better to go through init whenever possible and have the behavior keep track of relevant world state to where it should start the behavior (e.g. the last time the behavior ran - it's now starting again 3 seconds later, therefore don't play the first animation again) rather than making assumptions about why the behavior is being resumed.  However, if special considerations are needed, ResumeInternal is a thing.

#### StartActing/UpdateInternal
Most of a behavior's responsibility is to select the next action that the robot should perform be it an animation, interacting with an object, or following the user's face.  To add this action to the action que all behaviors should go through the StartActing function.  Going through this function rather than queuing directly to the action queue and monitoring for action results ensures that the behavior respects requests from other parts of the system such as ending after the next action is complete.  This functionality is hidden away at the iBehavior level so that behaviors don't have to worry about it, but if a behavior doesn't use the StartActing function there are requests that will silently fail.

With that in mind, the StartActing function is extremely powerful.  In addition to specifying the action to queue, it also allows behaviors to pass in a callback lambda or member function that will be called when the action completes.  There are variants of the StartActing function which will pass the callback references to the robot, action results etc.  Most behaviors operate by chaining together StartActing calls that have the next member function passed in as a callback and so forth.  This paradigm ensures that the logical chain of the actions the behavior will perform is clear and that all assumptions other parts of the system make about behaviors will be followed properly.

So what is the UpdateInternal function for?  In its most basic implementation UpdateInternal says that if the robot is "acting", i.e. performing an action, then the behavior's running, and whenever the behavior doesn't queue a new action, then it must be complete.  But for more complex behaviors that may need to respond to new information in the world UpdateInternal provides the opportunity each tick to determine if the behavior wants to stop what it's currently doing and pick a new action.  

For a behavior like StackBlocks this might mean that the behavior has seen a block which is closer than the one it planned to stack on top of.  As a result it might stop driving to the further block, and start driving to the new block to save the behavior time.  StartActing doesn't call its callback until the action is fully complete, so it doesn't allow actions to be interrupted, but UpdateInternal can stop the current action (generally without the callback) and then start a new action.  By having UpdateInternal monitor for interrupts it effectively becomes an enumeration of the conditions under which a behavior's current plan breaks.  This clear division between the roles of StartActing with callbacks, which is responsible for laying out the expected chain of logic the behavior will follow, and UpdateInternal, which enumerates the conditions under which that chain of logic will break, makes it easy to reason about all possible paths the behavior might follow.

Whenever possible this division of having UpdateInternal only contain logic that 1) needs to be checked every tick and 2) is checking for breaking conditions in the behavior's current action, should be maintained.  In the event that a behavior needs to be more "dynamic" and not follow a linear path through StartActing calls, a separate state machine function should be used rather than having a special conditional portion of the Update loop.  This state machine can then be passed in as the callback to all StartActing functions, evaluate the current world state/behavior condition and determine the next phase of the behavior without breaking the Update/StartActing division of labor.

#### "Smart" functions
There is a class of functions within iBehavior which are prefaced by the term "Smart" (e.g. SmartDisableReactionsWithLock, SmartDelegateToHelper).  These functions are "smart" in that iBehavior will clean up their effects when the behavior is Stopped.  They were introduced to alleviate the need to track state for most behaviors.  Since behaviors can be interrupted at any point in time prior to smart functions if a behavior disabled reactions for only one StartActing call it would have to track whether or not it was currently in that phase and then remove the disable if and only if it was stopped while in that stage.  Smart functions alleviate this tracking by simply saying "if I have something when the behavior is stopped, clean it up".  In general, use smart functions when you can. 

### Creating a behaviorClass/behaviorID/JSON File

Once you've figured out the functionality of your new behavior, the final stage is to set up the necessary components so that the behavior is actually loaded into the factory when the app starts up:

1) Introduces a behaviorClass and behaviorID - in the behaviorTypes.clad file there are two enums  which you will likely have to update.  BehaviorClass should match the new .cpp class you've created with the behavior implementation, and behaviorIDs identify the unique instances of the new class you're going to create.  Once you've introduced your new enums (please keep the enum alphabetized), regenerate clad.  The behavior factory should now fail to compile until you introduce a case for your new BehaviorClass enum which maps the enum to creating a new instance of your class.

2) Create jsons for your behavior - in the behavior resources folder create a new JSON file with a file name that matches the behaviorID you created above.  Specify the behaviorClass and behaviorID in the behavior file.  Presto!  On the next app run the behaviorManager will read your JSON file, pass the class into the factory to create the new instance of your class, and map that instance of the behavior to your behaviorID

## An Introduction to BehaviorHelpers

A final tool which is useful for quickly creating robust behaviors is the BehaviorHelper system.  BehaviorHelpers encompass a task (pickup an object, place an object, etc.) and are similar to actions with two key differences:  

1) Helpers can perform a large number of individual actions in order to try to accomplish the task they're assigned.  If a behavior tells a helper to roll a block, and that block is beneath another block, the helper may pickup the top block, place it on the ground, and then roll the bottom block.  By giving up the finer grained control over exactly what actions will be performed behaviors gain a level of robustness that would be difficult to accomplish if it implemented custom solutions to every possible edge case.

2) Helpers only fully succeed or completely fail.  There is no in-between.  Where actions have a large enumeration of the reasons why a failure happened, helpers provide no feedback to the behavior since they may have attempted any number of actions that the behavior has no knowledge of.

This method of delegating to helpers which the behavior gives up any direct control over is a deeper version of the standard StartActing/Update work divide that most behaviors follow.  The behavior should be monitoring in its update loop for conditions in which it wants to take control back from the helper (e.g. it's taking too long to accomplish the task), but should otherwise allow the behaviorHelper to operate without any input from the behavior.

## Miscellaneous Behavior Style
* When possible define constant properties as static const within an anonymous namespace in the .cpp file.  Following Anki's style guidelines this should be prefaced as kVariableDescription.  This is preferable to having a const member variable.
* Use the macro DEBUG_SET_STATE to identify the current state of the behavior.  The string version of the state passed in will be displayed in Webots and be printed in the log to help debugging.  If your behavior needs to use state information make a custom version of SET_STATE which assigns the state to your state member variable and then sets the debug state name at the iBehavior level
