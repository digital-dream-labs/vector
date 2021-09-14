/**
* File: streamingAnimationModifier.cpp
*
* Authors: Kevin M. Karol
* Created: 6/10/18
*
* Description: 
*   1) Receives messages from engine that should be applied to the animation streamer
*      at a specific timestep in animation playback
*   2) Monitors the animation streamer's playback time and applies messages appropriately
*
* Copyright: Anki, Inc. 2018
*
**/

#include "cozmoAnim/animation/streamingAnimationModifier.h"

#include "clad/robotInterface/messageEngineToRobotTag.h"
#include "cozmoAnim/animation/animationStreamer.h"
#include "cozmoAnim/audio/engineRobotAudioInput.h"
#include "cozmoAnim/textToSpeech/textToSpeechComponent.h"
#include "util/logging/logging.h"

namespace Anki {
namespace Vector {
namespace Anim {

namespace{
const uint8_t kOffsetForEndOfFrame = 1;
}

StreamingAnimationModifier::StreamingAnimationModifier(AnimationStreamer* streamer, Audio::EngineRobotAudioInput* audioInput, TextToSpeechComponent* ttsComponent)
{
  auto newAnimationCallback = [this](){
    _streamTimeToMessageMap.clear();
  };
  streamer->AddNewAnimationCallback(newAnimationCallback);
  _audioInput = audioInput;
  _ttsComponent = ttsComponent;
}

StreamingAnimationModifier::~StreamingAnimationModifier()
{
  
}

void StreamingAnimationModifier::ApplyMessagesHelper(AnimationStreamer* streamer, TimeStamp_t streamTime_ms)
{
  auto it = _streamTimeToMessageMap.begin();
  while(it != _streamTimeToMessageMap.end()){
    if(it->first <= streamTime_ms){
      ApplyMessageToStreamer(streamer, it->second);
      it = _streamTimeToMessageMap.erase(it);
    }else{
      break;
    }
  }
}

void StreamingAnimationModifier::ApplyAlterationsBeforeUpdate(AnimationStreamer* streamer)
{
  const TimeStamp_t streamTime_ms = streamer->GetRelativeStreamTime_ms();
  ApplyMessagesHelper(streamer, streamTime_ms);
}

void StreamingAnimationModifier::ApplyAlterationsAfterUpdate(AnimationStreamer* streamer)
{
  const TimeStamp_t streamTime_ms = streamer->GetRelativeStreamTime_ms();
  // subtract one to avoid keyframes scheduled for the next tick
  ApplyMessagesHelper(streamer, streamTime_ms - 1);
}

void StreamingAnimationModifier::HandleMessage(const RobotInterface::AlterStreamingAnimationAtTime& msg)
{
  auto relativeStreamTime_ms = msg.relativeStreamTime_ms;
  if((relativeStreamTime_ms % ANIM_TIME_STEP_MS) != 0){
    relativeStreamTime_ms -= (relativeStreamTime_ms % ANIM_TIME_STEP_MS);
    PRINT_NAMED_WARNING("StreamingAnimationModifier.DelayPending.InvalidDelay",
                        "Delay %d is not a multiple of animation time step %d - \
                        it will be updated to %d to align with preceeding frame",
                        msg.relativeStreamTime_ms, ANIM_TIME_STEP_MS, relativeStreamTime_ms);
  }

  // If this message should be applied at the end of the tick increase its time by 1
  // This keeps lookups efficient without creating a new map to track this data 
  if(!msg.applyBeforeTick){
    relativeStreamTime_ms += kOffsetForEndOfFrame;
  }

  RobotInterface::EngineToRobot alterationMessage;
  switch(static_cast<RobotInterface::EngineToRobotTag>(msg.internalTag)){
    case RobotInterface::EngineToRobotTag::setFullAnimTrackLockState:
    {
      RobotInterface::EngineToRobot alterationMessage(std::move(msg.setFullAnimTrackLockState));
      AddToMapStreamMap(relativeStreamTime_ms, std::move(alterationMessage));
      break;
    }
    case RobotInterface::EngineToRobotTag::postAudioEvent:
    {
      if(ANKI_DEV_CHEATS){
        ANKI_VERIFY(msg.postAudioEvent.callbackId == 0, "StreamingAnimationModifier.HandleMessage.InvalidCallbackID",
                    "Callbacks are not currently supported for altering the streaming animation");
        ANKI_VERIFY(msg.postAudioEvent.gameObject == Anki::AudioMetaData::GameObjectType::Animation,
                    "StreamingAnimationModifier.HandleMessage.PostAudioEvent.ImproperGameObject", 
                    "All game objects sent through alter streaming animation must have object type Animation");
      }

      RobotInterface::EngineToRobot alterationMessage(std::move(msg.postAudioEvent));
      AddToMapStreamMap(relativeStreamTime_ms, std::move(alterationMessage));
      break;
    }
    case RobotInterface::EngineToRobotTag::textToSpeechPlay:
    {
      RobotInterface::EngineToRobot alterationMessage(std::move(msg.textToSpeechPlay));
      AddToMapStreamMap(relativeStreamTime_ms, std::move(alterationMessage));
      break;
    }

    default:
    {
      PRINT_NAMED_ERROR("AnimationComponent.AlterStreamingAnimationAtTime.UnsupportedMessageType",
                        "Message Type %d is not currently implemented - update clad and and anim process to support",
                        msg.internalTag);
      break;
    }
  }
}


void StreamingAnimationModifier::ApplyMessageToStreamer(AnimationStreamer* streamer, 
                                                        const RobotInterface::EngineToRobot& msg)
{
  switch(msg.tag){
    case (uint32_t)RobotInterface::EngineToRobotTag::setFullAnimTrackLockState:
    {
      streamer->SetLockedTracks(msg.setFullAnimTrackLockState.trackLockState);
      break;
    }
    case (uint32_t)RobotInterface::EngineToRobotTag::postAudioEvent:
    {
      if(_audioInput != nullptr){
        _audioInput->HandleMessage(msg.postAudioEvent);
      }
      break;
    }
    case (uint32_t)RobotInterface::EngineToRobotTag::textToSpeechPlay:
    {
      if(_ttsComponent != nullptr){
        _ttsComponent->HandleMessage(msg.textToSpeechPlay);
      }
      break;
    }
    default:
    {
      PRINT_NAMED_ERROR("StreamingAnimationModifier.ApplyMessageToStreamer.NoImplementation",
                        "Attempted to apply message tag of type %d to streamer, but no implementation was found",
                        static_cast<int>(msg.tag));
    }
  }
}

void StreamingAnimationModifier::AddToMapStreamMap(TimeStamp_t relativeStreamTime_ms, RobotInterface::EngineToRobot&& msg)
{
  _streamTimeToMessageMap.emplace(relativeStreamTime_ms, std::move(msg));
}


} // namespace Anim
} // namespace Vector
} // namespace Anki

