# Bright Color Detection

Created by Andrew Stein Last updated Dec 07, 2018

* This feature is on hold until a decision is made regarding the end use (how a user will experience it)
* Goals:
    * Make Vector appear interested in objects that are bright colors
    * Make it clear to a user that Vector is interested because the object is brightly colored
    * Avoid repeatedly reacting to the same object
* Ideas for reactions:
    * Play an animation on the face that temporarily changes eye color to “match” the object’s color
    * Like / dislike certain colors, decided randomly for each Vector (the preference remains persistent over time)
    * Have the robot attracted to whatever eye color is currently set
        * Adds some customizability and interactivity
        * Need to see if we can differentiate the fixed set of eye colors reliably enough — especially in a variety of lighting scenarios
    * Just be attracted to brightly colored things in general, with no real color-specific action
        * Simple, but need to see if this adds enough to be better than random (see goals above)

* Technical approach:
    * Start with C++ port of Hanns' bright color Python algorithm?
        * Implements this paper from EPFL
        * "They had a group of people score tons of images by how colorful they thought they were on a scale from 1..7. Then they tried to derive an algorithm which scores the same. The one implemented above overlaps 95% with what their tests showed people consider colorful. SO for example even if you look at one tile and it's fully green, that still gets a lower score than if the same tile has both green and red etc. in it. So a box of crayons would score really high."
    * [Initial Python Implementation from Hanns](colorfulscore.py)

