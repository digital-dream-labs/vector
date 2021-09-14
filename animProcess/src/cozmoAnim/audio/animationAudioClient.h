/*
 * File: animationAudioClient.h
 *
 * Author: Jordan Rivas
 * Created: 09/12/17
 *
 * Description: Animation Audio Client is the interface to perform animation audio specific tasks. Provided a
 *              RobotAudioKeyFrame to handle the necessary audio functionality for that frame. It also provides an
 *              interface to abort animation audio and update (a.k.a. “tick”) the Audio Engine each frame.
 *
 * Copyright: Anki, Inc. 2017
 */


#ifndef __Anki_Cozmo_AnimationAudioClient_H__
#define __Anki_Cozmo_AnimationAudioClient_H__

#include "cannedAnimLib/baseTypes/audioKeyFrameTypes.h"
#include "audioEngine/audioTypeTranslator.h"
#include <set>
#include <mutex>


namespace Anki {
namespace AudioEngine {
struct AudioCallbackInfo;
}
namespace Util {
class RandomGenerator;
}
namespace Vector {
class RobotAudioKeyFrame;
class TextToSpeechComponent;

namespace Audio {
class CozmoAudioController;


class AnimationAudioClient {

public:

  static const char* kAudioLogChannelName;

  AnimationAudioClient( CozmoAudioController* audioController );

  ~AnimationAudioClient();

  inline void SetTextToSpeechComponent(TextToSpeechComponent* ttsComponent)
  {
    _ttsComponent = ttsComponent;
  }

  // Prepare to start animation
  void InitAnimation();

  // Perform functionality for frame
  void PlayAudioKeyFrame( const RobotAudioKeyFrame& keyFrame, Util::RandomGenerator* randomGen );

  // Perform abort animation audio event
  void AbortAnimation();

  // Check if there is an event being performed
  bool HasActiveEvents() const;

private:
  CozmoAudioController*                 _audioController = nullptr;
  TextToSpeechComponent*                _ttsComponent = nullptr;
  std::set<AudioEngine::AudioPlayingId> _activeEvents;
  mutable std::mutex                    _lock;

  // Key frame types
  void HandleAudioRef( const AudioKeyFrameType::AudioEventGroupRef& eventRef, Util::RandomGenerator* randomGen = nullptr );
  void HandleAudioRef( const AudioKeyFrameType::AudioStateRef& stateRef );
  void HandleAudioRef( const AudioKeyFrameType::AudioSwitchRef& switchRef );
  void HandleAudioRef( const AudioKeyFrameType::AudioParameterRef& parameterRef );

  // Perform an event
  AudioEngine::AudioPlayingId PostCozmoEvent( AudioMetaData::GameEvent::GenericEvent event,
                                              AudioMetaData::GameObjectType gameObject );

  // Update parameters for a event play id
  bool SetCozmoEventParameter( AudioEngine::AudioPlayingId playId,
                               AudioMetaData::GameParameter::ParameterType parameter,
                               AudioEngine::AudioRTPCValue value ) const;

  // Perform Event callback, used by "PostCozmoEvent()"
  void CozmoEventCallback(const uint8_t ttsID, const AudioEngine::AudioCallbackInfo& callbackInfo);

  // Track current playing events
  void AddActiveEvent( AudioEngine::AudioPlayingId playId );
  void RemoveActiveEvent( AudioEngine::AudioPlayingId playId );

};


}
}
}

#endif /* __Anki_Cozmo_AnimationAudioClient_H__ */
