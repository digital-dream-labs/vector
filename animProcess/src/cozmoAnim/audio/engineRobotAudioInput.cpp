/*
 * File: engineRobotAudioInput.cpp
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


#include "cozmoAnim/audio/engineRobotAudioInput.h"
#include "cozmoAnim/animProcessMessages.h"
#include "clad/audio/audioMessageTypes.h"
#include "clad/robotInterface/messageRobotToEngine_sendAnimToEngine_helper.h"
#include <util/logging/logging.h>


namespace Anki {
namespace Vector {
namespace Audio {

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void EngineRobotAudioInput::HandleMessage( const AudioEngine::Multiplexer::PostAudioEvent& postAudioEvent ) 
{
  AudioEngine::Multiplexer::AudioMuxInput::HandleMessage(postAudioEvent);
}
void EngineRobotAudioInput::HandleMessage( const AudioEngine::Multiplexer::StopAllAudioEvents& stopAllAudioEvents )
{
  AudioEngine::Multiplexer::AudioMuxInput::HandleMessage(stopAllAudioEvents);
}
void EngineRobotAudioInput::HandleMessage( const AudioEngine::Multiplexer::PostAudioGameState& postAudioGameState )
{
  AudioEngine::Multiplexer::AudioMuxInput::HandleMessage(postAudioGameState);
}
void EngineRobotAudioInput::HandleMessage( const AudioEngine::Multiplexer::PostAudioSwitchState& postAudioSwitchState )
{
  AudioEngine::Multiplexer::AudioMuxInput::HandleMessage(postAudioSwitchState);
}
void EngineRobotAudioInput::HandleMessage( const AudioEngine::Multiplexer::PostAudioParameter& postAudioParameter )
{
  AudioEngine::Multiplexer::AudioMuxInput::HandleMessage(postAudioParameter);
}
void EngineRobotAudioInput::HandleMessage( const AudioEngine::Multiplexer::PostAudioMusicState& postAudioMusicState )
{
  AudioEngine::Multiplexer::AudioMuxInput::HandleMessage(postAudioMusicState);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void EngineRobotAudioInput::PostCallback( AudioEngine::Multiplexer::AudioCallbackDuration&& callbackMessage ) const
{
  if (!RobotInterface::SendAnimToEngine(callbackMessage)) {
    PRINT_NAMED_ERROR("EngineRobotAudioInput.PostCallback", "Failed.SendMessageToEngine.AudioCallbackDuration");
  }
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void EngineRobotAudioInput::PostCallback( AudioEngine::Multiplexer::AudioCallbackMarker&& callbackMessage ) const
{
  if (!RobotInterface::SendAnimToEngine(callbackMessage)) {
    PRINT_NAMED_ERROR("EngineRobotAudioInput.PostCallback", "Failed.SendMessageToEngine.AudioCallbackMarker");
  }
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void EngineRobotAudioInput::PostCallback( AudioEngine::Multiplexer::AudioCallbackComplete&& callbackMessage ) const
{
  if (!RobotInterface::SendAnimToEngine(callbackMessage)) {
    PRINT_NAMED_ERROR("EngineRobotAudioInput.PostCallback", "Failed.SendMessageToEngine.AudioCallbackComplete");
  }
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void EngineRobotAudioInput::PostCallback( AudioEngine::Multiplexer::AudioCallbackError&& callbackMessage ) const
{
  if (!RobotInterface::SendAnimToEngine(callbackMessage)) {
    PRINT_NAMED_ERROR("EngineRobotAudioInput.PostCallback", "Failed.SendMessageToEngine.AudioCallbackError");
  }
}

  
} // Audio
} // Cozmo
} // Anki
