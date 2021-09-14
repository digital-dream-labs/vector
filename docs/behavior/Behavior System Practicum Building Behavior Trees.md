# Behavior System Practicum: Building Behavior Trees

Created by Kevin Karol May 17, 2018

## Overview

The backbone of Victor's behavior system is a data defined behavior tree. While some behaviors have hard coded relationships between their .cpp files, many decisions and relationships between behaviors are entirely defined by where the behavior lives within the behavior tree. Gaining a sense of how Victor's tree is built will unlock the ability to quickly update the robot's decision making process in a reliable and scalable way.

## Task

Replace Victor's behavior tree with one of your own creation. This new behavior tree should:

* Each time Victor sees a cube he should respond to it in a different way (Roll, pickup, etc)
* Victor should occasionally try to find a face/person to interact with
* Victor should respond when he hears the "Hey Vector" trigger word
* Victor should respond to some stimuli (shaking, being placed on charger, being picked up)
* Victor should decide to do something different when touched
* Victor should decide to do something different when the prox sensor detects an object
* Victor should decide to do something when he detects a cliff

It should be possible to complete this task by exclusively altering files within the resources folder and running build/generation scripts.

Challenge: Try to make Victor's responses as unexpected as possible rather than building a tree that is similar to the shipping tree. What makes the robot's responses intelligible to the end user? What makes the robot feel broken?

## Tips
* What file specifies the root behavior for the tree?
* What behavior classes make it easy to data define relationships between behaviors?
* What are BEIConditions?

## Resources
* [How Data Drives Behaviors](How%20Data%20Drives%20Behaviors.md)
* Check out the scripts in tools/ai/behavior/