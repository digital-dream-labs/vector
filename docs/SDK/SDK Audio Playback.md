# SDK Audio Playback

Created by Bruce vonKugelgen Last updated Jan 25, 2019

**This is not to be confused with accessing the Robot's audio feed**

## Feature
VIC-13004 - Robot Plays Audio from SDK CLOSED
* SDK users will be able to play 8- and 16-bit wav/aif files
* Audio will be streamed to minimize memory use and latency
* Files for playback must be mono, and no more than 16 khz (maybe 32 khz for 8 bit?)

## Messaging
A bidirectional gRPC stream will be used to send requests to the robot, as well as receive responses/results.  

### Messages to Robot
`AudioStreamRequest` will contain an `audio_request_type`

* `start_stream`
* `continue_stream`
* `close_stream`

### Messages from Robot

`AudioStreamResponse` will contain an `audio_response_type`

* `stream_started`
* `stream_closed`

### Include?
`stream_status` for `audio_request_type`?

that would add stream_active and stream_inactive to audio_response_type

## Exceptions and Error Conditions

`stream_cancelled` 

`stream_starving`

`stream_not_started`

`stream_data_not_added`


![](images/SDK%20Audio%20Playback%20Messaging.png)



websequencediagrams.com source

```
title SDK Streaming Audio Playback
participant SDK
participant Gateway
participant Engine
participant Anim
 
alt First Audio Chunk
note right of SDK: New gRPC channel for audio streaming
SDK->Gateway:  AudioStreamRequest(start_stream)
Gateway->Engine:  GatewayWrapperTag::kAudioStreamRequest
note over Engine: SDKComponent.cpp::HandleProtoMessage()
Engine->Anim:  ExternalAudioMessage (CLAD)
note over Anim:
    new SDKAudioComponent.cpp
     
    First chunk received
end note
Anim->Audio: Prepare plugIn
Anim->Audio: AppendStandardWaveData
Anim->Audio: PostAudioEvent
Anim->Engine:  ExternalAudioPlayEvent(StreamStarted)
Engine->Gateway: wrapped AudioStreamResponse
Gateway->SDK:  AudioStreamResponse(stream_started)
end alt
loop While data exists
SDK->Gateway:  AudioStreamRequest(continue_stream
Gateway->Engine:  GatewayWrapperTag::kAudioStreamRequest
Engine->Anim:  ExternalAudioMessage (CLAD)
Anim->Audio: AppendStandardWaveData
note left of Audio: 
No return message sent in order
to reduce message overhead
end note
end loop
alt End of Stream
note right of SDK: 
    Send when cancelling streaming
    or after last chunk sent
end note
SDK->Gateway:  AudioStreamRequest(close_stream)
Gateway->Engine:  GatewayWrapperTag::kAudioStreamRequest
Engine->Anim:  ExternalAudioMessage (CLAD)
Anim->Audio:  DoneProducingData
note over Audio:  when final buffer completed
Audio->Anim:  completion callback
Anim->Audio:  cleanup
Anim->Engine:  ExternalAudioPlayEvent(StreamClosed)
Engine->Gateway: wrapped AudioStreamResponse
Gateway->SDK:  AudioStreamResponse(stream_closed)
end alt
```