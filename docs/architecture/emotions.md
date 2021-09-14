# Emotion System

Victor, like Cozmo, has a range of emotions he can express. In many cases these emotions are expressed through
selecting appropriate animations. Events that happen in Victor's universe can also trigger *emotion events*.
Those events then modify victor's *mood*, which is controlled by the *Mood Manager*

## Mood Manager

Victor's mode is made up of a set of emotions defined in [emotionTypes.clad](../../clad/src/clad/types/emotionTypes.clad).
Note that many of these emotions exist but aren't really used for anything (e.g. "excited").

Each mood is phrased as a positive, e.g. `Happy`. The values range from -1.0 to 1.0, where negative values
reflect the opposite emotion.

* The opposite of `Happy` is `Sad`
* The opposite of `Confident` is `Frustrated`. This is updated primarily when Victor succeeds or fails at something
* The opposite of `Social` is `Lonely`. Seeing and interacting with people effects this emotion

Emotions shift over time. In general, they decay towards 0, the default state. Note that "default" for Cozmo
was happy-go-lucky, "default" doesn't mean "boring".

Mood values can effect any number of things in code, but primarily they are used for animation selection and
behavior selection. For example, the `DrivingAnimationHandler` chooses a set of driving animations based on
the current mood.

## Historical note

The emotion system was originally intended to be very broad and the main driver of behavior. Over time, it's
role decreased because we either didn't have enough variation to cover the states (e.g. what does a "bored" +
"happy" + "frustrated" greeting look like?), or because we wanted more direct control over behavior. For
example an explicit "search for faces" transition rather than having the robot automatically search for faces
when lonely. The explicit transition can then have a cooldown, be based on the status of known faces in the
world, or can be selected when a face is required (e.g. the user requested something via the app). The
resulting system is less modular and emergent, but easier to control and handle specific design requirements
and edge cases.

Old documentation from the emotion system is Cozmo can be found
[here](https://ankiinc.atlassian.net/wiki/spaces/COZMO/pages/48595096/Emotion+Behavior+Architecture). Note
that most of that page isn't accurate for Victor anymore, but it does have some useful background
