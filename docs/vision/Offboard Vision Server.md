# Offboard Vision Server

Created by Robert Cosgriff Last updated Apr 16, 2019

"Offboard" here means "not on robot". I.e., this document explains how to run some kind of vision processing on a laptop or other machine for prototyping vision algorithms or neural nets not yet ready or performant enough to run directly on the robot's hardware.

## Prerequisites

* `Install python3`
* `pip install grpcio`
* `pip install protobuf`
* `pip install numpy`
* Install `opencv` with python bindings.

## Setup / Test
Your vision algorithm will be executed by the AnalyzeImage function in coretech/vision/tools/offboard_vision_server/offboard_vision_server.py. First ensure that it can talk to the robot as follows:

* Edit the offboard_vision field in resources/development/config/server_config.json to be the IP address of the machine on which you'll be running offboard_vision_server.py, an example of server_config.json with where you should place your ip is below.

```
{
  "jdocs": "<JDOCS Address>",
  "tms": "<Token Server Address>",
  "chipper": "<Chipper Address>",
  "check": "<Connection Check Address>",
  "logfiles": "<Logging Address>",
  "appkey": "<Environment Appkey>",
  "offboard_vision": "<offboard_vision_ip>:16643"
}
```

* Turn on the OffboardVision Vision Mode on the robot, most easily done via its console variable (see below)
* Turn on the MirrorMode Vision Mode on the robot to see a live feed of the robot's camera
* Start the server with: python offboard_vision_server.py
* You should see the server print the dimensions of the images it's receiving
* You should see two example boxes for a "cat" and "dog" detection drawn side by side on the robot's face

## Running your own Vision Algorithm
Now that you've proven the robot and your server are talking to each other, you're ready to implement your own vision algorithm by modifying  AnalyzeImage in offboard_vision_server.py. It should consume images arriving from the robot in Protobuf messages, and it should produce Json results to send back to the robot over the wire. Those Json results are interpreted into CLAD-defined SalientPoint structs by the NeuralNets::ParseSalientPointsFromJson() function running on the robot. Use the Json "procType" field to specify how you want your Json interpreted on the robot. You can of course implement your own, but the built-in processing types are "SceneDescription", "ObjectDetection" (as shown in the example code), "FaceRecognition", and "OCR". Anything else will result in the Json result being interpreted as an array of SalientPoints in Json format.

## Console Vars
Here is some documentation on how to edit Console Variables. You can find you're Vector's IP by putting him on the charger hitting the backpack button twice and raising and lowering the lift. All the relevant console vars are under the Vision tab, under the VisionModes section.

## Optional: Editing the ImageRequest and ImageResponse objects
If you need to edit the ImageRequest and ImageResponse objects, which should be considered a last resort option, you should follow these steps.

* `pip install grpcio-tools`
* Make the necessary changes to tools/protobuf/vision/private/vision.proto 
* Now to create the new proto/grpc files that we'll use in the offboard_vision_server
    * `cd tools/protobuf/vision/private`
    * `python -m grpc_tools.protoc -I. --python_out=../../../../coretech/vision/tools/offboard_vision_server/ --grpc_python_out=../../../../coretech/vision/tools/offboard_vision_server/ vision.proto`
* Now we'll need to propagate those changes to vic-cloud via the usual build victor_build_release -f which will generate the necessary proto/grpc files for vic-cloud