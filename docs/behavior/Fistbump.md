# Fistbump

Created by Joseph Lee Jan 10, 2017


# Fist Bump (Upgrade)

In this upgrade Cozmo raises his lift toward the player as if he is asking for a fist bump / high five, and responds accordingly if he gets one (as well as if he doesn’t).

Requirements:

* Zero Power Cubes
* One player



Behavior steps:
1. Cozmo excitedly looks for the nearest face (last known? If no face found he should eventually just proceed to step 2)
2. He raises his lift toward the person and waits for an impact.
3. If no impact is detected he moves forward slightly and wiggles his lift impatiently.
4. Eventually it times out if no impact - Cozmo is sad because you left him hanging.
5. If impact is detected, Cozmo celebrates!

## Initial Test Prototype:

This simple prototype needs to prove that we can reliably detect a fist-bump style impact with Cozmo’s lift (both a very gentle impact, and a more forceful one, to cover a range of play styles).

In the simple prototype Cozmo needs to do the following:

1. Raise lift
2. Wait X seconds for any impact with his lift
3. Succeeds if impact detected (PH reaction)
4. Fails if time runs out (PH reaction)


