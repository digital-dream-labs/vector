# SDK Guidelines

Created by Shawn Blakesley Last updated Jun 20, 2018

## Terminology

| Term       | Description |
|------------|-------------|
|Protobuf    |Protocol Buffers. Google's equivalent to CLAD.|
|RPC         |Remote Procedure Call. A data defined operation that mimics invoking a function on a server from a client.|
|gRPC        | Google's RPC. An RPC syntax defined inside of Protobuf. See Go Basics and Python's aiogrpc for server and client respectively.|
|vic-gateway |SDK gRPC server on the robot. It communicates to vic-engine via a domain socket that now talks Protobuf. There is an old CLAD socket that still exists for now / the foreseeable future.|
|grpc-gateway|A reverse proxy that provides a REST API wrapping the gRPC server.|

## Protobuf

Protobuf is similar to CLAD in many ways. It is a data definition language that allows us to define the messages we will be passing between the outside world and the engine. It's best to go over the documentation for proto3 syntax to understand the available features.

For our use cases, the most interesting thing is our definition of the message GatewayWrapper in messages.proto. This is equivalent to MessageGameToEngine in the old CLAD world, and represents all the messages we want to send to and from the engine.

## gRPC

A single RPC looks like

```
rpc PlayAnimation(PlayAnimationRequest) returns (PlayAnimationResult) { // This defines a function "PlayAnimation" that takes a "PlayAnimationRequest" and returns a "PlayAnimationResult"
  option (google.api.http) = { // this is boilerplate for adding the rest api
    post: "/v1/play_animation", // the api accepts a "post" request at the url "/v1/play_animation"
    body: "*" // not entirely clear what this means, but it appears to be necessary for all messages
  };
}
```

As convention, the functions should take a Request and return a Result. The Result should have some generic status information as its first field.

```
message ResultStatus {
  string description = 1; // this should probably be an enum, but right now it's a string as a placeholder
}
 
message PlayAnimationResult {
  ResultStatus status = 1; // this provides a generic way to update the status for everything all in one request
}
 
message SomeOtherResult {
  ResultStatus status = 1; // use the same ResultStatus here
  string some_value = 2;
}
```

## Gateway Go Code
For every rpc you must add a handler in go. Examples to come.

```
func (m *rpcService) Pang(ctx context.Context, in *extint.Ping) (*extint.Pong, error) {
    log.Println("Received rpc request Ping(", in, ")")
 
    _, err := WriteProtoToEngine(protoEngineSock, &extint.GatewayWrapper{
        OneofMessageType: &extint.GatewayWrapper_Ping{
            in,
        },
    })
    if err != nil {
        return nil, err
    }
 
    return &extint.Pong{Pong: 1}, nil
}
```

Or if you need a response from the engine:

```
func (m *rpcService) Pang(ctx context.Context, in *extint.Ping) (*extint.Pong, error) {
    log.Println("Received rpc request Ping(", in, ")")
     
    f, result := createChannel(extint.GatewayWrapper_Pong{})
    defer f()
 
    _, err := WriteProtoToEngine(protoEngineSock, &extint.GatewayWrapper{
        OneofMessageType: &extint.GatewayWrapper_Ping{
            in,
        },
    })
    if err != nil {
        return nil, err
    }
 
    pong := <-result
    return pong.GetPong(), nil
}
```
