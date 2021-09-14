# DevImageCapture

Created by Andrew Stein Last updated Dec 14, 2018

__The most useful tool ever created.__

DevImageCapture is a behavior which uses MirrorMode to show the Vector's camera feed (and, optionally, anything the vision system detects) on his face. We use this behavior to capture data for testing and training vision systems. For example, this is the tool we use for employee take-home data capture.

Pressing the backpack button saves an image. Pressing and holding the backpack button for a second enables "streaming" mode which saves every image. (Press again to disable.)

Raising and lowering the lift cycles through a set of class names defined in the behavior's Json configuration file and saves subsequent images in a correspondingly-named subdirectory. The current class name is indicated at the top left of the screen with a counter indicating how many images are currently stored for that class.

The behavior can also be used for "hard example mining" for training neural nets by using special class names "FalsePositives" and "FalseNegatives". See also the "Saving Images" section here. 

The saved files are located <cachePath>`/vision/camera/images` and can be retrieved (or erased) using the `project/victor/scripts/get-dev-images.sh` (or wipe-dev-images.sh) scripts.

Other options include the ability to capture multiple images with each button press (with head/body movement between captures), enabling/disabling a camera shutter noise, enabling VisionModes, etc. See also documentation in the Json configuration file for more. Specific versions of the behavior with predefined settings can be created by saving new Json files with specific configurations for a specific task. Because this is just like any other behavior, it can be accessed using the WebViz Behaviors tab.

