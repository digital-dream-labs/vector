## BlockWorld

### Quick Summary

* Robot component for storing [Observable Objects](observableObjects.md)
* ["Located" and "Connected"](#locatedVsConnected) objects accessed separately
* Active objects assumed unique and matched by _type_, Passive objects matched by [_pose_](poses.md)
* Query for objects by origin, type, ID, etc, using a [BlockWorldFilter](#blockWorldFilter)
* Handles "[rejiggering](#rejiggering)" of origins when same object observed in different coordinate frames

---

### Details

[`BlockWorld`](/engine/blockWorld/blockWorld.h) is the component of the Robot for storing [Observable Objects](observableObjects.md), objects with markers on them. This includes the light cube(s), charger, and "custom" defined objects which use markers from the SDK. Objects can be returned by various queries on absolute location, location relative to other objects, and other properties. BlockWorld is also responsible for "rejiggering" its objects' poses on relocalization.

See also: [Poses](poses.md), [Observable Objects](observableObjects.md)

### History
This is one of the oldest classes in the project. It may appear overly complex given the small number of objects with which Cozmo and Victor interact. Originally, Cozmo was intended to be a construction robot capable of keeping track of _many_ different blocks of different types. Thus the somewhat inappropriate name and potentially overkill storage/searching mechanisms. 

<a name="locatedVsConnected"></a>
### Located vs. Connected objects

The storage and methods in BlockWorld are generally divided into two types: "Located Objects" and "Connected Objects".

**Located Objects** are those objects for which we have an estimated pose, e.g. by virtue of having seen them. 

**Connected Objects** are objects to which we have a radio connection (i.e. the robot has "heard" from them, even if it hasn't seen them with the camera). 

The containers for each are maintained separately because we can hear from objects before seeing them. Connected objects' lights can be controlled and their moving state queried (known by virtue of their accelerometers) even without knowing where they are. 

### Adding/Updating Objects

When an object from a library is observed, first BlockWorld checks to see if it already exists in the list of located objects. Then:

* For unique objects (only one of a given type may exist in the world at the same time), either the first instance of an object of the observed type is added, or the pose of the existing object with matching type is updated. If the object exists in a previous origin, it is pulled into the current origin.
* For nonunique objects, BlockWorld looks for an object with matching type and in roughly the same pose of any existing located object. If _no match_ is found, it is added as a new object. If a match _is_ found, the existing object is updated; i.e. its pose and last observed time are set to the latest observation's.

### "Deleting" Objects

Objects are deleted from the located objects in the following situations:

* The robot drives through the last known position of an object (indicating it must not be there anymore)
* A VisionMarker is seen behind the last known position of the object (which isn't possible if the object were there to occlude the VisionMarker)
* A [Dirty Object](poses.md) is not observed where expected. 

Note that we do not take a lack of observation of objects with Known Pose State as enough evidence to remove the object. I.e., absence of evidence is not considered evidence of absence (unless the object was already Dirty -- e.g. moved -- and thus we have _additional_ indication it is probably no there).

Also note that Active Objects may be removed from the located list, but the Robot may still have a radio connection to them, so they can still exist in the Connected Objects list. Again, this is why any usage of radio-based functionality (lights and motion) should use the Connected interface to BlockWorld.

<a name="blockWorldFilter"></a>
### Object Organization and BlockWorldFilters

Objects in BlockWorld are stored in internal containers. Most methods for accessing objects allow you to filter queries based on these properties using a [`BlockWorldFilter`](/engine/blockWorld/blockWorldFilter.h). Again owing to the original design for Cozmo, the degree of taxonomy here is probably overkill for Victor, but it also works. (Until it's an actual performance issue, it's probably not worth changing.)

BlockWorld technically supports any number of **passive objects** of the same type, so each observed instance gets a runtime-assigned unique `ObjectID`. BlockWorld attempts to match and merge passive, non-unique objects by their pose. Due to robot localization errors and drift, this is inevitably an imperfect process, so one cannot rely completely on ID. 

For **unique objects** like LightCubes and Chargers, however, we assume/require that there be only one of each type. This assumption makes it possible to associate a visually-observed object with the corresponding type being "heard" over the radio. Other methods for relaxing this assumption have been discussed but are exceedingly complex and not really necessary given the product's direction. Therefore, we can also guarantee a one-to-one mapping between ObjectType and ObjectID for active objects (e.g. LightCubes).

BlockWorldFilters allow you to specify either a set of attributes to "allow" or "ignore" during a query. If not specified, all are allowed. You cannot specify both allowed and ignored sets for the same attribute at the same time. In addition to filtering on the properties described above, you can also use custom filter functions to do special-case filtering. Some commonly-used filters functions are provided as static methods as well: PoseStateKnownFilter, ActiveObjectsFilter, and UniqueObjectsFilter.

<a name="rejiggering"></a>
### Origins, Delocalization, and Rejiggering

The [poses](poses.md) of the robot and all objects known to it exist relative to some arbitrary 3D origin, (0,0,0). Each time the robot is **delocalized** (e.g. by being picked up), it no longer knows where it is with respect to that origin and must start a new coordinate frame with its own origin. (Note that "frame" and "origin" are often used somewhat interchangeably.) The Robot's [`PoseOriginList`](/coretech/common/engine/math/poseOriginList.h) keeps track of these various origins.

In BlockWorld, objects are stored per-origin since poses of objects in different coordinate frames are not comparable, by definition. When the robot re-observes an object from a prior coordinate frame (one differing from the robot's _current_ coordinate frame), and that object can be used for localization (which currently includes just the Charger), we perform a "**rejigger**". This process effectively merges the information from the current coordinate frame with the prior one containing the object, using the pose of the object they have in common as the reference to update all poses with respect to a single origin.

Origins with no localizable objects remaining in them are referred to as "zombie" origins. We will never be able to rejigger those origins, but poses created elsewhere in the system may still use them as parents, so they are maintained to keep pose trees valid. Now that because Victor is intended to be "always on", it is an open question whether this is scalable or if we should periodically clear the `PoseOriginList` somehow.

### Differences with Cozmo

Due to the always-on nature of Vector and the limited battery life of the cube, Vector is rarely connected to his cube (unlike Cozmo, which is always connected). This makes the cube less useful for localization, since we rarely get indications that it has been moved. 

Therefore Vector does not use the cube for localization, and instead uses only the charger. The charger has a bigger marker, so its pose is generally more accurate, and it is usually stationary. This makes it more suitable for localization than the cube. However, since the charger does not have an accelerometer or radio, we generally do not know if it has been moved, but we make the assumption that it usually remains in the same place.
