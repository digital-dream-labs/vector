/**
 * File: audioLayerManager.cpp
 *
 * Authors: Al Chaussee
 * Created: 06/28/2017
 *
 * Description: Specific track layer manager for RobotAudioKeyFrame
 *
 * Copyright: Anki, Inc. 2017
 *
 **/

#include "cannedAnimLib/baseTypes/audioKeyFrameTypes.h"
#include "cozmoAnim/animation/trackLayerManagers/audioLayerManager.h"
#include "util/console/consoleInterface.h"
#include <algorithm>


namespace Anki {
namespace Vector {
namespace Anim {

namespace
{
  const auto kProceduralGameObject = AudioMetaData::GameObjectType::Procedural;
  #define CONSOLE_PATH "Audio.KeepAlive"
  CONSOLE_VAR(bool, kEnableKeepAliveEyeBlinkAudioEvents, CONSOLE_PATH, true);
  CONSOLE_VAR(bool, kEnableKeepAliveEyeDartAudioEvents, CONSOLE_PATH, true);
  CONSOLE_VAR(bool, kEnableKeepAliveEyeSquintAudioEvents, CONSOLE_PATH, true);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
AudioLayerManager::AudioLayerManager(const Util::RandomGenerator& rng)
: ITrackLayerManager<RobotAudioKeyFrame>(rng)
{
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
  Result AudioLayerManager::AddEyeBlinkToAudioTrack(const std::string& layerName,
                                                    const BlinkEventList& eventList,
                                                    const TimeStamp_t timeSinceAnimStart_ms)
{
  if (!_enabled || !kEnableKeepAliveEyeBlinkAudioEvents) {
    return RESULT_OK;
  }
  
  using namespace AudioKeyFrameType;
  using namespace AudioMetaData;
  Animations::Track<RobotAudioKeyFrame> audioTrack;
  
  const auto eventIt = std::find_if(eventList.begin(), eventList.end(), [](const BlinkEvent& event)
                                                                        { return (BlinkState::Closed == event.state); });
  if (eventIt != eventList.end()) {
    // Add Event Group
    RobotAudioKeyFrame frame;
    AudioEventGroupRef eventGroup(kProceduralGameObject);
    eventGroup.AddEvent(GameEvent::GenericEvent::Play__Robot_Vic_Sfx__Scrn_Procedural_Blink, 1.0f, 1.0f);
    frame.AddAudioRef(std::move(eventGroup));
    frame.SetTriggerTime_ms(eventIt->time_ms);
    audioTrack.AddKeyFrameToBack(frame);
  }

  if (audioTrack.IsEmpty()) {
    // Don't add an empty track
    return RESULT_OK;
  }
  
  return AddLayer(layerName, audioTrack);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
Result AudioLayerManager::AddEyeDartToAudioTrack(const std::string& layerName,
                                                 const TimeStamp_t interpolationTime_ms,
                                                 const TimeStamp_t timeSinceAnimStart_ms)
{
  if (!_enabled || !kEnableKeepAliveEyeDartAudioEvents) {
    return RESULT_OK;
  }
  
  using namespace AudioKeyFrameType;
  using namespace AudioMetaData;
  RobotAudioKeyFrame frame;
  Animations::Track<RobotAudioKeyFrame> audioTrack;
  // Add parameter
  auto paramRef = AudioParameterRef(GameParameter::ParameterType::Robot_Vic_Screen_Shift_Interpolation_Time,
                                    interpolationTime_ms,
                                    0.0f,
                                    AudioEngine::Multiplexer::CurveType::Linear,
                                    kProceduralGameObject);
  frame.AddAudioRef(std::move(paramRef));
  // Add Event Group
  AudioEventGroupRef eventGroup(kProceduralGameObject);
  eventGroup.AddEvent(GameEvent::GenericEvent::Play__Robot_Vic_Sfx__Scrn_Procedural_Shift, 1.0f, 1.0f);
  frame.AddAudioRef(std::move(eventGroup));
  frame.SetTriggerTime_ms(interpolationTime_ms);   // Always start with begining of movement
  audioTrack.AddKeyFrameToBack(frame);
  
  return AddLayer(layerName, audioTrack);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
Result AudioLayerManager::AddEyeSquintToAudioTrack(const std::string& layerName,
                                                   const TimeStamp_t timeSinceAnimStart_ms)
{
  if (!_enabled || !kEnableKeepAliveEyeSquintAudioEvents) {
    return RESULT_OK;
  }
  
  using namespace AudioKeyFrameType;
  using namespace AudioMetaData;
  Animations::Track<RobotAudioKeyFrame> audioTrack;
  RobotAudioKeyFrame frame;
  // Add Event Group
  AudioEventGroupRef eventGroup(kProceduralGameObject);
  eventGroup.AddEvent(GameEvent::GenericEvent::Play__Robot_Vic_Sfx__Scrn_Procedural_Squint, 1.0f, 1.0f);
  frame.AddAudioRef(std::move(eventGroup));
  frame.SetTriggerTime_ms(timeSinceAnimStart_ms);  // Always start with begining of movement
  audioTrack.AddKeyFrameToBack(frame);
  
  return AddLayer(layerName, audioTrack);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void AudioLayerManager::GenerateGlitchAudio(u32 numFramesToGen,
                                            Animations::Track<RobotAudioKeyFrame>& outTrack) const
{
  if (!_enabled) {
    return;
  }
  
  // TODO: VIC-447: Restore glitching
  /*
  float prevGlitchAudioSampleVal = 0.f;
  
  for(int i = 0; i < numFramesToGen; i++)
  {
    AnimKeyFrame::AudioSample sample;
    for(int i = 0; i < sample.sample.size(); ++i)
    {
      // Adapted Brownian noise generator from
      // https://noisehack.com/generate-noise-web-audio-api/
      const float rand = GetRNG().RandDbl() * 2 - 1;
      prevGlitchAudioSampleVal = (prevGlitchAudioSampleVal + (0.02 * rand)) / 1.02;
      sample.sample[i] = ((prevGlitchAudioSampleVal * 3.5) + 1) * 128;
    }

    outTrack.AddKeyFrameToBack(sample);
  }
   */
}

}
}
}
