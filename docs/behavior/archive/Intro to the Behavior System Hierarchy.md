# ARCHIVE: Intro to the Behavior System Hierarchy

Created by Kevin Karol Oct 04, 2017

**THIS PAGE IS NO LONGER ACCURATE/MAINTAINED. PLEASE SEE VICTOR BEHAVIOR SYSTEM HOME FOR UPDATED PAGES.**

Cozmo's "Behavior System" is the part of Cozmo's AI which 1) takes a set of diverse inputs (emotion state, user requests, needs) and determines what actions Cozmo should take at any given moment and 2) ties together robot actions, animations, music, etc. into higher level continuity for the user.  It is most active during Freeplay when Cozmo's actions are driven by what he "wants" to do, but aspects of Cozmo's personality like Reactions and Voice Commands persist throughout the entire app experience to help Cozmo feel alive.  The number of inputs the system can process and outputs it can generate has resulted in a behavior system hierarchy that strives to bind related ideas together so that they are easier to think about and balance against each other.

This document will outline how this hierarchy is structured, and the baseline principles for determining how a new feature would fit into the existing system.  As the system is explained, keep in mind the key question for providing continuity for the user: under what circumstances do Cozmo's actions and the light/music/etc state need to persist?  Answering this question will guide where in the hierarchy a feature lives and how it should be implemented.  Let's start out by considering how Cozmo and his behavior system would fit into the world of human beings playing a game of futbol.

## An Analogy: Futbol

Let's say that Cozmo can engage in one of three sports - baseball, basketball and futbol.  We'll call each of these sports an activity.  What makes each of these as an activity?  If you tried to swap the balls between any of these games you would end up breaking the game on a fundamental level.  For Cozmo the equivalent might be control over the music state of the app, or setting lights on the cubes that should persist regardless of whether he's picking it up or being shaken by the user.

Once Cozmo has decided he's going to be playing futbol, he has to decide what he is going to do to help his team.  Perhaps Cozmo should run and try to get the ball.  Perhaps Cozmo has the ball and should pass it to a team mate.  Perhaps Cozmo should be a bad sport and storm off the field to yell at the ref.  Each of these options would be a behavior that Cozmo could run.  And just as in a game of futbol you might decide to pass the ball but then notice that the goalie has run to block the pass leaving the goal wide open, Cozmo can decide at any time that he wants to run a different behavior.  That's why every basestation tick the futbol activity would get a ChooseNextBehavior tick.  Cozmo can return the same behavior every tick if he wants to keep doing what he's doing, but whenever he changes his mind he returns a new behavior and the old behavior is stopped and the new behavior becomes active.

<hr/>

Let's take a moment to consider the differences between activities and behaviors as they've just been presented.  A behavior can be interrupted at any point in time because it's been determined that it would be better to run a different behavior.  On the other hand, the futbol activity lasts until the game hits some sort of end condition (90 minutes of play time, thunder storms in the area etc).  

As a result: defining the type of ball to be used for the game makes sense at the activity level, but not at the behavior level; activities tend to be sticky, while behaviors are ephemeral; and activities have their own rules that have little cross over, while behaviors from one activity might be useful in other activities (e.g. running to get a ball).

<hr/>

Back to the game!

As Cozmo is running after the ball he suddenly:

* Sees a snake in the grass and gets scared
* Gets knocked to the ground by an opponent
* Hears a friend call to him from the sidelines

All three of these cases are events which would interrupt Cozmo's current behavior in different ways.  The first two would be Reaction Triggers and the third is a Voice Command.  These interruptions have different effects depending on their severity.  Seeing a snake may cause Cozmo to play a scared reaction but then continue running right along, resuming his previous behavior.  Being knocked to the ground, on the other hand, fundamentally alters cozmo's world state and so he will be unable to resume his previous running behavior.  The same options apply to voice commands.

<hr/>

Let's zoom in on the process of running to get the ball to examine the mechanics of a behavior.  From a high level the process of running could be seen as a single process.  However, to actually run requires that one foot be placed in front of the other over and over again very quickly.  In Cozmo's world each of these steps would be implemented as an Action.  A behavior is what binds these individual steps together into the concept of running, and frequently variants of a behavior, such as different speeds of running by placing one foot in front of the other at a different speed can be different instances of the same behavior with the differences driven by data.  In this way the running behavior defines the concept of how actions relate to each other with the option to create multiple instances at different speeds/for different distances.

Although the action level provides very fine grained control over how the running behavior operates, there are times when a behavior might not care about the specifics of how a step is taken, and instead just wants Cozmo to reach a point where it can do something new.  So in the case of running after the ball maybe the behavior's desired logic is 1) Catch up to the ball 2) Give it a strong boot to get it out of the are 3) play a victory dace.  If the behavior could only operate directly on Cozmo's foot movement through actions it would have to handle every possible action failure on the way to trying to catch up to the ball.  Instead, if the behavior decides it doesn't care how the running happens, it can delegate the first stage of the behavior to a behavior helper.  These helpers have only two options - complete success or total failure.  As a result the behavior can say "catch up to that ball" and the helper does everything it can to make that happen.  But if it's impossible to do that the helper will fail and the behavior will know that there's nothing to be done and the behavior can't continue with what it initially set out to do.

<hr/>

As the game of futbol continues Cozmo will continue to decide the best behavior to run to help his team, will be interrupted by reaction triggers and voice commands, will perform actions directly and will delegate out more complex components of behaviors to behavior helpers.  At some point Cozmo will finish the futbol activity (or it will be interrupted by a strong input) and will select a new activity with different behaviors... and so the process continues.

<hr/>

## Behavior System Hierarchy
### Part 1: The Behavior Manager
Taking the concepts introduced in Cozmo's game of futbol, let's break down what these concepts look like in the engine.  At the very top of the hierarchy is the BehaviorManager which ticks all of the underlying components, manages the running behavior, and resuming behaviors that have been interrupted.

![](BehaviorManager%20Infrastructure.jpg)

Figure 1: Behavior Manager Infrastructure

The BehaviorManager provides the hierarchy two function ticks each basestation tick: ChooseNextBehavior and Update.  

`ChooseNextBehavior` is a const function with one job: to return a pointer to the behavior which should be running.  It's important to note that this is a function which is called every tick, not just when a behavior ends and a new one needs to be selected.  Behaviors are not sticky in the same way activities are - they can be stopped anytime for any reason.  In most cases ChooseNextBehavior will return the behavior that's currently running - but if something changes (a block disappears and the behavior can't run anymore, behaviors are being scored against each other and a different behavior gets a higher score) a different pointer is returned.  The BehaviorManager will then stop the current behavior and initialize the new behavior which was returned.

<hr/>

`Update` is a non-const function which allows activities and behaviors to alter properties of the experience such as cube lights, app music, robot backpack lights, etc.  All logic which changes system state should live within this function.

The diagram displays two concepts directly managed by the behavior manager - High Level Activities and Interrupts.  High level activities are the states that the behavior system can be put into through commands from the game.  Most of the time cozmo is in activityFreeplay - this class contains the logic for deciding between the various freeplay activities.  Other high level activities include MeetCozmo and Selection which allows the game to have its own logic about behavior selection and set them via CLAD messages.  Interrupts is a term which encompasses ReactionTriggers and VoiceCommands.  These systems are queried by the BehaviorManager each tick to determine whether they want to interrupt the currently running behavior.  If they do, the behavior manager stops the active behavior and activates the behavior the ReactionTrigger or VoiceCommand maps to until it completes.  Some interrupts (generally ones which are short and don't result in cozmo's world state changing) can be resumed from.  In this case the behavior that was running before the interrupt will resume, otherwise a new behavior will be selected through ChooseNextBehavior.

### Part 2: Activities

Let's zoom in to the activity level to see how an activity selects a behavior, and how a behavior selects the action which runs on the robot.

![](Activity%20Infrastructure.jpg)

Figure 2: Activity Infrastructure

The two functions that have been passed down from the behaviorManager (and possibly through a high level activity that contains multiple sub-activities like activityFreeplay) Update and ChooseNextBehavior feed into the activity, but only the Update tick makes it past the activity level. Activities have the option of implementing their own version of ChooseNextBehavior, or they can delegate the selection process out to a BehaviorChooser. A BehaviorChooser is a class that defines a generic method for deciding which behavior to run. e.g. the scoringBehaviorChooser selects the behavior which currently has the highest score (using cooldowns, emotion scores, added score while running etc), while the strictPriorityChooser will always select the highest priority behavior that can run. Activities can either implement their own ChooseNextBehavior function or specify a BehaviorChooser which will automatically be ticked for them (but not both).

Whether by specifying a or implementing a custom function, the activity is ultimately responsible for choosing what behavior should be running (except in the case of an interrupt), and is therefore also responsible for ensuring the continuity of how one behavior relates to the next.  For a deeper explanation of why this is important, see the Where does my feature fit? section below.

The other class in Figure 2 which was not touched on in the futbol example is the ActivityStrategy.  Every activity has a strategy which determines whether the activity WantsToStart and WantsToEnd.  The strategy is what makes an activity stickier than a behavior.  Within activityFreeplay  there is a strict priority of activities.  The highest priority activity that WantsToStart will be selected, and it will continue to run until its strategy says that it WantsToEnd, even if a higher priority activity WantsToStart.  By having this logic live in a strategy rather than the activity itself the process of selecting an activity is separated out from what the activity actually does.  This paradigm is mirrored by ReactionTriggerStrategies which contain all logic about whether an interrupt should happen and then map to the behavior that should be run when the strategy's requirements are met.

<hr/>

After the behavior that should be running is chosen, the behavior gets an update tick.  Before exploring the update tick, it's important to discuss iBehavior's StartActing and SmartDelegateToHelper function.  

`StartActing` is the preferred method for behaviors to queue an action to the robot's action queue.  The reasons why behaviors should use this are too implementation specific to go into in this document, but as a rule of thumb all behaviors should rely on StartActing to perform actions.  StartActing accepts a callback which will be called when the action completes so that the behavior doesn't have to monitor for action completions itself.  This relieves the update function from the necessity of monitoring action completions and starting the next action - almost all behaviors should follow the format of starting an action and passing in a callback which will then queue the next action etc.

`SmartDelegateToHelper` follows the same paradigm as StartActing in terms of delegation and callback, but as explained in the futbol example should be used when the behavior doesn't care about how something gets done, just that it does get done.  So while StartActing might be called with the PickupBlockAction, the behavior would be responsible for 1) making sure the block could be picked up 2) that the robot was at a pre-dock pose where the block could be picked up etc.  Delegating to the PickupHelper on the other hand could result in a long series of 12 actions wherein the block on top of the target has to be picked up, moved out of the way, put down, and then the target block is picked up.  The trade off is one of robustness vs control of robot action.

So with StartActing and SmartDelegateToHelper queuing actions and then receiving callbacks directly, what's the Update loop supposed to do?  Monitor for breaking changes.  E.G. the behavior starts a process, say picking up a block, and that action gets queued.  However, while driving to pickup that block, the robot sees another block which is closer to the robot and would result in the behavior completing more quickly.  The pickup block helper or action don't know why they're picking up a block, so they don't know whether this new block would be useful or not - that's up to the behavior to decide.  When the behavior gets its next update tick and sees that a closer block has appeared it can stop the current action, assign a new target block, and then start a new action to pickup that block.  This creates a clear separation within the behavior code of the logical path through a behavior's logic, and the conditions which can cause that path to change due to unforeseen circumstances (seeing a new block, the helper taking too long etc).

<hr/>

And that's it!  We've worked all the way down the hierarchy from the BehaviorManager, through the High Level Activities and Interrupts, to the activity strategy level where the activity that wants to run is selected.  Then with an activity running, there was the behavior chooser which said what behavior should be actively ticked, and then the behavior was given regular update ticks so that it could monitor world state while the action that it had either directly queued or the helper that it delegated off to were running.  Now Cozmo knows what action to perform!

<hr/>

## Where does my feature fit?

Although to users everything may appear to be a "behavior", there are important differences in the capabilities/scope of features that live at different levels of the behavior system hierarchy.  The key to figuring out where a feature should fit is in thinking about how closely ideas are bundled and how long they need to persist.

### Example 1: ReactToObjectMoved

Part of Cozmo's core personality is that he's connected to his cubes - when they move he should respond by turning to see if they're still in the same area.  As a result, this behavior should interrupt whatever cozmo's doing and turn towards the object's last location regardless of the current activity, so it is implemented as a reactionTrigger

### Example 2: Building a Pyramid

Although there is a "buildPyramid" behavior, as a user experiences it Building a Pyramid is actually implemented as an activity.  The key reason for this is that the cube lights persist throughout the experience, regardless of what behavior Cozmo is running.  If a pyramid base exists and Cozmo is searching for the third cube, the pyramid base lights need to continue marching around the outside of the cube.

### Example 3: Feeding

Feeding Cozmo is an activity because there is a required continuity between behaviors - 1) Cozmo requests food 2) the user prepares the food 3) Cozmo drives over to the food and consumes it.  However, when digging into the details the bundling of ideas becomes messier than it might appear at first.  For example, after the user prepares the food and Cozmo sees it he plays an excited reaction and drives over to the cube to consume it.  At first glance this would all appear to be part of a single "eating behavior" → 1) React 2) drive to cube 3) eat.  But what happens if in the middle of driving to the cube Cozmo falls off a table?  The eating behavior will be stopped, and when Cozmo is placed back on the table and sees the cube again it would restart.  The result would be the "excited to see food" animation playing twice.  In some cases this might be desired, but if the reaction is specifically "I'm excited you just prepared this for me" rather than "I'm going to go eat that now" playing the duplicate animation might result in breaking the continuity of behaviors.  To get rid of the duplicate animation the activity might need to play the animation as a separate behavior and then when the "eating behavior" starts/ is interrupted/starts again the activity would have maintained the continuity of behaviors.  This is why figuring out where your feature fits in the hierarchy is primarily a question of bundling ideas together appropriately.

## Conclusion

The behavior system is responsible for selecting Cozmo's current action and controlling cube/music state during freeplay.  It manages this responsibility through a hierarchy that allows related concepts to be bundled based on the desired persistence of the feature.  When interacting with each level of the behavior system it's important to understand how the active part of the system is selected (strategies which are sticky, checked every tick to see what the best thing to run is), and the conditions under which that part of the system maintains control of Cozmo.  Many parts of the system present a trade off between robustness by delegating control away or fine grained control over Cozmo's actions, but generally delegating and monitoring provides a clear separation between the flow of behavior logic and the conditions under which the behavior will change course.

Now that you have an understanding of the behavior system's architecture, be sure to check out the Behavior Checklist page when implementing new features to ensure you've considered common edge cases behaviors encounter.  And go outside and play some futbol!