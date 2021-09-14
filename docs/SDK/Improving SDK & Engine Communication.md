# Improving SDK & Engine Communication

Created by Shawn Blakesley Last updated Jun 18, 2018

### Decision Made

We decided to use protobuf inside vic-engine. This document is here mainly for posterity. Please view this document for details about using protobuf in the engine.

## Problem Definition

We're looking to establish a connection from Victor Gateway to the Engine by which adding a new message to the Vector interface is as simple as possible. Specifically, we want to be able to write a message in one place and basically have the communication work instantly.

## Current State
As demonstrated in gRPC SDK Connection, there are significant changes and clean up that must be done to make the SDK more maintainable.

A single message must be written in three distinct places (2 clad and 1 proto file) just to expose it to gateway.

In addition, the code surrounding each message and manually writing the translation code between proto and clad.

It's already getting unwieldy with an extremely small set of messages.

## Solution Goals

There are a few primary goals we should target with our solution:

* Avoid repetition: we should ideally have a single message definition, so people don't need to look in a bunch of places to understand what's being passed around.
* Maintain simplicity: the code to read and process a message should be easy to write, maintain, and understand
* Maintain performance: passing messages between gateway and engine should aim to remain close to their current performance.
* Minimize disruption: the changes should aim to disrupt the least people possible

## Options
To solve these issues, we propose there three solutions that can be achieved in the current timeframe.

1. ✅ Use protobuf-lite in the engine to parse incoming and construct outgoing messages.
2. Use flatbuffers in the engine, and translate from protobuf inside gateway.
3. Use clad in the engine, and rewrite all of the existing clad messages to be parsable by golang.

We decided to go the route of using protobuf in the engine, and it is currently being converted over. For the majority of new messages, they should be written in protobuf (everything without a good reason to not be in proto).

To learn more about the details of using protobuf in the engine, please read gRPC SDK Connection.

## [Implemented!] Use Protobuf Everywhere
### Advantages
+ All messages defined in a single file

+ No custom codegen or manual conversion of messages in gateway

+ vic-gateway will only need to handling versioning and rpc calls. Not translating message formats.

### Disadvantages
- Message compression / decompression will happen for messages sent to and from the engine (adding cpu overhead)

- Engine would need to build with protobuf

Example
```
func (m *rpcService) PlayAnimation(ctx context.Context, in *extint.PlayAnimationRequest) (*extint.PlayAnimationResult, error) {
    animation_result := make(chan RobotToExternalResult)
    engineChanMap[extint.PlayAnimationResult] = animation_result // This code will change as a part of other improvements
    defer ClearMapSetting(extint.PlayAnimationResult)
    // TODO: this is where versioning changes would happen
    _, err := WriteToEngine(engineSock, in)
    if err != nil {
        return nil, err
    }
    result <-animation_result // TODO: Properly handle the result
    return &result, nil
}
```


### Work Remaining
* Convert vic-engine to pass protobuf messages to the external world
* Update webots, webviz, etc. to speak protobuf instead of clad

## Convert Incoming Protobuf to Flatbuffers
### Advantages
+ All messages defined in a single file (proto generates fbs during build)

+ Zero copy means both small memory footprint and little to no added cpu overhead (in engine)

### Disadvantages
- Still requires custom code generation

- Less obvious syntax than clad

### Example

```
func (m *rpcService) PlayAnimation(ctx context.Context, in *extint.PlayAnimationRequest) (*extint.PlayAnimationResult, error) {
    animation_result := make(chan RobotToExternalResult)
    engineChanMap[extint.PlayAnimationResult] = animation_result // This code will change as a part of other things
    defer ClearMapSetting(extint.PlayAnimationResult)
    // TODO: this is where versioning changes would happen
 
    // v proto -> flatbuffer
    builder := flatbuffers.NewBuilder(0)
    builder.Reset()
    name_position := builder.CreateString(in.Animation.Name)
 
    flat.AnimationStart(builder)
    flat.AnimationAddName(builder, name_position)
    flat.AnimationAddNumLoops(builder, in.num_loops)
    flat.AnimationAddIgnoreLiftTrack(builder, in.ignore_lift_track)
    flat.AnimationAddIgnoreHeadTrack(builder, in.ignore_head_track)
    flat.AnimationAddIgnoreBodyTrack(builder, in.ignore_body_track)
    user_position := flat.AnimationEnd(builder)
 
    builder.Finish(user_position)
    // ^ proto -> flatbuffer
 
    _, err := WriteToEngine(engineSock, builder.Bytes(user_position)) // This is approximately the right function to get the bytes
    if err != nil {
        return nil, err
    }
    result := <-animation_result // TODO: Properly handle the result
    // v flatbuffer -> proto (Plan to be autogenerated eventually)
        // TODO: probably similar to setting the values
        // Creating some anim protobuf value
    // ^ flatbuffer -> proto
    return &extint.PlayAnimationResult{
        Status: &extint.ResultStatus{
            Description: "Animation completed",
        },
        Animation: &anim, 
    }, nil
}
```
 
 
### Work Remaining
* Convert vic-engine to pass flatbuffer messages to the external world
* Update webots, webviz, etc. to speak protobuf instead of clad
* Write code to autogenerate flatbuffer to protobuf golang conversions

## Convert Incoming Protobuf to CLAD
### Advantages
+ We already use it everywhere else

+ Easier to create custom translations than it is with flatbuffers

### Disadvantages
- Go emitter is still in its infancy, and cannot handle the complex structure of existing GameToEngine (This means we'd need to rewrite the majority of clad)

- Manual conversion (like with flatbuffers)

- A single message must be written in both proto and clad to be usable

### Example

```
func ProtoPlayAnimationToClad(msg *extint.PlayAnimationRequest) *gw_clad.MessageExternalToRobot {
    if msg.Animation == nil {
        return nil
    }
    log.Println("Animation name:", msg.Animation.Name)
    return gw_clad.NewMessageExternalToRobotWithPlayAnimation(&gw_clad.PlayAnimation{
        NumLoops:        msg.Loops,
        AnimationName:   msg.Animation.Name,
        IgnoreBodyTrack: msg.IgnoreBodyTrack,
        IgnoreHeadTrack: msg.IgnoreHeadTrack,
        IgnoreLiftTrack: msg.IgnoreLiftTrack,
    })
}
 
func (m *rpcService) PlayAnimation(ctx context.Context, in *extint.PlayAnimationRequest) (*extint.PlayAnimationResult, error) {
    animation_result := make(chan RobotToExternalResult)
    engineChanMap[gw_clad.MessageRobotToExternalTag_RobotCompletedAction] = animation_result
    defer ClearMapSetting(gw_clad.MessageRobotToExternalTag_RobotCompletedAction)
    _, err := WriteToEngine(engineSock, ProtoPlayAnimationToClad(in))
    if err != nil {
        return nil, err
    }
    result := <-animation_result
 
    return &extint.PlayAnimationResult{
        Status: &extint.ResultStatus{
            Description: "Animation completed",
        },
        Animation: &extint.Animation{Name: result.Name}, 
    }, nil
}
```

### Work Remaining

* Write code to autogenerate clad to protobuf golang conversions
* Write tests to verify that messages match between clad and proto definitions
* Improve clad emitter for go to be more robust
* Restructure clad messages for talking to gateway
