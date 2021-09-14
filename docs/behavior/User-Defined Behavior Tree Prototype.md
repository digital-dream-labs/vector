# User-Defined Behavior Tree Prototype (AI Intern project 2018)

Created by Hamzah Khan Aug 09, 2018

As part of the desire to be able to personalize robots, the AI Intern project for 2018 resulted in a system that enables users to select which behavior they would like Vector to respond to upon hitting a specific trigger, such as detecting a cliff, seeing a face, etc.

## Instructions for Using the Feature

At the time this document was written (8/9/18), the user-defined behavior tree was not defined in the codebase as an ActiveFeature, and so can not be enabled or disabled with feature flags. The feature can be enabled as follows.

1. Edit /resources/config/engine/behaviorComponent/behaviors/victorBehaviorTree/globalInterruptions.json by adding the behavior UserDefinedBehaviorTreeRouter to the front of the list of behaviors.
2. See /resources/config/engine/userDefinedBehaviorTree/conditionToBehaviorMap.json for the list of customizable BEI conditions and the sets of behaviors to which they may map. Edit the file as necessary to add/remove possible custom settings.
3. Build and compile the project normally.
4. Once the code is being simulated or run, the user-defined behavior tree will not be activated until the enableUserDefinedBehaviorTree flag is set to true. It defaults to false.
5. Enjoy being able to customize Vector!

## A Technical Description of the Feature
The user-defined behavior tree feature, as it stands as of 8/9/18, can be defined in three parts.

* The user-defined behavior tree component (/engine/aiComponent/behaviorComponent/userDefinedBehaviorTreeComponent/userDefinedBehaviorTreeComponent.h) is a BehaviorComponent that maintains the ground truth of the personalization. It keeps track of the conditions that can be given user-defined response behaviors, loads and saves the data between boots using variable snapshots, and provides an interface to add/modify them.
* The user-defined behavior tree router (/engine/aiComponent/behaviorComponent/behaviors/userDefinedBehaviorTree/behaviorUserDefinedBehaviorTreeRouter.h) uses the user-defined behavior component to delegate to the user-defined behavior, given a condition. It is activated upon any of the tracked BEI conditions being triggered. Once activated, it either plays the behavior or, if the user has not yet customized a response to the trigger, then it delegates to the selector behavior and, upon returning to control, plays the behavior that was selected.
* The user-defined behavior tree selector (/engine/aiComponent/behaviorComponent/behaviors/userDefinedBehaviorTree/behaviorUserDefinedBehaviorSelector.h) enables a user to select a reaction to a condition that becomes the new default behavior to respond to that trigger. It provides a voice interface that also uses user intents to do so. As of now, the selector behavior loops through all possible behaviors, demonstrating them one at a time and asking whether the user likes the reaction, until the user says yes to one of the behaviors.

## Considerations and Lessons From the Implementation
In the process of developing this feature, we .. met .. to get feedback on how to make this a feature user's would want to and enjoy interacting with. From these discussions, we made a few observations that may be useful for anyone that continues this work.

* Users enjoy acknowledgement when prompted for answers - Between the first and second iterations of this feature, we added in more conversation between Vector and the user. This conversation was meant to guide the user through what was happening. For example, rejecting a possible reaction in the selector behavior made vector say, "Ok. Let's try another one." These types of acknowledgements helped users understand the process more clearly and led to a more enjoyable experience. It may be worth focusing on how we can use conversational expectations to highlight subtler interactions/reactions going forward.
* At the moment, the selector is implemented as a catch-all, where the user will have full control to see and select from among all of the possible delegation behaviors. However, we came to see that it may be more natural for the user to slowly express a preference over some amount of time.
* One way this could be expressed in practice is that each time Vector hits a trigger (i.e. CliffDetected), it chooses an arbitrary behavior to delegate to. If the user does not like it, the user could say "bad robot" or some other negative phrase, which would lower the likelihood of the behavior that was just chosen. This is an example of negative reinforcement.
* Alternatively, we might consider positive reinforcement, where Vector displays two behaviors for each trigger and asks the user which one they like best. Once the user chooses, Vector can form a ranking of the behaviors that can be used to delegate in a more preferable way.
* If Vector ends up using a long-term, evolutionary method to personalize it's reactions, then we may also want a way for a user to see and select from all the options, as the current selector behaviors allows. One way to implement something like this would be to identify one behavior (like shaking the robot) as a special behavior that can trigger the selector if triggered within some amount of time after a reaction occurs during freeplay.

## Resources and Example Videos

Presentation - Hamzah (AI Intern 2018)'s final presentation slides - video should be in dropbox

Video 1 - selector behavior - no, no, loops to start, yes

Video 2 - selector behavior - no, yes