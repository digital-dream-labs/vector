# Best Practices: "Behavior Flow"

Created by Kevin Karol Jan 12, 2018

Since behaviors are essentially a fancy way to tie together higher level concepts of how the robot makes decisions and expresses these decisions to the user ("I have decided to pick up this cube and I will indicate that by driving towards it at the same time as changing the color of the lights on the cube - I have seen a different cube and changed my mind and will indicate this by playing an animation and then driving in the opposite direction") after they fulfill their interface obligations their internals can be structured with near infinite variety. However, as the building blocks of Victor's personality behaviors tend to be a bit "volatile" and must be regularly updated to meet new design needs. A typical behavior may be built up by multiple engineers and designers across multiple teams and undergo rapid iteration for a period of several months. As a result, it's helpful for behaviors to follow a decision making process that makes it clear 1) what the current flow of the behavior looks like and 2) clear locations to alter this flow which result in as few silent side effects as possible. 

This second requirement is an art, not a science, but this document outlines a couple of tried and true behavior structures that can help minimize silent failures across behavior iterations.

## Structure 1: Golden Path With Interrupts

![](images/Breaking%20Conditions.png)

Figure 1: Golden Path with Interrupts

iCozmoBehavior offers the DelegateIfInControl helper function which accepts a member function as a callback. As a result many behaviors can be constructed by stringing together a series of functions that pick the appropriate action/lights to start, delegate control and then specify the function that will make the next decision along the path. This style of stringing together callbacks tends to work better for shorter behaviors and/or behaviors with a limited number of branch/loop points.

Since functions designate the path internally there is no inherent need for decision making within the BehaviorUpdate function. This function can either go unimplemented, or can be used as a great central location for  all "breaking" conditions along the golden path.

Take the theoretical behavior defined in Figure 1 which could define the process of picking up a cube. For the purposes of this example let the following definitions apply:

* Thing 2 is Play the "About to Pickup" animation
* Thing 4 is the pickup action
* Thing 5 evaluates the pickup result and branches to Thing 7 in the case of a successful pickup and Thing 6 in the case of a failure
* Thing 6 is playing the "re-evaluating the situation" animation
* Interrupt 1 is checking for a cube that is closer than the cube Victor is currently attempting to pickup

In this case within the BehaviorUpdate function there would be a block of code that checks block world every tick for a new, closer block. It would be clearly specified that Interrupt only applies in the range of Thing 2 - Thing 5 and therefore wouldn't interrupt the behavior after the block is successfully picked up. This structure means that if an interrupt is breaking the golden path unexpectedly, the engineer assigned to the task can easily check a single function to see what condition is being incorrectly evaluated according to design specifications. It also means that if a new Interrupt is desired, (say the user can now place a block directly into Victor's lift to jump straight to the success animation) there's a clear spot to introduce it along with all applicable conditions and where it should jump the behavior to along the golden path.

## Structure 2: State Machine

![](images/Flow%202_%20Central%20State%20Machine.png)

Figure 2: State Machine

Behaviors which may have a larger variety of paths/loops/transitions may be better served by implementing a full state machine. Under this structure each "Transition" function bundles together all functionality corresponding to a given behavior stage (lights, actions, etc), and then each DelegateIfInControl should route the behavior back to a central decision making function which can re-evaluate the world/behavior state and decied which Transition function to call next.

The key to this structure is that it ensures all decision making logic remains within a confined area of the behavior. If there are direct connections between e.g. Thing 3 and Thing 4 or branching paths and loops that aren't specified within the central state machine it becomes challenging to change part of the decision making logic at a request from design without accidentally triggering unintended/silent side effects.