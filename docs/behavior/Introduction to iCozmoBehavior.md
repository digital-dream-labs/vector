# Introduction to iCozmoBehavior

Created by Kevin Karol May 30, 2018

iCozmoBehavior is the Victor specific implementation of iBehavior. It provides a bunch of helper functionality that behaviors can use to write code that is readable and flexible without getting caught up in the weeds of storing/checking properties themselves.

## Key Functions

If you haven't read the Behavior Lifecycle Overview, take a minute to review the lifecycle functions laid out there. iCozmoBehavior implements a final version of most of the functions specified in iBehavior, but then has Behavior specific sub-functions which are listed below.

### Constructor

The constructor for all behaviors receives all the json data read in from the behavior's json file.  It's good to have:

1) A single param struct that contains all data driven parameters that will be read in.  This helps make clear the behavior's properties that are essentially const vs member variables that are actively maintained throughout the behavior's runtime.

2) Use json parsing which asserts when a property is undefined in most cases.  Unless there's a strong default option (in which case should it maybe be a const instead of loaded from json?), it's better to let someone know about a property the've forgotten then to let the property go unset accidentally the next time an instance is created.

### Behavior Lifecycle
Functions: OnBehaviorActivated, InitBehavior, WantsToBeActivatedBehavior,  BehaviorUpdate, OnBehaviorDeactivated

These functions are the behavior specific implementations of the iBehavior lifecycle functions

The *OnBehaviorActivated* function provides behaviors the opportunity to set up any lights they need and start their first action with the *DelegateIfInControl* call (see below).  Note that the behavior's first delegation should be called from this function (or by calling the behaviors first function that contains an action), not from the first tick of *BehaviorUpdate.*

Most behaviors support the idea of "fast forwarding" i.e. when the behavior is initialized they should identify the furthest point in the behavior that they have the preconditions met for, and jump immediately to that part of the behavior's logic.  For example, in the stack blocks behavior, if the behavior starts and Cozmo already has the first block in his lift the behavior should "fast forward" to the action that drives him over to the block he's going to stack on top of.  Behaviors which are designed to be a state machine instead of a linear series of actions should contain all transition/fast forwarding logic within a single function that checks state and determines what function to call next rather than maintaining the state machine within the update function.  For a deeper explanation of why/how a state machine based behavior should operate see the *DelegateIfInControl/BehaviorUpdate* function below.

### DelegateIfInControl/BehaviorUpdate

Most of a behavior's responsibility is to select the next action or behavior that the robot should perform be it an animation, interacting with an object, or following the user's face.  To delegate to this action or behavior all behaviors should go through the DelegateIfInControl function.  Going through this function not only provides conveniences like callbacks, but it also allows other parts of the behavior system to have insight into what the behavior is currently doing.

With that in mind, the DelegateIfInControl function is extremely powerful.  In addition to specifying the action or behavior to delegate to, it also allows behaviors to pass in a callback lambda or member function that will be called when the action completes. Most behaviors are built by chaining together DelegateIfInControl calls that have the next member function passed in as a callback.  This paradigm ensures that the logical chain of the actions the behavior will perform is clear and that all assumptions other parts of the system make about behaviors will be followed properly.

So what is the *BehaviorUpdate* function for?  In its most basic implementation BehaviorUpdate says that if control is delegated, then the behavior's running, and whenever the behavior doesn't have control delegated, then it must be complete.  But for more complex behaviors that may need to respond to new information in the world BehaviorUpdate provides the opportunity each tick to determine if the behavior wants to stop what it's currently doing and pick a new action.  

For a behavior like StackBlocks this might mean that the behavior has seen a block which is closer than the one it planned to stack on top of.  As a result it might stop driving to the further block, and start driving to the new block to save the behavior time.  DelegateIfInControl doesn't call its callback until the action is fully complete, so it doesn't allow actions to be interrupted, but BehaviorUpdate can cancel its delegate (generally without the callback) and then start a new delegate.  By having BehaviorUpdate monitor for interrupts it effectively becomes an enumeration of the conditions under which a behavior's current plan breaks.  This clear division between the roles of DelegateIfInControl with callbacks, which is responsible for laying out the expected chain of logic the behavior will follow, and BehaviorUpdate, which enumerates the conditions under which that chain of logic will break, makes it easy to reason about all possible paths the behavior might follow.

Whenever possible this division of having BehaviorUpdate only contain logic that 1) needs to be checked every tick and 2) is checking for breaking conditions in the behavior's current action, should be maintained.  In the event that a behavior needs to be more "dynamic" and not follow a linear path through DelegateIfInControl calls, a separate state machine function should be used rather than having a special conditional portion of the BehaviorUpdate loop.  This state machine can then be passed in as the callback to all DelegateIfInControl functions, evaluate the current world state/behavior condition and determine the next phase of the behavior without breaking the BehaviorUpdate/DelegateIfInControl division of labor.

### AlwaysHandleInScope/HandleWhileActivated/HandleWhileInScopeButNotActivated

The behavior external interface provides all behaviors with information about the behavior events that have happened over the last tick within the behavior's update function. However, for message handling iCozmoBehavior provides some convenience functions in case there's a desire to have a division between event handling and the behavior's update function implementation. By overriding the AlwaysHandle function a behavior will receive just the specific type of events specified by the function decleration that iCozmoBehavior extracts from the behavior external interface for the programmer. Note: These events are still accessible through the Behavior External Interface's Behavior Event Component, even if AlwaysHandle is implemented.  There is no need to check the values twice.

### "Smart" functions
There is a class of functions within iBehavior which are prefaced by the term "Smart" (e.g. SmartDisableReactionsWithLock, SmartDelegateToHelper).  These functions are "smart" in that iBehavior will clean up their effects when the behavior is Stopped.  They were introduced to alleviate the need to track state for most behaviors.  Since behaviors can be interrupted at any point in time prior to smart functions if a behavior disabled reactions for only one StartActing call it would have to track whether or not it was currently in that phase and then remove the disable if and only if it was stopped while in that stage.  Smart functions alleviate this tracking by simply saying "if I have something when the behavior is stopped, clean it up".  In general, use smart functions when you can. 