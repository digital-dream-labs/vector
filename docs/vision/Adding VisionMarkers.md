# Adding VisionMarkers

Created by Andrew Stein Last updated Apr 03, 2019

For historical reasons, Cozmo and Vector's VisionMarkers are compiled directly into the executables from generated header files. (This is because long, long ago, the marker detector actually ran on Cozmo, using the ARM M4 processor.) Until we switch to a world where we simply store the markers as "resources" and ingest them on startup (adding variations as needed), this page describes the steps needed to add new VisionMarkers to either platform. This is not the cleanest, prettiest process (and still involves Matlab), but given how rarely it occurs, it has not yet been a high priority to simplify or beautify it.

## Basic steps:
Despite the number of "Detailed Steps" below, this should not take too long or be terribly complicated. The verbosity in the following section is an attempt to be clear (and to address minor differences between Vector and Cozmo), as opposed to being an indicator of complexity. At a high level, the basic flow is:

* Get images and put them in the right place (in Dropbox)
* Create rotated versions of the images with a Matlab script
* Tell the VisionMarkersTrained class in Matlab about the new markers
* Run another script to create various "versions" of each marker (to better populate the nearest neighbor library), and then actually generate the new headers with them

## Detailed steps:

1. You'll need access to the existing markers in "Vision Markers - Final" on Dropbox. The entire marker library is created each time you follow these steps, so you'll need the existing markers in order to add new ones. You cannot (yet) just add to an existing library.
2. Open Matlab and initialize the path by running initCozmoPath (or initVectorPath) from the root directory of the Cozmo (or Vector) repository. (NOTE: If you get an error about getdirnames not being found, you should be able to ignore that for the purposes of this process.)
3. Make a note of the root image location referenced by the VisionMarkerTrained class, which scripts used later rely on:
    a. For Cozmo, just type VisionMarkerTrained.RootImageDir at the prompt and note the location. (E.g. ~/DropBox/VisionMarkers - Final/MassProduction/trainingImages)
    b. For Vector, the root path is concatenated from two variables in VisionMarkerTrained: fullfile(VisionMarkerTrained.RootImageDir, VisionMarkerTrained.VictorImageDir) (You may see warnings here.)
    c. If you need to change these paths, you can simply do: edit VisionMarkerTrained.m (see also below). But for basic usage, you can just use the paths already stored there.
4. Get the new VisionMarker(s) images, complete with fiducial, from Art/Design.
    a. These should generally be in PNG format, with at least 256x256 resolution.
    b. It's also fine (and even helpful) if an alpha channel is used to make the background transparent. Note that "background" is the white part for Cozmo and the black part for Vector.
    c. Put the new markers into a new or existing subdirectory of the path obtained above, as appropriate.
5. Create rotated versions of each fiducial, rotated by 0º, 90º, 180º, and 270º.
    a. The MarkerDetector's nearest neighbor library treats the four versions as four separate entries.
    b. The createRotatedVisionMarkers Matlab script will do this for you:
        1. For Cozmo, edit the rootPath at the top of the script to point just to the new marker(s)' subdirectory (so you'll need to repeat this rotation step for each subdirectory manually)
        2. For Vector, edit the subDirs list to contain only the directories with the new markers (it will assume those subdirectories are under the "root image location" found above)
        3. Run the script
        4. Verify that there is a rotated subdirectory for each marker subdirectory, containing the rotated versions of the new images (and any previously existing ones, if applicable)
        5. NOTE: If you remove an image file, you will need to manually remove its rotated versions as this script does not do it for you!
6. If you created new rotated subdirectories above, "register" them with the VisionMarkersTrained class:  (If you simply added a marker to an existing sub-directory, you can skip this step.)
    a. Open the VisionMarkerTrained class definition file: edit VisionMarkerTrained. (Again, you may see some warnings in Vector.)
    b. Add the paths to the TrainingImageDir list.
7. Run trainNewMarkersScript. This script is a simple convenience wrapper which extracts the "probe values" (i.e. sampled nearest neighbor examples with appropriate variations), generates the header files, and creates a test image (for unit tests) in a new figure window. You can decide whether or not you want to update the unit test image by clicking the button to save it, but this is not a requirement.
    a. You should see that MarkerCodeDefinitions.h and nearestNeighborLibraryData.h have updated
    b. Test with these changes and then commit the new headers.
8. Though not required for the MarkerDetector, you should probably also add the new textures to Webots. 
    a. Put the images under simulator/protos/textures/
    b. You only need the originals, not the rotated versions
    c. Note that the size must be a power of two (so 256x256 or 512x512 are probably good choices)
    d. This is where having the alpha channel can be helpful

# Future improvements:
1. Convert the Matlab script into Python (probably using OpenCV), keeping only the functionality we actually use. (The Matlab script is quite general and can create far more variation for "training" than we now use.)
2. As mentioned above, consider having the C++ MarkerDetector simply ingest png images from resources and do the augmentations programmatically as it inserts them into the Nearest Neighbor Library. Then this whole crazy process can be replaced with something like: "put new marker images into  resources/engine/vision/markers".