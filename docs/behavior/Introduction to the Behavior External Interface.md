# Introduction to the Behavior External Interface

Created by Kevin Karol Oct 20, 2017

As discussed in [The Behavior Tree](Behavior%20Tree.md), one of the primary design goals for the Victor behavior system is to create behaviors which function as independent units. Within Cozmo's behavior system the full robot was passed into behaviors and distinctions had to be made about when the robot was const or non-const to try and have some control over what the behavior could and couldn't alter. Additionally, it allowed behaviors to reach into the robot to ask questions about behavior manager, freeplay or robot state which tied a behavior's operation into a very specific configuration of how freeplay operated. As a result, any changes to the structure of how freeplay operated could create silent failures throughout the entire behavior system.

![](images/Reaching%20Up%20into%20Behavior%20Manager.png)

Figure 1: Reaching up the tree into the Behavior Manager

## The Behavior External Interface: Misdirection and Components

To address these issues, passing the robot through to behaviors has been replaced by passing a new component into behaviors. This component is called the Behavior External Interface (BEI) and it serves as the only way that behaviors can interact with any part of either the Victor Behavior System or Victor itself. It provides a clear distinction between aspects of a behavior's operation which are internal to its decision making, and externally accessing information or updating state outside itself. Two key advantages to this approach are that it's a component based model and it provides a level of misdirection about what's happening behind the scenes.

The fact that the BEI passes information into the behavior as a set of components rather than giving the behavior access to the full robot means that it's possible to control finer grained control over how the behavior can interact with the system. For example, behaviors should never be updating block world or face world, so these two components can be passed in as const components. On the other hand the AIWhiteboard should always be accessible and mutable by behaviors. Additionally, some components may be optional (such as the nurture system), and under the component model these can be made explicitly optional so that the behavior will still operate as expected down the line if these components are removed from the victor experience.

![](images/BEI%20Misdirection.png)

Figure 2: The Behavior External Interface Hides Where Information Comes From

In terms of misdirection the BEI is wonderful because it hides where exactly the information the behavior is asking for is coming from and how processes like message subscription work. As a result it's been possible to make the behavior system completely synchronous using an async message gate at the Behavior Component level and then passing messages directly into behavior's update functions. Additionally, while most BEI function calls route directly into the Behavior System Manager, it's possible to re-direct these calls through stub implementations for tests or behavior planning.