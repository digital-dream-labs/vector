/**
 * File: cozmoAudioController.h
 *
 * Author: Jordan Rivas
 * Created: 09/07/2017
 *
 * Description: Cozmo interface to Audio Engine
 *              - Subclass of AudioEngine::AudioEngineController
 *              - Implement Cozmo specific audio functionality
 *
 * Copyright: Anki, Inc. 2017
 *
 **/

#ifndef __Anki_Cozmo_CozmoAudioController_H__
#define __Anki_Cozmo_CozmoAudioController_H__

#include "audioEngine/audioEngineController.h"
#include <atomic>
#include <memory>
#include <map>


namespace Anki {
namespace AudioEngine {
class SoundbankLoader;
}
namespace AudioMetaData {
namespace GameParameter {
// Forward declare audioParameterTypes.h
enum class ParameterType : uint32_t;
}
}
namespace Vector {
namespace Anim {
  class AnimContext;
}
namespace Audio {


class CozmoAudioController : public AudioEngine::AudioEngineController
{
public:

  CozmoAudioController(const Anim::AnimContext* context);

  virtual ~CozmoAudioController();
  
  // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
  // Overrides of AudioEngineController's basic audio engine functions
  // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
  using AudioEventId = AudioEngine::AudioEventId;
  using AudioPlayingId = AudioEngine::AudioPlayingId;
  using AudioGameObject = AudioEngine::AudioGameObject;
  using AudioStateGroupId = AudioEngine::AudioStateGroupId;
  using AudioStateId = AudioEngine::AudioStateId;
  using AudioSwitchGroupId = AudioEngine::AudioSwitchGroupId;
  using AudioSwitchStateId = AudioEngine::AudioSwitchStateId;
  using AudioCallbackContext = AudioEngine::AudioCallbackContext;
  enum { kInvalidAudioGameObject = (AudioGameObject)-1 };
  virtual AudioPlayingId PostAudioEvent( const std::string& eventName,
                                         AudioGameObject gameObjectId = kInvalidAudioGameObject,
                                         AudioCallbackContext* callbackContext = nullptr ) override;
  
  virtual AudioPlayingId PostAudioEvent( AudioEventId eventId,
                                         AudioGameObject gameObjectId = kInvalidAudioGameObject,
                                         AudioCallbackContext* callbackContext = nullptr ) override;
  
  // Stops playing all sounds on the specified game object
  // + If kInvalidAudioGameObject is specified, then ALL audio will be stopped
  virtual void StopAllAudioEvents( AudioGameObject gameObjectId = kInvalidAudioGameObject ) override;
  
  virtual bool SetState( AudioStateGroupId stateGroupId,
                         AudioStateId stateId ) const override;
  
  virtual bool SetSwitchState( AudioSwitchGroupId switchGroupId,
                               AudioSwitchStateId switchStateId,
                               AudioGameObject gameObjectId ) const override;
  // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
  
  // Save session profiler capture to a file
  bool WriteProfilerCapture( bool write );
  // Save session audio output to a file
  bool WriteAudioOutputCapture( bool write );
  // Console helpers
  static void RemoveCaptureFiles( const std::string& dirPath, const std::string& fileExtension, uint8_t maxCount );
  static std::string CreateFormattedUtcDateTimeString();
  
  // Activate consumable parameters to get updated Audio Engine runtime values
  // See cozmoAudioController.cpp for "consumable parameters" list
  // Return true if successfully activated/deactivated
  bool ActivateParameterValueUpdates( bool activate );
  bool GetActivatedParameterValue( AudioMetaData::GameParameter::ParameterType parameter,
                                   AudioEngine::AudioRTPCValue& out_value );

private:
  
  const Anim::AnimContext* _animContext = nullptr;
  std::unique_ptr<AudioEngine::SoundbankLoader> _soundbankLoader;
  // Parameter Value Update functionality
  AudioEngine::AudioEngineCallbackId _parameterUpdateCallbackId = AudioEngine::kInvalidAudioEngineCallbackId;
  std::map<AudioMetaData::GameParameter::ParameterType,
           std::atomic<AudioEngine::AudioRTPCValue>> _consumableParameterValues;
  
  // Register CLAD Game Objects
  void RegisterCladGameObjectsWithAudioController();
  
  // Setup the structures of consumable Audio Engine Parameters
  void SetupConsumableAudioParameters();
  
  bool ParameterUpdatesIsActive() const
  { return ( _parameterUpdateCallbackId != AudioEngine::kInvalidAudioEngineCallbackId ); }
  
};

}
}
}

#endif /* __Anki_Cozmo_CozmoAudioController_H__ */
