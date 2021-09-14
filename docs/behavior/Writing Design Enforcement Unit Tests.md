# Writing Design Enforcement Unit Tests

Created by Kevin Karol Oct 19, 2017

## Background: Lessons Learned from Cozmo

When figuring out the high level behavior system for Cozmo there was a lot of back and forth about how to ensure that the end user could have reliable expectations about how Cozmo would respond to certain stimuli (seeing a block, being picked up etc). Eventually it was decided that "reactions" would be global behaviors that had to be explicitly disabled by behaviors they would interfere with. This then required additional systems to be built including a static assert system which required that every location in the codebase that interacted with reactions had to state whether it cared about every single reaction so that as new reactions were introduced they would be appropriately handled. In short, to ensure that Cozmo responded to designed stimuli extremely explicit knowledge about how the behavior system functioned had to be introduced into every single behavior, and changing anything about reactions became an arduous process.  For a more detailed discussion of why "reactions" do not function as a concept within the victor behavior component, see the page devoted to addressing that question.

One of the primary reasons why the global approach to reactions was preferred for Cozmo was that otherwise if a new behavior was introduced it would be easy to forget to add reactions and "Cozmo the robot" would no longer respond in the way "Cozmo the character" was supposed to respond. Fortunately, within Victor the ability to do full behavior tree walks has been introduced. This opens up the ability move requirements of how the behavior component is supposed to operate out of the way the component is implemented, and instead into a unit test that ensures design constraints are met. The advantages to this approach are:

1. It allows design constraints and reactions to be moved up the behavior tree. Instead of putting a reaction at the root of the tree and then giving the leaves 
2. It makes it easy to respond to changes in design requirements or special case scenarios because requirements aren't baked into the system

Using unit tests to enforce design requirements strikes a great balance between the flexibility of the system and the reliability of "character", but it requires that unit tests be written or updated when Design Constraints change.

## Example: Moving Reactions Down the Tree
As a practical example, let's consider how Cozmo's "reaction" concept can be implemented within the victor behavior system in a way that is reliable thanks to unit tests, but easier to update as designs change.


![](images/Moving%20Reactions%20Down%20Tree.png)

Figure 1: Global Reactions with Disables vs Reactions Included Where Appropriate

Figure 1 presents us with three reactions (A, B, C) and four behaviors (B1, B2, B3 and B4) where B1 can delegate to B2, B3 or B4 each of which supports different sets of reactions. One of the downsides of the global approach is that it requires all of the leaf behaviors to have full knowledge of what behaviors can be enabled and disabled. This makes an NxM matrix of disables that must be considered where N is the number of global reactions and M is the number of leaf behaviors that might not want global reactions to interrupt them. The reality is actually even worse than this because each behavior might have multiple states which have different disable sets, but we'll pass over that point for now. Within the leaf reaction approach the required knowledge is much cleaner - each leaf behavior only needs to know about the reactions that it wants to handle, and there could be an infinite number of other reactions which it doesn't have to worry about.

Of course the danger of the second approach is that if B5 is introduced as a delegate of B1 it would be easy to forget to include a reaction like React B which seems to be desired across pretty much all leaf behaviors. This is where design enforcement unit tests come in. If design decides that every leaf behavior MUST be able to handle Voice Commands, it's easy to write a test which walks through the behavior tree and asserts that every leaf must have a reaction hooked up for Reaction B. If it doesn't the unit test fails and the design constraint has to be resolved before the new behavior is introduced into master freeplay. However, if design decides that B5 actually is a special case that doesn't have to handle Reaction B, no code within the behavior system has to change, instead an exception is introduced into the unit test.

Having these unit tests in place also means that if pieces of Victor's behavior system are re-arranged or introduced it's not necessary to work through the full reaction matrix to ensure freeplay behavior is consistent. Instead, any behavior inconsistencies with the new tree configuration will be highlighted by the design unit test and once resolved it's assured that Victor will behave as he did with the previous behavior tree configuration.