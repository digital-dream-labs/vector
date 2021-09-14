/**
* File: streamingAnimationModifier.h
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

#ifndef __Anki_Cozmo_StreamingAnimationModifier_H__
#define __Anki_Cozmo_StreamingAnimationModifier_H__


#include "coretech/common/shared/types.h"
#include <map>

namespace Anki {
namespace Vector {

namespace RobotInterface{
struct AlterStreamingAnimationAtTime;
struct EngineToRobot;
}

namespace Anim {
class AnimationStreamer;
}
class TextToSpeechComponent;

namespace Audio {
class EngineRobotAudioInput;
}

namespace Anim {
class StreamingAnimationModifier
{
public:
  StreamingAnimationModifier(AnimationStreamer* streamer, 
                             Audio::EngineRobotAudioInput* audioInput, 
                             TextToSpeechComponent* ttsComponent);
  virtual ~StreamingAnimationModifier();

  // Messages applied before update will be displayed to users that tick (e.g. display a new image)
  void ApplyAlterationsBeforeUpdate(AnimationStreamer* streamer);
  // Messages applied after update will be applied after the keyframe's processed (e.g. lock face track after drawing an image)
  void ApplyAlterationsAfterUpdate(AnimationStreamer* streamer);

  void HandleMessage(const RobotInterface::AlterStreamingAnimationAtTime& msg);

private:
  std::multimap<TimeStamp_t, RobotInterface::EngineToRobot> _streamTimeToMessageMap;
  Audio::EngineRobotAudioInput* _audioInput = nullptr;
  TextToSpeechComponent* _ttsComponent = nullptr;

  void ApplyMessagesHelper(AnimationStreamer* streamer, TimeStamp_t streamTime_ms);
  void ApplyMessageToStreamer(AnimationStreamer* streamer, const RobotInterface::EngineToRobot& msg);
  void AddToMapStreamMap(TimeStamp_t relativeStreamTime_ms, RobotInterface::EngineToRobot&& msg);

}; // class StreamingAnimationModifier
  

} // namespace Anim
} // namespace Vector
} // namespace Anki



#endif /* __Anki_Cozmo_StreamingAnimationModifier_H__ */
