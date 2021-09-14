/*
 * File: engineRobotAudioInput.h
 *
 * Author: Jordan Rivas
 * Created: 9/12/2017
 *
 * Description: This is a subclass of AudioMuxInput which provides communication between itself and an
 *              EndingRobotAudioClient by means of EngineToRobot and RobotToEngine messages. It's purpose is to perform
 *              audio tasks sent from the engine process to the audio engine in the animation process.
 *
 * Copyright: Anki, Inc. 2017
 */

#ifndef __Cozmo_Anim_Audio_EngineRobotAudioInput_H__
#define __Cozmo_Anim_Audio_EngineRobotAudioInput_H__


#include "audioEngine/multiplexer/audioMuxInput.h"
#include "clad/robotInterface/messageEngineToRobot.h"


namespace Anki {
namespace Vector {
namespace Audio {

class EngineRobotAudioInput : public AudioEngine::Multiplexer::AudioMuxInput {
  
public:
  
  virtual void PostCallback( AudioEngine::Multiplexer::AudioCallbackDuration&& callbackMessage ) const override;
  virtual void PostCallback( AudioEngine::Multiplexer::AudioCallbackMarker&& callbackMessage ) const override;
  virtual void PostCallback( AudioEngine::Multiplexer::AudioCallbackComplete&& callbackMessage ) const override;
  virtual void PostCallback( AudioEngine::Multiplexer::AudioCallbackError&& callbackMessage ) const override;
  
  virtual void HandleMessage( const AudioEngine::Multiplexer::PostAudioEvent& eventMessage ) override;
  virtual void HandleMessage( const AudioEngine::Multiplexer::StopAllAudioEvents& stopEventMessage ) override;
  virtual void HandleMessage( const AudioEngine::Multiplexer::PostAudioGameState& gameStateMessage ) override;
  virtual void HandleMessage( const AudioEngine::Multiplexer::PostAudioSwitchState& switchStateMessage ) override;
  virtual void HandleMessage( const AudioEngine::Multiplexer::PostAudioParameter& parameterMessage ) override;
  virtual void HandleMessage( const AudioEngine::Multiplexer::PostAudioMusicState& musicStateMessage ) override;
};


} // Audio
} // Cozmo
} // Anki


#endif /* __Cozmo_Anim_Audio_EngineRobotAudioInput_H__ */
