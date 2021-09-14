# Behavior Component Overview

Created by Kevin Karol

## Introduction

The documentation on the victor behavior component page provides a detailed look at one of Victor's major AI systems. Fundamentally, victor could operate without the behavior component. To have the robot do something all that is required is that a user (programmer, ai system or end user) send the robot an action to perform. However, defining complex things for the robot to do as a series of actions (not to mention communicating with other systems like cube lights and audio) is an extremely arduous and fragile process. The behavior component is simply a layer of abstraction that allows a high level bundling of actions together. As a result it's possible for the user to ask the behavior component to "Stack two blocks" and let the behavior component worry about finding cubes, picking up cubes, handling failures etc.

![](images/Behavior%20Component%20Role.png)

Figure 1: Behavior Component Abstraction Layer

This document will lay out where exactly the behavior component lives within engine's code base. The documents which follow will provide an overview of the guiding philosophy and fundamental systems that allow the behavior component to support high level behavior requests from the user and give victor the appearance of "deciding" what behaviors to run next.


## High Level Component Nesting

![](images/Data%20Divide.png)

Figure 2: Layers between the Robot and the Behavior Stack

The robot class has no knowledge of the behavior component. It creates an AIComponent and provides the AIComponent regular update ticks. In turn, the AIComponent creates a behavior component but has no knowledge of the internal workings of the behavior component. It's theoretically possible to have an AIComponent which never creates a BehaviorComponent and instead just does other high level information processing and decision making and then passes this information along to a different robot process, but in practice there's almost always a desire to have the BehaviorComponent drive victor's actions.

Within the behavior component the main system responsible for handling victor's behaviors is the BehaviorSystemManager. As the name implies the behavior system manager pretty much runs the show with regards to behaviors. In that case why have a distinction between the behavior component and the behavior system manager? While this documentation won't cover other systems within the behavior component in great detail, there are processes related to behaviors which don't have to do with managing behaviors. For example, the behavior component could have a system which prints out or visualizes the full behavior tree or stack within the behavior system manager. Because this printing process isn't directly related to behavior system managment and can rely on the behavior system manager and the iBehavior interface it's better to maintain it within the behavior component rather than tying it into the behavior system manager directly.

The behavior system manager is the last tangible level of the behavior component. The rest of the behavior system is entirely defined by a data driven set of behaviors laid out in a tree. As a later section will cover the behavior system manager has two primary responsibilities. 1) It receives commands about how to update the behavior stack (delegate to a new behavior, cancel a behavior on the stack) and 2) It calls all appropriate lifecycle functions on the behaviors in the stack as they are activated/deactivated.

## Summary

The behavior component is a layer of abstraction around actions that allows high level concepts about what victor should do into "behaviors". The robot has no knowledge of these "behaviors". Instead it is the AIComponent that creates a behavior component which in turn allows the behavior system manager to maintain the behavior stack, a data defined tree of behaviors that determine what cozmo does.


The next section will cover how the behavior compenent relies on the data defiend behavior tree to create victor's "behavior".