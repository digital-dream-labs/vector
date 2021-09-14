/**
* File: cozmoAnim/showAudioStreamStateManager.h
*
* Author: Kevin M. Karol
* Created: 8/3/2018
*
* Description: Communicates the current state of cloud audio streaming to the user
* and ensures expectations of related animation components are met (e.g. motion/lack there of when streaming)
*
* Copyright: Anki, Inc. 2018
**/

#ifndef COZMO_ANIM_SHOW_AUDIO_STREAM_STATE_MANAGER_H
#define COZMO_ANIM_SHOW_AUDIO_STREAM_STATE_MANAGER_H

#include "clad/robotInterface/messageEngineToRobot.h"
#include "coretech/common/shared/types.h"
#include "cozmoAnim/animContext.h"

namespace Anki {
namespace Vector {

namespace Anim {
  class AnimationStreamer;
}
enum class AlexaUXState : uint8_t;

namespace Audio {
class EngineRobotAudioInput;
}

class ShowAudioStreamStateManager{
public:

  ShowAudioStreamStateManager(const Anim::AnimContext* context);
  virtual ~ShowAudioStreamStateManager();

  void Update();
  
  void SetAnimationStreamer(Anim::AnimationStreamer* streamer)
  {
    _streamer = streamer;
  }

  // Most functions here need to be thread safe due to them being called from trigger word detected
  // callbacks which happen on a separate thread
  
  void SetTriggerWordResponse(const RobotInterface::SetTriggerWordResponse& msg);
  
  void SetAlexaUXResponses(const RobotInterface::SetAlexaUXResponses& msg);
  
  // Start the robot's response to the trigger in order to indicate that the robot may be streaming audio
  // The GetInAnimation is optional, the earcon and backpack lights are not
  using OnTriggerAudioCompleteCallback = std::function<void(bool success)>;
  void SetPendingTriggerResponseWithGetIn(OnTriggerAudioCompleteCallback = {});
  void SetPendingTriggerResponseWithoutGetIn(OnTriggerAudioCompleteCallback = {});

  // Indicates whether or not the audio stream state manager will be able to indicate to the user
  // that streaming may be happening - if this returns false 
  bool HasValidTriggerResponse();

  // Indicates whether voice data should be streamed to the cloud after the trigger response has indicated to
  // the user that streaming may be happening
  bool ShouldStreamAfterTriggerWordResponse();
  
  bool ShouldSimulateStreamAfterTriggerWord();

  uint32_t GetMinStreamingDuration();
  
  // with the exception of HasAnyAlexaResponse, alexa methods should be called on the main thread.
  // This is only because the current Alexa implementation fits this constraint, so I'm assuming it here.
  bool HasAnyAlexaResponse() const; // ok to call off thread
  bool HasValidAlexaUXResponse(AlexaUXState state) const;
  bool StartAlexaResponse(AlexaUXState state, bool ignoreGetIn = false);
  
  void SetOnCharger(bool onCharger) { _onCharger = onCharger; }
  void SetFrozenOnCharger(bool frozenOnCharger) { _frozenOnCharger = frozenOnCharger; }

private:

  void StartTriggerResponseWithGetIn(OnTriggerAudioCompleteCallback = {});
  void StartTriggerResponseWithoutGetIn(OnTriggerAudioCompleteCallback = {});


  const Anim::AnimContext* _context = nullptr;
  Anim::AnimationStreamer* _streamer = nullptr;

  Anki::AudioEngine::Multiplexer::PostAudioEvent _postAudioEvent;
  int32_t _minStreamingDuration_ms;
  bool _shouldTriggerWordStartStream;
  bool _shouldTriggerWordSimulateStream;
  uint8_t _getInAnimationTag;
  std::string _getInAnimName;
  
  bool _frozenOnCharger = false;
  bool _onCharger = false;

  // Trigger word responses are triggered via callbacks from the trigger word detector thread
  // so we need to be thread safe and have pending responses to be executed on the main thread in Update
  mutable std::recursive_mutex _triggerResponseMutex;
  bool _havePendingTriggerResponse = false;
  bool _pendingTriggerResponseHasGetIn = false;
  OnTriggerAudioCompleteCallback _responseCallback;
  
  // Alexa-specific get-ins and audio info
  struct AlexaInfo
  {
    AlexaUXState state; // a transition from Idle to this state will trigger the below response
    Anki::AudioEngine::Multiplexer::PostAudioEvent audioEvent;
    uint8_t getInAnimTag;
    std::string getInAnimName;
  };
  std::vector<AlexaInfo> _alexaResponses;
  
  
};

} // namespace Vector
} // namespace Anki

#endif  // #ifndef COZMO_ANIM_SHOW_AUDIO_STREAM_STATE_MANAGER_H
