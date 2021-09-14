# Victor SDK Design

Created by Shawn Blakesley Last updated May 22, 2018

## Goals
* Allow for easy, secure communication with Victor
* Provide a consistent high-level API for the Chewie, Python Applications, and Third-Party C# Apps
* Translate messages to avoid the types of version mismatch problems described in the Victor Versioning Plan

## Glossary

| Term        | Description |
|-------------|-------------|
|SDK User     | Any code that uses the sdk to talk to the robot. This includes chewie, codelab, the python sdk, and external apps.|
|External SDK | The interface that will be made easily accessible to non-Anki users of the SDK.|
|Internal SDK | The set of messages that are only used under the hood of the SDK. While not technically excluded from the External SDK, these messages will not have helpers exposed to external users. This is most closely implemented in C# where the Internal SDK adds functionality on top of the External SDK which contains all the CLAD messages.|
|App SDK      | The C# SDK can sometimes be referred to as the App SDK because the intention is that users may use it to create Xamarin apps.|
|Switchboard  | The BLE message handler. Currently used for pairing messages via clad. Part of the sdk interface for the limited subset of messages that will be available via ble.|
| Gateway     | Robot message handler that handles version translations for SDK messages|

## Connecting to the Robot

There will be a few paths for connecting to the robot.

1. There will be a BLE connection to do the initial pairing flow (see below), this will be done in the clear to establish a secure connection.
2. The rest of the messages will use a grpc server with an added rest interface to connect over wifi (as describe in the Victor Gateway documentation).
    a. To establish this connection, the app will provide a way of accessing the ip address of Victor and exposing that to a user.
3. Some subset of the wifi messages may have a BLE analogue to allow for communication while wifi is not available (at a later date).

## Considerations
* All messages between Victor and the app (or 3rd party app / python sdk) will be going through the SDK.
* The SDK will be implemented in a number of languages. C# and Python are the most important two, but we should design to be useful for more languages.
* There are two parts to the SDK, the Internal SDK and the External SDK.
 The majority of the SDK is what's being called the External SDK.
        * The External SDK is what will be deployed publicly to external users.
        * This includes all the useful helper functions and higher-level design for a nice user-facing api.
    * The private layer of the SDK is called the Internal SDK.
        * The Internal SDK will not be made easily accessible in the External SDK.
        * There may be higher-level functions available for internal users, but external users would have to implement such helpers themselves.
* The first goal for the SDK is to provide a communication layer between the Victor Companion App (Chewie), and Victor
* The majority of the App-Side Messaging Layer will be exposed as part of the External SDK.
* Code written in the SDK will be released to the public.

For any language that wants to support the SDK we only require that they support rest calls (basically everything). For ideal communication they would want to use gRPC directly.

## Sharing SDK Messages with Victor and Apps
### Separate Repository

To support the ability to have a number of separate repos which all make use of the same messages, we created a separate repo that holds all the details on building the external interfaces themselves: victor-clad (should probably rename this). This way, that repo may be included as a submodule or subtree for any other repo that uses the messages and they will have the ability to generate compatible code.

To facilitate ease of use, this repo provides the following:

* CLAD emitters for each language in which we support switchboard connections
* CMake wrappers that may be included in other repos which will allow it to build generated code.
* A configure script for repos that may only want to generate for the external interface.

#### Pros
* Changing the public interfaces are made obvious to developers by being commits to a separate repo.
* Each submodule may point to a different commit to the repo as desired.
    * This is only a good thing if: versioning works well, and sdk users have tags to point to specific victor releases of message versions.
* Code generation may be integrated as part of the build for sdk users.
* SDK Users may add messages and write code before it needs to be implemented on the robot.

#### Cons
* Requires understanding of CLAD generation in submodules.
* Simultaneous changes to victor code and clad may only be indicated by a submodule update.

There were two other considerations, but for now we're going to go with a separate repository. We may reconsider as we discover more pain points, so the second option was to have the victor code completely own the messages, and the generated C# code be delivered as a versioned build artifact to the chewie repo. Right now we're not going with that to hopefully make early development easier with each team only generating the files they need for their projects.

## Best Practices
* Extensive documentation should be written, and should be as accessible to complete newcomers as is possible.
* Code in the SDK will be public. Avoid mentioning future products, jokes, peoples' names or anything that you wouldn't want the outside world to hear about.
* There should be a mixture of low and high level functions for every action.
    * ex. An eternal user may want to connect to Victor, and only know if it worked or not, but the App Team will need to hook into each step of the connection. The best way to serve both is to provide the low-level functions to do the individual steps of connection, and also provide a high-level function like bool connect() that would do all the steps as part of one function call.
* If you need help designing for external users, talk to somebody on the SDK team.

## Next Steps

Learn more about the app side of the SDK with Victor C# SDK (Chewie and CodeLab).

Learn more about the robot-side ble connection with Victor Switchboard.

Learn more about the robot-side wifi connection with Victor Gateway.

Learn more about the traditional SDK with Victor Python SDK.

