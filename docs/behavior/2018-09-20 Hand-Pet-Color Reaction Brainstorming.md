# 2018-09-20 Hand/Pet/Color Reaction Brainstorming

# Created by Andrew Stein Sep 20, 2018

## Goals

* Brainstorm possible reactions for seeing pets, hands, and bright colors

* Determine if there are enough feasible ways to differentiate reacting to these objects to justify investment by vision team to detect them (and CPU to run those detectors)

## Discussion items
Assuming we can even detect these new things to a reliable enough degree, we discussed these ideas for reactions:

* To hands:

    * Request a fist bump

    * Some kind of “nuzzling”

    * Purr / want to be petted 

    * Trigger pounce on motion behavior

    * Note: It was discussed that we might rather drive up to an object via current Exploring and _then_ ask the vision system whether it’s a hand. If so, do one of the above. If not, do standard scan-and-poke. This has two benefits: (1) less need to localize the hand in the image or deal with as much scale variation, and (2) reduce the variety of training data. Otherwise, we’ll need to detect hands in a wider variety of positions relative to the robot and drive over to them.

* To color:

    * Play an animation on the faces that temporarily changes their color to “match” the object’s color

    * Like / dislike certain colors, decided randomly for each Vector (the preference remains persistent over time)

    * Have the robot attracted to whatever eye color is currently set (adds some customizability and interactivity)

        * Need to see if we can differentiate the fixed set of eye colors reliably enough — especially in a variety of lighting scenarios

    * Just be attracted to brightly colored things in general, with no real color-specific action 

        * Simple, but need to see if this adds enough to be better than random

* To pets:

    * Start with Cozmo-like reactions (turn towards and bark/meow/sneeze)

    * Reactions that attempt to engage the pet to play (or trigger special play-with-pet behavior)

    * Can we limit scope to only being above or on same plane (not below), or vice versa?

The big limiting factor here is on the technical side: how well can we get any of these detectors to work in the limited time we have, particularly for hands and pets, which will require large amounts of data. So next steps are for the vision team to figure out an initial data and evaluation plan, build an preliminary network architecture, and get a baseline for feasibility.

## Action items

* Vision Team to meet and discuss how best to constrain goals and data capture for 1.1

* Vision Team to see if we can differentiate fixed set of eye colors in varying lighting (for attracted to current eye color idea)