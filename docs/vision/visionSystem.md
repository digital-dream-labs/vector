## Vision Component / System

### Quick Summary

* The `VisionSystem` is computationally expensive and runs asynchronously in a thread. The Robot's `VisionComponent` is the interface to it from the main thread.
* There are several sub-components for detecting faces, markers, etc, each with its own `VisionMode`
* Modes can be scheduled by pushing/popping a `VisionModeSchedule`
* A simple `ImageCache` is used to avoid repeated resizing and color/gray conversions amongst the various modes


---

### Details

The Robot's vision system has two parts: 

 1. The `VisionComponent` which is a component of the Robot which provides an interface to vision capabilities to the rest of the system ([behaviors](behaviors.md), [actions](actions.md), etc.).
 2. The `VisionSystem` which is a member of the `VisionComponent` and runs on its own thread. This is because vision processing is substantially slower than anything else (and depends heavily on which vision modes are enabled) and thus we cannot have the processing of an image block the rest of an engine update tick.

Note that it is _possible_ to put the vision system in "synchronous" mode, making it run on the main thread. This is mainly useful for unit/webots tests. 

### Vision Modes

The `VisionSystem` has several modes that can be enabled, and corresponding sub-components which do different kinds of visual processing:

* Marker Detection
* Face Detection & Recognition 
* Pet Detection
* Motion Detection
* Laser Point Detection
* Overhead Edge Detection (for finding "interesting" edges on the ground plane)
* Camera Calibration (used by the Playpen Factory Test)
* Benchmarking
* Overhead Mapping / Driving Surface Classification
* [Neural Nets](neuralNets.md) (object detection, image classification, etc, via Convolutional Neural Nets)
* Illumination Detection

Note that both Face Recognition and anything using neural nets are themselves quite computationally intensive and slow, so they also have their own sub-threads in order to run asynchronously.

The results of all of the above are put into a `VisionProcessingResult` which is a simple container of all the things we could detect in an image. That container is put in a "mailbox" to be picked up by the `VisionComponent` on the main thread and handled appropriately. For example, the `VisionComponent` is responsible for passing any detected `VisionMarkers` on to [BlockWorld](blockWorld.md) for adding/update objects.

The `VisionSystem` also handles keeping camera exposure adjusted and provides a basic measure of image quality (for reporting "too dark" or "too bright").

Note that you can wait for a number of images which had a specific processing mode enabled using a `WaitForImagesAction`.


### Vision Schedules

It is genereally undesirable and/or impossible to run all the vision modes on every frame. A `VisionModeSchedule` provides a mechanism for enabling a certain schedule of modes to be run. For example, perhaps in a given behavior we want to run face detection in every frame, marker detection every other frame, and motion detection every 10th frame. The behavior can "push" a new `VisionModeSchedule` using the `VisionComponent` and then "pop" it when done, returning to the previous schedule in the stack.

### Image Cache

A simple `ImageCache` is used to help avoid sub-components of the `VisionSystem` redoing computation such as color/gray conversion or resizing, without them all needing to know about each other (or the order in which they are called). 

It is a simple compute-on-demand approach. When any system requests a half-size grayscale image from the cache for the first time, for example, that image is computed and stored. Any subsequent request will use that cached version. 

All cached data is cleared when the cache is updated with a new image.

