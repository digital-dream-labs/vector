# Leveraging BEI Conditions for Data Driven Behaviors

Created by Kevin Karol Jan 11, 2018
As discussed in How Data Drives Behaviors most behaviors are data defined instances of behavior classes. A distinction is also made between "Core" behaviors and "Designed" behaviors where a "designed" behavior is generally a one off use case that has more to do with character/light interactions than it does a core "robotics" functionality that can be re-used in multiple locations. However, there is another reasonably well defined set of behaviors that individuals working with the behavior system will often encounter or be asked to write. These behaviors are "designed" responses to some aspect of character or world state that tend to be described along the lines of "play an animation when you see an object". It seems wasteful to write a full new "designed" behavior class in order to accommodate what boils down to "run X behavior when Y condition is met". This is a great opportunity to leverage BEI Conditions to add a data defined modifier onto a "core" behavior.

What follows is an overview of an idealized process that keeps both behaviors and beiConditions as general purpose and re-usable as possible. It's very possible that the specific requirements for your behavior make it nearly impossible to have a clean, entirely data defined, implementation. In that case feel free to bail out of the advice this page provides and hack your behavior into a one off .cpp implementation in order to keep making progress

## What is a "BEI Condition"?

BEI Condition stands for "Behavior External Interface Condition". Each class that derives from the beiCondition interface must implement one function - AreConditionsMet which accepts a Behavior External Interface and returns a bool. In effect the class is responsible for answering a single yes/no question based entirely on the data contained within a Behavior External Interface. beiCondition source files live within the aiComponent - existing conditions can be referenced by a BEIConditionType which is defined in clad and created anywhere within the code base using the beiConditionFactory. To create a new beiCondition simply introduce a new type to the clad file, a new source file within the conditions folder that inherits from iBEICondition, and tie the two together within the beiConditionFactory.

## How do I use BEI Conditions in a Behavior?

Conditions can be created and used anywhere within the code base to make decisions. If a behavior has a branch point and wants to choose the most appropriate action as a result of a BEICondition it's easy to generate a condition with the factory and then ask whether its conditions are met or not with no knowledge of the internal decision making process.

It's even simpler to utilize beiConditions to make data defined behaviors that operate under the "run X when Y" formula laid out above. iCozmoBehavior accepts a data defined list (currently called "wantsToBeActivatedCondition") of beiConditions to be evaluated on each "wantsToBeActivated" lifecycle function call. Adding a beiCondition to this list means that a behavior will not run unless all conditions in the list are met.

Therefore, to create a data defined behavior of the form "run X when Y":

  1) Identify or write a "core" behavior X that is re-usable (e.g. turn to face and play animation)

  2) Identify or write a beiCondition/s that fulfill requirement Y (e.g. true when 2 blocks exist in block world )

  3) Create a data defined instance of X which lists beiCondition Y as a wantsToBeActivated requirement