/**
 * File: microphoneAudioClient.h
 *
 * Author: Jordan Rivas
 * Created: 06/20/18
 *
 * Description: Mic Direction Audio Client receives Mic Direction messages to update the audio engine with the current
 *              mic data. By providing this to the audio engine it can adjust the audio mix and volume to better serve
 *              the current enviromnent.
 *
 * Copyright: Anki, Inc. 2018
 *
 **/


#include "cozmoAnim/audio/microphoneAudioClient.h"

#include "audioEngine/audioTypeTranslator.h"
#include "clad/robotInterface/messageRobotToEngine.h"
#include "cozmoAnim/audio/cozmoAudioController.h"
#include "util/console/consoleInterface.h"
#include "util/logging/logging.h"
#include "util/math/math.h"


namespace Anki {
namespace Vector {
namespace Audio {
namespace {
#define CONSOLE_PATH "Audio.Microphone"
// Shift noise floor value
CONSOLE_VAR_RANGED(float, kNoiseFloorMin, CONSOLE_PATH, 1.5f, 0.0f, 10.0f);
// Range of normalize noise floor
CONSOLE_VAR_RANGED(float, kNoiseFloorRange, CONSOLE_PATH, 5.5f, 0.0f, 10.0f);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
MicrophoneAudioClient::MicrophoneAudioClient( CozmoAudioController* audioController )
: _audioController( audioController )
{
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
MicrophoneAudioClient::~MicrophoneAudioClient()
{
  _audioController = nullptr;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void MicrophoneAudioClient::ProcessMessage( const RobotInterface::MicDirection& msg )
{
  if ( nullptr != _audioController ) {
    const float noiseFloor = log10(msg.latestNoiseFloor);
    float normNoiseFloor = MAX((noiseFloor - kNoiseFloorMin), 0.0f); // Shift floor down
    normNoiseFloor = MIN((normNoiseFloor / kNoiseFloorRange), 1.0f); // Normalize and limit ceiling
    
    using AudioParameter = AudioMetaData::GameParameter::ParameterType;
    const auto param = AudioEngine::ToAudioParameterId( AudioParameter::Robot_Vic_Environment_Ambient_Volume );
    _audioController->SetParameter( param,
                                    normNoiseFloor,
                                    AudioEngine::kInvalidAudioGameObject ); // Set Global Parameter
  }
}

}
}
}

