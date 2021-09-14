# gRPC SDK Connection

Created by Shawn Blakesley Last updated Jan 28, 2019

## High-Level Overview
The external interface for Victor is now exposed via a gRPC service running on the robot named vic-gateway. This service also provides a REST api in front of the grpc service using a tool called grpc-gateway. This api exists because of the limitations of our C# companion app for interfacing with gRPC directly. With this design we are able to use .proto files to define both interfaces, and allow python / grpc-compatible languages to use grpc while other languages can treat it as a generic REST server.

For messages, we are now using Protocol Buffers (protobuf) for all new messages sent between vic-engine and vic-gateway. Each RPC will still need a handler implemented in vic-gateway, and the majority of the work for vic-gateway will be handling versioning conflicts between SDK Users and the robot inside these handlers.

For some old messages, we may still need to translate from protobuf to clad inside vic-gateway. However, the go emitter for clad cannot properly parse the GameToEngine and EngineToGame messages. To get things working we're currently faking those messages in clad files under clad/gateway. Eventually these new clad files will probably replace the old ones.

For getting started, you will need to read the README file located in https://github.com/anki/victor/tree/master/tools/sdk, and run the relevant commands there.

If you encounter a TLS error there are two possible things to do (until everything is set up on the robot correctly):

1. Check the time on the robot to make sure it is current.
2. Rerun make create_certs to generate a new key-cert pair


## Protocol Versioning
The version for the protocol is defined in external_interface.proto, and should be updated whenever the protobuf files are updated (and typically 1 version per release). The current (as of writing) protocol version looks like:

version definition
```
enum ProtocolVersion {
  option allow_alias = true;
  PROTOCOL_VERSION_UNKNOWN = 0; // Required zero value for enums
  PROTOCOL_VERSION_MINIMUM = 0; // Minimum supported version
  PROTOCOL_VERSION_CURRENT = 3; // Current version
}
```

When updating the version, always leave the PROTOCOL_VERSION_UNKNOWN as 0.

Only update minimum version when we explicitly want to disallow older versions. Leave at least 1 version of compatibility when deprecating old versions so the app and robot releases don't ever get completely incompatible.

## Connection Guidelines
For a list of guidelines and terms related please see SDK Guidelines.

## Adding a New Message
We'll use PlayAnimation as an example getting added to all the parts of the code where necessary.

## In The Protobuf Definition
We define the message in messages.proto (currently living in victor/cloud/gateway) which will be used by everyone including external users. This message should be robust, and consider future use cases in its definition.

cloud/gateway/messages.proto
```
message PlayAnimationRequest {
  string name  = 1;
  uint32 loops = 2;
}
 
message PlayAnimationResult {
  ResultStatus status    = 1; // generic status value for any results
  Animation    animation = 2;
  bool         success   = 3;
}
 
message GatewayWrapper {
  oneof oneof_message_type {
    ...
    PlayAnimationRequest animation_request = 6;
    PlayAnimationResult  animation_result  = 7;
    ...
  }
}
```
Then there will need to be an RPC added to external_interface.proto.

cloud/gateway/external_interface.proto
```
service ExternalInterface {
  rpc PlayAnimation(PlayAnimationRequest) returns (PlayAnimationResult) {
    option (google.api.http) = {
      post: "/v1/play_animation",
      body: "*"
    };
  }
}
```

## In Gateway
Adding to Gateway is extremely simple now:

cloud/gateway/message_handler.go
```
func (m *rpcService) PlayAnimation(ctx context.Context, in *extint.PlayAnimationRequest) (*extint.PlayAnimationResult, error) {
    // Create the response pipeline
    resultChannel := make(chan extint.GatewayWrapper)
    reflectedType := reflect.TypeOf(&extint.GatewayWrapper_PlayAnimationResult{})
    engineProtoChanMap[reflectedType] = resultChannel
    defer delete(engineProtoChanMap, reflectedType)
 
    // Write to the engine
    _, err := WriteProtoToEngine(protoEngineSock, &extint.GatewayWrapper{
        OneofMessageType: &extint.GatewayWrapper_PlayAnimationRequest{
            in,
        },
    })
    if err != nil {
        return nil, err
    }
 
    // Get the engine response
    result := <-resultChannel
    return result.GetPlayAnimationResult(), nil
}
```

## In Engine
The engine now has a ProtoMessageHandler class defined in protoMessageHandler.h. This allows any users to subscribe to protobuf messages, and broadcast those same protobuf messages out to gateway. To subscribe and send a message, grab a pointer to GatewayInterface and 

```
Result MyClass::Init(...){
  // make sure there's a pointer to GatewayInterface, _gatewayInterface, then
   
  auto myCallback = std::bind(&MyClass::HandleEvents, this, std::placeholders::_1);
  _signalHandles.push_back(_gatewayInterface->Subscribe(external_interface::GatewayWrapperTag::kPing, myCallback));
  // ...
}
 
void MyClass::HandleEvents(const AnkiEvent<external_interface::GatewayWrapper>& event) {
  switch(event.GetData().GetTag())
  {
    case external_interface::GatewayWrapperTag::kPing:
      // now send a Pong
      int payload = event.GetData().ping().ping(); // the ping message has a field "ping"
      external_interface::Pong* pong = new external_interface::Pong( 1 + payload );
      _gatewayInterface->Broadcast( ExternalMessageRouter::WrapResponse(pong) );
      break;
  }
}
```

Note that the above example uses some shortcuts provided by an Anki extension to the protobuf generated C++ code, namely the GatewayWrapperTag and the ability to pass the parameters of Pong to its constructor. GatewayWrapperTag is an enum class, found in a separate file, that matches the C-style enums used for protobuf's OneofMessageTypeCase. An example of subscribing to and sending a protobuf message using "pure" generated messages can be found in engine/cozmoAPI/comms/protoMessageHandler.cpp under PingPong and BingBong.

engine/cozmoAPI/comms/protoMessageHandler.cpp
```
Result ProtoMessageHandler::Init(...){
  // ...
  auto commonCallback = std::bind(&ProtoMessageHandler::HandleEvents, this, std::placeholders::_1);
  _signalHandles.push_back(Subscribe(external_interface::GatewayWrapper::OneofMessageTypeCase::kPing, commonCallback));
  // ...
}
 
void ProtoMessageHandler::HandleEvents(const AnkiEvent<external_interface::GatewayWrapper>& event) {
  switch(event.GetData().oneof_message_type_case())
  {
    case external_interface::GatewayWrapper::OneofMessageTypeCase::kPing:
      PingPong(event.GetData().ping());
      break;
    // ...
  }
}
 
void ProtoMessageHandler::PingPong(const external_interface::Ping& ping) {
  external_interface::Pong* pong = new external_interface::Pong();
  pong->set_pong(ping.ping() + 1);
  external_interface::GatewayWrapper wrapper;
  wrapper.set_allocated_pong(pong);
  DeliverToExternal(wrapper);
}
```

The Gateway currently handles request/response pairs from the app. In order for the engine to send a message to the app that was not requested, an Event stream is left open that the engine can broadcast to. To keep things organized and make it easier to filter messages moving forward, this Event stream is a hierarchy of oneof message types.

cloud/gateway/shared.proto
```
message Event {
  oneof event_type {
    Status                           status                              = 1;
    Onboarding                       onboarding                          = 2;
    WakeWord                         wake_word                           = 3;
    AttentionTransfer                attention_transfer                  = 4;
...
```

If you are sending an Event, a helper method ExternalMessageRouter::Wrap is provided for you that constructs an Event from a single message type. 

```
   _gatewayInterface->Broadcast( ExternalMessageRouter::Wrap(msg) );
```

On the other hand, if you are sending a response to a request, use WrapResponse:

```
   _gatewayInterface->Broadcast( ExternalMessageRouter::WrapResponse(msg) );
```

This difference is temporary; we plan to change gateway so you don't have to think about this.

## Adding a New Action Message


## Adding a New CLAD Message (for existing CLAD engine messages)
When adding a clad message, there will be a decent amount of boilerplate. This mini-guide will explain where to edit to add a clad message to the interface, and should be updated as new messages are added / changes are made.

Let's use PlayAnimation as an example. First, we define the rpc service endpoint we want to the ExternalInterface service (found in external_interface.proto):

cloud/gateway/external_interface.proto
```
service ExternalInterface {
  // ...
  // Plays an animation by name
  rpc PlayAnimation(PlayAnimationRequest) returns (PlayAnimationResult) { // An rpc named PlayAnimation which expects a PlayAnimationRequest and returns a PlayAnimationResult
    option (google.api.http) = {
      post: "/v1/play_animation", // defines the endpoint that we can send a post request to
      body: "*"
    };
  }
  // ...
}
```

Then we add the messages for PlayAnimation later on in that same file:

cloud/gateway/external_interface.proto
```
message Animation {
  string name = 1;
}
 
message PlayAnimationRequest {
  Animation animation = 1;
  uint32 loops = 2;
}
 
message PlayAnimationResult {
  ResultStatus status = 1;
}
```

Now that these are defined in the proto file, the rpc must be handled in the go message handler ( message_handler.go ):

cloud/gateway/message_handler.go
```
func (m *rpcService) PlayAnimation(ctx context.Context, in *extint.PlayAnimationRequest) (*extint.PlayAnimationResult, error) {
    fmt.Printf("Received rpc request PlayAnimation(%s)\n", in)
    return nil, status.Errorf(codes.Unimplemented, "PlayAnimation not yet implemented")
}
```

In the Go code, you'll follow the pattern of the other functions to implement the actual functionality. Try to maintain the usage of the WriteToEngine(engineSock, ProtoPlayAnimationToClad(in)) style because we're ideally going to convert these into autogenerated functions.

As for the clad messages, you'll need to add the message you want to the messageGameToEngine.clad and the messageExternalToRobot.clad. These messages must have the same format and union id in each file:

clad/src/clad/externalInterface/messageGameToEngine.clad
```
// ...
 
// This might already exist
message PlayAnimation {
  uint_32 numLoops,
  string  animationName,
  bool    ignoreBodyTrack = 0,
  bool    ignoreHeadTrack = 0,
  bool    ignoreLiftTrack = 0,
}
 
// ...
 
autounion MessageGameToEngine
{
  UiDeviceConnectionWrongVersion UiDeviceConnectionWrongVersion = 0x00, // DO NOT CHANGE THIS VALUE
  // ...
  PlayAnimation PlayAnimation = 0x02,
  // ...
}
```

clad/src/clad/gateway/messageExternalToRobot.clad
```
// ...
 
message PlayAnimation {
  uint_32 numLoops,
  string  animationName,
  bool    ignoreBodyTrack = 0,
  bool    ignoreHeadTrack = 0,
  bool    ignoreLiftTrack = 0,
}
 
// ...
 
union MessageExternalToRobot
{
  UiDeviceConnectionWrongVersion UiDeviceConnectionWrongVersion = 0x00, // DO NOT CHANGE THIS VALUE
  // ...
  PlayAnimation PlayAnimation = 0x02,
  // ...
}
```

After that it's just a matter of filling in the go code, and writing the code in the python & json test code. Examples of that may be found in the tools/sdk/gateway-tests directory. (curl examples are found in that Makefile)

There is a corresponding MessageRobotToExternal and MessageEngineToGame pair for messages coming out of the engine.


MANDATORY
The union tags, and related messages in MessageExternalToRobot and MessageGameToEngine MUST match or else you'll have a bad time.

## App Session Token Management (Current)
The current implementation of the session tracking means that we will explicitly close the event stream when another user connects. This way we won't have falsely persistent connections which are causing issues for the app.

![](images/Connection%20Tracking.png)

websequencediagrams.com code
```
title Connection Tracking
 
App->+Gateway: EventStreamRequest(conn_id)
Gateway-->Gateway: close open event streams
Gateway->Switchboard: check_connected()
Switchboard->Gateway: is_connected, conn_id
Gateway->App: is_primary response
Gateway-->App: Events
Gateway-->App:
Gateway-->App:
Gateway-->App:
note over App,Gateway: App disconnects
Gateway-->-Gateway: detect and close event stream
```

## App Session Token Management (Proposed)
The goal is to provide the benefit that we can reject incoming messages when there is already a user connected and interacting with Vector.

This is achieved by using some short-lived session id to track connections instead of using the long-lasting client_token_guid to provide message authentication.

### Valid app_session_id
For the happy path where the app session id is either currently in use by ble, or that id doesn't exist yet.

~[](images/Auth%20Flow%20Rough.png)

websequencediagrams.com code
```
SDK User->+vic-gateway: AuthRequest(client_token_guid, app_session_id) to open a stream
vic-gateway->vic-switchboard: UserConnected(app_session_id)
vic-switchboard->vic-gateway: No other BLE or same BLE connection
vic-gateway->SDK User: AuthResponse(session_token) where session_token is used for all future connections
SDK User-->vic-gateway: requests with session token
vic-gateway-->SDK User: authorized responses
vic-gateway->-SDK User: stream ends from disconnection (session token is no longer active)
SDK User-->vic-gateway: request with old session token
vic-gateway-->SDK User: unauthorized rejection
```

1. All sessions will begin with an attempted AuthRequest using the client_token_guid, and the app_session_id representing the connection id as tracked by the app.
2. The app_session_id is then passed to Switchboard
3. Switchboard will check the app_session_id against the one it is using over BLE, and tell Gateway to reject primary control if there is somebody already connected with a different id.
4. Gateway returns the AuthResponse with a new temporary session token that will last until the user disconnects.
5. All messages to the sdk will include that session token in the header as an authorization token.
6. Once the stream ends, the token becomes inactive.

Why not use the client_token_guid for everything?: This allows us to track sessions as events across two processes (switchboard and gateway), and prevents somebody from using the same long-lasting client_token_guid for multiple connections thus lowering the chance of collisions

Why does this need to happen now?: If this is delayed to another release, it will cause the next release of the vector robot to need to be in sync with the release of an app update. This is very undesirable, and unreliable.

What happens if...?: Hopefully answered by the following diagrams.

### Invalid app_session_id WiFi
In the case where another user is connected via WiFi

![](images/session_token%20failure%20WiFi.png)

websequencediagrams.com code

```
SDK User->+vic-gateway: AuthRequest(client_token_guid, app_session_id) to open a stream
vic-gateway->vic-gateway: Detects a currently active connection
vic-gateway->SDK User: AuthResponse(session_token, is_secondary)
SDK User-->vic-gateway: non-control requests with session_token
vic-gateway-->SDK User: authorized responses
SDK User-->vic-gateway: control requests with session_token
vic-gateway-->SDK User: unauthorized responses
vic-gateway->-SDK User: stream ends from disconnection (session_token is no longer active)
SDK User-->vic-gateway: request with old session_token
vic-gateway-->SDK User: unauthorized rejection
```

1. All sessions will begin with an attempted AuthRequest using the client_token_guid, and the app_session_id representing the connection id as tracked by the app.
2. Since Gateway has a currently active connection, a message is returned providing the user with a token that has authorization as a secondary app.
3. All requests that poll for information, or listen to streams will be allowed.
4. All requests that attempt to cause an action on the robot will be rejected.
5. When the stream ends, vic-gateway will invalidate the second connection's token as above.

Why not reject the second connection?: In the case where we have two SDK users connected at the same time, the second user should still be able to read data off the robot. However, it shouldn't be able to make changes to the state because the first user is in direct control.

How will we know if things are primary-only or allowed for secondary?: TBD, but it may be possible with a custom proto extension to mark things as secondary-able. May require work, but a sane default is to make everything primary-only, and the first release will probably work this way since the Python SDK isn't publicly available until December.

### Invalid app_session_id BLE

In the case where another user is connected via BLE

~[](images/session_token%20failure%20case.png)

websequencediagrams.com code

```
SDK User->+vic-gateway: AuthRequest(client_token_guid, app_session_id) to open a stream
vic-gateway->vic-switchboard: UserConnected(app_session_id)
vic-switchboard->vic-gateway: SecondaryConnectionId(reason)
vic-gateway->SDK User: AuthResponse(session_token, is_secondary)
SDK User-->vic-gateway: non-control requests with session_token
vic-gateway-->SDK User: authorized responses
SDK User-->vic-gateway: control requests with session_token
vic-gateway-->SDK User: unauthorized responses
vic-gateway->-SDK User: stream ends from disconnection (session_token is no longer active)
SDK User-->vic-gateway: request with old session_token
vic-gateway-->SDK User: unauthorized rejection
```

1. All sessions will begin with an attempted AuthRequest using the client_token_guid, and the app_session_id representing the connection id as tracked by the app.
2. The app_session_id is then passed to Switchboard
3. Switchboard will check the app_session_id against the one it is using over BLE, and tell Gateway to reject primary control if there is somebody already connected with a different id.
4. Gateway sends a message providing the user with a token that has authorization as a secondary app.
5. All requests that poll for information, or listen to streams will be allowed.
6. All requests that attempt to cause an action on the robot will be rejected.
When the stream ends, vic-gateway will invalidate the second connection's token as above.

## Xamarin Interface
Sadly, gRPC's C# wrapper is not made for Xamarin, and is a huge pain to get working properly. This means we needed to find another way to deal with gRPC messages if we wanted to have them available for all the other platforms. To accomplish this, we've included grpc-gateway which will allow the server to provide a REST API in front of the grpc server. The only limitation this imposes is that we cannot have a stream to stream message using the grpc-gateway. This interface will be flushed out in more detail in Victor C# SDK (Chewie and CodeLab).

## Python Interface
For python, we're able to use grpc directly. Specifically for our use cases, it makes sense to use aiogrpc which allows us to asynchronously communicate with the robot. This interface will be flushed out in Victor Python SDK.

## Robot Interface
The details for the robot side of the interface are specified in Victor Gateway.