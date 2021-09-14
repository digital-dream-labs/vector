## FaceWorld / PetWorld

### Quick Summary

* Components of the Robot like [BlockWorld](blockWorld.md), but for storing human (and pet) faces
* Human faces' poses are also tracked, while pets' are not

---

### FaceWorld

`FaceWorld` is a component of the Robot for storing faces and their last known location. It is effectively a simpler version of [BlockWorld](blockWorld.md) for storing faces and their 3D poses (and "rejiggering" their poses like BlockWorld does for objects).

It is also is the mechanism for interacting with the face recognizier in the [Vision System](visionSystem.md), e.g. for Face Enrollment (aka Meet Cozmo).

**Face IDs**

The face recognition system runs slower than the face tracking system. So before a face is recognized, it receives an ID for the purposes of tracking only. These IDs are negative, by convention. 

There are two kinds of faces known to the recognition system: 

* "named" faces which were added as a result of Meet Victor
* "session only" faces which were added opportunistically during a given play session to keep up with faces seen before, but that do not have an associated name

Once a tracked face is matched by the recognition system to either a "named" or "session-only" face, it receives that face's ID, which is positive by convention. These are generally the IDs that are used throughout the rest of the system. 

### PetWorld

`PetWorld` is the analogous container for pet faces. A key difference for pet faces is that we do not know their 3D pose, only their heading. This is because we do not have any known-size landmark we can identify, unlike human faces where we can use the distance between eyes, which is a relatively consistent dimension, to estimate distance to the face. Since no poses are stored, no rejiggering is required.

