# BEI Conditions

BEI condition stands for Behavior External Interface Condition. Conditions should be simple, minimally stateful, classes that answer exactly one true/false question: whether the condition encapsulated is true.  Classes interacting with BEI Conditions will ask this question by calling the function AreConditionsMet and passing in a Behavior External Interface. This core interface is implied in the base class name (BEI implying that the condition answers true/false based on data found in the Behavior External Interface), and also means that generally BEIConditions are primarily useful within the behavior system where the BEI is easy to access.

## Common Uses
  * The BEI Condition Factory makes it easy to load conditions from data.
  * Data defined behaviors often use BEI Conditions as WantsToBeActivated conditions. By adding the condition type to a behavior instance's JSON file the behavior will only want to be activated if the condition is true. This supports the seperation of what the robot is "reacting to" from how the robot "reacts".  "Reacting" to being picked up can be implemented as a "Play Animation" behavior instance with a "Picked Up" WantsToBeActivated condition. A more detailed overview of this practice is available [on confluence](https://ankiinc.atlassian.net/wiki/spaces/CBS/pages/199917569/Leveraging+BEI+Conditions+for+Data+Driven+Behaviors)


## Best Practices
  * <b>Consider creating a compenent instead:</b> Since conditions require callers have access to a BEI, they are a poor source of truth for useful information. Many BEI conditions are light wrapper classes that call out to a robot/ai component. This allows other parts of engine to check the same state/condition information without requiring a custom BEI.

[BEI Conditions Folder](/engine/aiComponent/beiConditions/)
