## ProceduralFace

A `ProceduralFace` is simply the container for all the parameters it takes to renders the robot's eyes.   Most of the parameters control the shape and position of each eye independently, including the height, width, corner curvatures, and upper and lower lid shape/position. There are also "whole face" parameters for rotating, scaling, and translating, the entire face or adjusting the contrast of the stylized "scanlines". 

The actual rendering is done by a separate class, the `ProceduralFaceDrawer`. At a high level, each eye's shape is drawn as a filled polygon whose points are rotated, shifted, and scaled into place. Then the eye lids are drawn on top to black out their shapes. The face is drawn in HSV (or HSL) space because the hue of the face is not animated but rather controlled as a global setting configurable by the user.

**TO DO:** Add a diagram explaining the parameters visually.

**TO DO:** Consider renaming to "ParameterizedFace" or something similar. "Procedural" implies the face is generated at runtime, based on our usage of the word elsewhere. In fact, a ProceduralFace is just a rig with a bunch of parameters and is also stored in "canned" animations.

