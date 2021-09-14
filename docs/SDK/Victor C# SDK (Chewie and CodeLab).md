# Victor C# SDK (Chewie and CodeLab)

Created by Shawn Blakesley Last updated May 22, 2018

* SDK Interface
* Messaging Network Service

The design of the messaging layer can be found at Victor<->App Wireless Messaging.  For MVP, the API will provide interaction with high-level behaviors for the Victor Companion App to be able to interface with Victor.  Eventually, the API will extend to encompass similar functionality as demonstrated in the Cozmo Python SDK. The App (Chewie) will be using a subset of the C# SDK to connect to and communicate with Victor.  Any SDK app will be able to send the same messages that Chewie can, but some of the utilities for using pairing messages will not be included in our publicly exported code. For example, the app will have the ability to connect via BLE and pair with the robot, but the sdk will not expose those ble messages. Extra functionality will be available to the SDK that may not be used by Chewie, but are provided for users to have more fine-grained control of the robot.  Chewie (and other C# programs) only needs to know about calling the high-level functions like Robot[] FindNearbyRobots() and void TellRobotMyName(name, face_id) for theoretical examples.  The majority of messages will be passed over wifi using victor's rest api as seen in Victor Gateway.

## SDK Interface
This layer is responsible for providing a high-level abstraction that doesn't require the user to know about our communication protocols.

* Will be the way for Chewie to interact with the robot
* Provide high-level functions to tell the robot to do actions that may require many messages
    * Actions will be of varying granularity e.g. ConnectToRobot could do all the work to connect to a robot, but internally we may also expose FindNearbyRobots which is a small part of connecting to a robot
    * Chewie will define a large subset of the requirements for the interface, but will have lots of special cases where they want to do a complex UI flow where SDK users might just want to have the action happen 
* Provides delegates that users may listen on to receive notifications of events happening on the robot
* Public SDK will only communicate over Wi-Fi
    * Users will need the official app for the initial pairing flow
    * BLE will be available as a network for Chewie, but not 3rd party apps

The functions that are unused for Chewie are not MVP, and the design of the SDK Interface will end up extending upon Chewie's functionality.

## Messaging Network Service
This layer is responsible for encrypting and transmitting the message to the robot.

* Encrypts and delivers bytes to Victor
* Has no knowledge outside of what wire to write on
* Some messages will be hidden and not exposed in our public SDK
    * They'll still be accessed through the SDK layer if they exist
    * 3rd Party Developers could sniff the network and mock the messages so we can't assume they're secure