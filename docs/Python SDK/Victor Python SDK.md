# Victor Python SDK
Created by Shawn Blakesley Last updated Jan 25, 2019

## High Level Architecture
With the engine code running on robot, using the SDK significantly reduces in complexity. Once the user has access to the ip of the robot (exposed via the app or on the robot's face), a logged in user may connect to their robot (see Victor SDK Connection Authentication for more details).

A significant portion of the sdk code already exists for Cozmo, and can be reused where applicable. Below is a rough high-level interface for Vector's Robot class:

![](Python%20SDK.png)

The Robot class hides the complexities of async python inside the Action class. However, there is an AsyncRobot class which extends the Robot class to allow more asynchronous functionality (similar to how the Cozmo SDK worked with wait_for_completed on actions).

## Code Style Guide
The Code Style Guide may be found in the github repo's CodeStyle.md.

## Python Code Structure
Currently, the code structure is the same as the Cozmo SDK. However, we plan to update this shortly to achieve a few goals on top of the Cozmo design:

1. Divide the responsibility of the system more clearly.
    * robot is responsible for a lot of things in the Cozmo SDK (getting camera images, playing animations, driving motors, etc.). To improve upon this, we need to take some time to design the important subsystems of robot, and make those lines a little more visible.
2. Update the tutorial, examples and apps to be more in line with Victor's brand vs. Cozmo's.
3. As mentioned on the user forums, we should make an effort to re-tool / flush out the documentation

## Relevant Github Repos
All the code lives in a submodule of victor under tools/sdk/vector-python-sdk-private. The repo for the submodule live in vector-python-sdk-private.

## External Contributors
In the old world of the Cozmo Python SDK we had a public repo that required users to sign our CLA (and possibly our Corporate CLA). However, this may be changing, and this doc will need to be updated as we figure out the new system.

## Usage with Webots
We need to figure these steps out and make sure using webots with the SDK is added as an integration test.

## SDK Release Process
TBD when we go to release this to the public, but this will probably be similar to the way we deployed the Cozmo SDK.

## Test Steps
When a change is made to the python sdk, the following steps will need to be taken to run through the tests.

The testing process is described in more detail in Victor SDK Testing Process

## Design Considerations for Automation
* Multiple robots from 1 script
* Detailed robot status message (enables checking if playing animation, are motors turning, is recording on, is robot trying to make a sound)
* Pause all motors / actions / behaviors (including stopping animations, sounds, and recording mics) with a simple interface for lifetime testing of motors and sensors
* Support for async callback for robot receiving message
* Results from cloud intents
* Build details
