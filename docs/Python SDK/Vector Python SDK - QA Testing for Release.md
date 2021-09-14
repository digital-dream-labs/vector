# Vector Python SDK - QA Testing for Release
Created by Michelle Sintov Last updated Apr 17, 2019

## Testing for new Robot-OS-only release
These steps should be run when a robot OS is being prepped for release, and no new SDK release will go out with this release.

Please following these steps on Mac and on Windows.

1. Install the publicly available SDK
    a. First, uninstall your current SDK.
        i. On Mac
           `python3 -m pip uninstall anki_vector`
        ii. On Windows
           `py -3 -m pip uninstall anki_vector`
    b. Next, install the publicly available SDK by following the steps here.
        i. How to install on Mac https://developer.anki.com/vector/docs/install-macos.html
        ii. How to install on Windows https://developer.anki.com/vector/docs/install-windows.html
    c. Please be sure to rerun anki_vector.configure for each new robot build you test, against a robot with prod endpoints. 
2. Get set up to run the Hello World tutorial by following the instructions here to download the examples compressed file and then run the script: https://developer.anki.com/vector/docs/getstarted.html
3. Run all tutorials in the folder on Windows. Run the indicated ones also on Mac.
    a. 01_hello_world.py (TEST ON MAC) - Vector should say "Hello World"
    b. 02_drive_square.py - Vector should drive in a square. It is required that he have enough space to drive around. It is currently expected that if he encounters a cliff or runs into an obstacle, the script will do something unexpected.
    c. 03_motors.py - Vector moves all tracks
    d. 04_animation.py - Vector plays an animation
    e. 05_play_behaviors.py - Position robot so he can see his charger. Robot should drive onto charger and then drive off.
    f. 06_face_image.py - Robot should show an image with a white background on his face. We are aware there is a bug where his eyes show over his face currently.
    g. 07_dock_with_cube.py - Position cube in front of robot. Robot should dock with the cube. This may fail ~30% of the time and that is expected.
    h. 08_drive_to_cliff_and_back_up.py - Position robot roughly 12 inches in front of a cliff and run. Robot should drive to cliff, react, then back up roughly 6 inches.
    i. 09_show_photo.py (TEST ON MAC) - Robot should show a picture he already has. If he doesn't have one, do "Hey Vector, Take a photo" first.
    j. 10_eye_color.py - Robot's eye should turn purple for a few seconds.
    k. 11_face_event_subscription.py - Show robot your face. Robot should say, "I see a face!"
    l. 12_wake_word_subscription.py - When prompted, say, "Hey Vector" and he should respond "Hello".
    m. 13_custom_objects.py
        i. Print this image showing 2 circles so that it roughly 3 inches square: https://developer.anki.com/vector/docs/generated/anki_vector.objects.html?highlight=markers#anki_vector.objects.CustomObjectMarkers.Circles2
        ii. When running the program, show your printout to the robot so that the symbol appears entirely within his camera view
        iii. In the viewer showing his nav map, you should see a reddish box floating in the location where you showed it to Vector
        iv. It is expected that you will have to hit Control C multiple times to close the script.
4. Run all the projects in the apps folder
    a. 3d_viewer.py (TEST ON MAC)- Two windows should open: camera view and nav map. It is expected that they might open under other windows.
        i. Please test that the remote control of the robot works as described in the text in the nav map window. 
        ii. It is expected that you will have to hit Control C multiple times to close the script.
    b. interactive_shell.py (TEST ON MAC) - Follow the instructions to run the animation using tab completion to enter the command and observe the robot played an animation.
    c. proximity_mapper.py - Robot should start turning around and drawing the obstacles he sees in the 3d viewer. UPDATE: Starting with R1.6, the obstacles will not be seen in the 3d viewer, as we need to release a new SDK for prox sensor data changes. However nothing else egregious should occur.
        i. It is expected that you will have to hit Control C multiple times to close the script.
    d. remote_control.py (TEST ON MAC)
        i. Test driving forward, back, left, right
        ii. Test moving head up and down
        iii. Test moving lift up and down
        iv. Test running each of the 10 animation
        v. Check that the camera viewer displays output
        vi. Test that the robot says "Hi I'm Vector" when you hit the spacebar on your keyboard

## Testing for new SDK Release
### Setup
Follow steps on this page to set up the SDK: Python Vector SDK - Getting Started

### Testing
1. Navigate to tools/sdk/vector-python-sdk-private/examples/tutorials and run every single program in the folder.
2. Navigate to tools/sdk/vector-python-sdk-private/examples/apps and run each of the following scripts that can be found in the subfolders: 3d_viewer.py,  interactive_shell.py, proximity_mapper.py, and remote_control.py.


NOTE: If there are any errors or if Vector is not doing what you think they should be, please create a JIRA bug for each issue