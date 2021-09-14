# Behavior system

* High level behaviors coordinate overall activity, delegate to other behaviors
* Lower level behaviors control the robot by delegating to actions or helpers
* Multiple behaviors are *active* in the stack, only the last one is *in control*
* Behaviors which might run are in *activatable scope* 

**The behavior system documentation is [on confluence](https://ankiinc.atlassian.net/wiki/spaces/CBS/overview).**
Look there for details, this is just a tiny overview.

The behavior system is the high level AI system that control's victors actions. At any given time, a "stack"
of behaviors can be active, managed by the Behavior System Manager. Each behavior in Victor is of type
`ICozmoBehavior`. Behaviors can then delegate control to [Actions](actions.md) or other behaviors.

On Victor, if you are trying to make the robot do something from engine, then (99% of the time) you need to be
using a behavior

## Active Feature

The behavior system is also responsible for determining what the current _active feature_ is. This informs the
feature that will appear in the status-log on app, as well as the data which is sent to DAS. Behavior json
files can specify an "associatedActiveFeature" to declare that they (and all delegates that don't have their
own associated features) are associated with a given feature enum (from
[activeFeatures.clad](/clad/src/clad/types/behaviorComponent/activeFeatures.clad)). All branches of the
production behavior tree must have an active feature defined (or explicitly declare
ActiveFeature::NoFeature). This is checked by the `DelegationTree.CheckActiveFeatures` unit test.

## Running behaviors

The default behavior is set in [victor_behavior_config.json](/resources/config/engine/behaviorComponent/victor_behavior_config.json)

Using [webots](/simulator/README.md) you can run a behavior by pasting the ID (see [behaviorIDs.clad](../../clad/src/clad/types/behaviorComponent/behaviorIDs.clad)) into the `behaviorName` field and pressing "Shift+C"

## Examples

1. The very simple ["observing without turn"](/engine/aiComponent/behaviorComponent/behaviors/observing/behaviorObservingWithoutTurn.h) behavior
   which simply looks up and down with random delays (and can run on the charger)

2. The ["come here"](/engine/aiComponent/behaviorComponent/behaviors/simpleFaceBehaviors/behaviorDriveToFace.h) behavior
   in response to a voice command which turns and drives towards a face, handling some edge cases
   
## Basic concepts

* All behaviors specify all other behaviors that they may possibly delegate to in `GetAllDelegates()`. The possible delegates of any `Active` behavior are `InActivatableScope`
* All behaviors which are `Active` or `InActivatableScope` get an update tick `ICozmoBehavior::BehaviorUpdate()`
* Following the possible links from `GetAllDelegates()` starting from the base behavior gives you a tree of all possible behaviors that can run
* Only the behavior at the end of the stack can delegate to actions or behaviors. All other active behaviors have "control delegated" (`IsControlDelegated() == true`)
* Behaviors that have delegated control can cancel themselves or the behaviors they delegated to
* At least one behavior is always active. There is a `Wait` behavior that does nothing, but there is never "no behavior"
* Behaviors will only become active if `WantsToBeActivated()` returns true. Note that there are some other requirements here like `ShouldRunWhileOnCharger()`. See
  `ICozmoBehavior::WantsToBeActivated()` for details
* All behavior instances have a `.json` file which specifies a `BehaviorClass` and `BehaviorID` from `behaviorTypes.clad`, as well as other parameters

## Script-generated CLAD and behavior container files

To ease the manual work of creating new behaviors, [createNewBehavior.py](/tools/ai/createNewBehavior.py)
exists. This script can be run (interactively or using command line args) to create a new behavior class (.cpp
and .h file). This script itself will then run [generateBehaviorCode.py](/tools/ai/generateBehaviorCode.py),
which scans the appropriate json and code directories, and automatically generates BehaviorClass and
BehaviorID CLAD entries, as well as an entry into the behavior container to allow creation of the
factory. [generateBehaviorCode.py](/tools/ai/generateBehaviorCode.py) can also be run manually if you create
your own behavior files.

Note that the file name, class name, and BehaviorClass CLAD enum are all linked. Specifically, if you were to
create a new behavior for doing the dishes, you'd create `behaviorDoDishes.h` and `behaviorDoDishes.cpp` in
the appropriate directory (and subdirectory to keep things organized). Inside those files, the behavior C++
class must be called `BehaviorDoDishes`. Then, the python script will automatically create
`BehaviorClass::DoDishes` in CLAD, and update the container to create this behavior automatically. If you add
a new instance file called `doDishesAfterDinner.json` and inside declared it to have a behavior ID
`"DoDishesAfterDinner"`, then the python script would also create `BehaviorID::DoDishesAfterDinner`
automatically.

The scripts also have some checks to ensure that the above conventions are matched and that json files match
known behavior classes.

## See also

[User/cloud/app intents in the behavior system](/docs/architecture/behaviors_intents.md)

You can run the [plotBehaviorTree.sh](/tools/ai/plotBehaviorTree.sh) function with no arguments to see the entire tree, or give it a filename as the single argument to output a pdf of the tree. Look at that script to see more about underling arguments and how to plot other things. Note that plotting the behavior tree relies on artifacts from running the engine unit tests. You can run engine unit tests with [project/buildServer/steps/unittestsEngine.sh](/project/buildServer/steps/unittestsEngine.sh).
