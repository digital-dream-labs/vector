# Victor Bluetooth Low Energy (BLE) Requirements
Created by Stuart Eichert Last updated Dec 08, 2017

On Victor, we will use Bluetooth Low Energy for the following:

1. To act as a server. Users/developers will be able to connect to Victor from their smartphones or computers to configure WiFi, upload an SSH key (for developers only), and execute arbitrary commands (for developers only).  In addition, users will be able to use BLE as the sole means of communication between the app and Victor if necessary.
2. To communicate with Victor's cubes. Victor's cubes are BLE peripherals and Victor will be responsible for finding them, connecting to them, and telling them what to do. Victor will also update their firmware over a BLE connection.

## General Requirements
1. Be able to determine whether a Bluetooth controller is present
2. Be able to turn the controller on / off
3. Be able to set the name associated with the controller 

## BLE Central Mode
1. Start and stop scan for advertising BLE peripherals and optionally filter advertisements to only see Victor's cubes.
2. Establish and maintain connections to 3 or more peripherals at once. The connections can be established serially, but after all 3 are active, it must be possible to communicate with any peripheral at any time.
3. Be notified as soon as possible if a connection is dropped. We may want to try and re-establish the connection or inform a higher layer of the loss of connection.
4. Perform service discovery to gather the provided services, characteristics, and descriptors of the peripherals.
5. Send a request to the peripheral to be notified when a new value is available to read from a particular characteristic.
6. Be able to write to a peripheral's characteristics both with and without response.
7. Be able to initiate a disconnection at any time.

## BLE Peripheral Mode
1. Be able to set the contents of the advertising and scan response packets that are broadcast. Specifically want to broadcast the flags to indicate that we can be connected to, the service UUID, and the name (for developer mode only right now). Possibly want to set the manufacturer data.
2. Be able to broadcast advertisements at least 2 times per second.
3. Be able to start/stop advertisement broadcast
4. Be able to accept inbound low energy (LE) connect requests
5. Be notified of connection state (connection and disconnection) within 1 second of it happening.
6. Be able to reject additional inbound connections after the first one is established.
7. Be able to respond to service discovery requests from connected centrals. Either responding to the live query or having already provided the information about services, characteristics, and descriptors.
8. Be able to respond to requests for notifications about changes in values of readable characteristics.
9. Be able to respond to read/write characteristic/descriptor requests.
10. For writable characteristics, be able to support write with response and without.
11. For sending notifications about changes in value of readable characteristics, be able to know if sending the notification failed.
12. Be able to do all the proceeding while still acting as a BLE Central (see above) and maintaining connections to cubes.