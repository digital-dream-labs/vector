# Cozmo Pet Detection

Created by David Mudie Nov 16, 2016

## Animations

The ReactToPet behavior chooses animations from the following groups:

* PetDetectionDog (aka ag_petdetection_dog.json)

* PetDetectionCat (aka ag_petdetection_cat.json)

* PetDetectionSneeze (aka ag_petdetection_misc.json)

Normally, Cozmo will play an animation appropriate to the pet type, but there is a random chance that he will respond with a sneeze.

Animations within each group are weighted so that "small" reactions will play more often than big ones. This is done so that false positive reactions are less likely to draw attention and so that "big" reactions don't become annoyingly repetitive.

## Vision System
Cozmo's vision system supports basic pet detection.  The vision system attempts to identify cat or dog faces in each image.  The vision system does NOT identify cat or dog bodies. It works best when the pet face is straight-on and straight-up to the camera.

Cozmo's vision system does NOT support pet recognition.  Cozmo can't tell pets apart, and he can't tell if a pet face was observed in a previous session. Pets are assigned a new ID each time they are detected. Pet IDs are not saved between sessions.

Cozmo's vision system isn't perfect.  It may report "false positives" such as an odd shadow or a human face that registers as a pet.

## Engine Behaviors
Cozmo has a reaction behavior called "ReactToPet" that responds to pet detection.  When pets are detected by the vision system, Cozmo performs the following sequence:

1. Turn toward pet
2. Play reaction animation
3. Track pet for short period

After reacting, the behavior has a cooldown period (1 minute) before it will react again.

