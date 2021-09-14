# Victor Switchboard

Created by Shawn Blakesley Last updated Jan 25, 2019

The robot-side BLE communications handler is a robot process called Switchboard. Switchboard communicates to the app over BLE, and is used for setting up the robot, and managing the network configuration.

## switchboard
The process on Victor that handles BLE message translation is named switchboard.  It is responsible for the authentication flow, and establishing a secure Wi-Fi connection.

* Handles initial setup flow
    * Authenticate a BLE connection
    * Connect Vector to WiFi

## Initial Pairing Flow
The initial pairing flow encompasses the steps to get Victor set up and connected to Wi-Fi. The messages to and from Victor will be CLAD messages, but will not be directly exposed in helper functions in the SDK.