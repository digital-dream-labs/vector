/**
 * File: sdkAudioComponent.h
 *
 * Author: Bruce von Kugelgen
 *
 * Description: Component wrapper to generate, cache and use wave data from an SDK message.
 *
 * Copyright: Anki, Inc. 2016-2019
 *
 */

#ifndef __Anki_cozmo_sdkAudioComponent_H__
#define __Anki_cozmo_sdkAudioComponent_H__

#include "clad/types/sdkAudioTypes.h"
#include "audioEngine/audioTools/standardWaveDataContainer.h"
#include "audioEngine/audioTools/streamingWaveDataInstance.h"

// Forward declarations
namespace Anki {
  namespace Vector {
    namespace Anim {
      class AnimContext;
    }
    namespace Audio {
      class CozmoAudioController;
    }
    namespace RobotInterface {
      struct ExternalAudioPrepare;
      struct ExternalAudioChunk;
      struct ExternalAudioComplete;
      struct ExternalAudioCancel;
    }
  }
}


namespace Anki {
namespace Vector {

class SdkAudioComponent
{
public:
  //Constructor, destructor
  SdkAudioComponent(const Anim::AnimContext* context);
  ~SdkAudioComponent();

  //
  // CLAD message handlers are called on the main thread to handle incoming requests.
  //
  void HandleMessage(const RobotInterface::ExternalAudioPrepare& msg);
  void HandleMessage(const RobotInterface::ExternalAudioChunk& msg);
  void HandleMessage(const RobotInterface::ExternalAudioComplete& msg);
  void HandleMessage(const RobotInterface::ExternalAudioCancel& msg);


private:
  // -------------------------------------------------------------------------------------------------------------------
  // Private types
  // -------------------------------------------------------------------------------------------------------------------
  using AudioController = Vector::Audio::CozmoAudioController;
  using StreamingWaveDataPtr = std::shared_ptr<AudioEngine::StreamingWaveDataInstance>;
  using AudioCallbackType = std::function<void()>;

  // -------------------------------------------------------------------------------------------------------------------
  // Private members
  // -------------------------------------------------------------------------------------------------------------------

  AudioController * _audioController = nullptr;
  StreamingWaveDataPtr _waveData = nullptr;
  uint16_t  _audioRate;
  uint32_t  _totalAudioFramesReceived;
  bool      _audioPrepared;
  bool      _audioPosted;
  std::shared_ptr<AudioCallbackType> _audioPlaybackFinishedPtr;

  // -------------------------------------------------------------------------------------------------------------------
  // Private methods
  // -------------------------------------------------------------------------------------------------------------------

  // Set up Audio Engine to play text's audio data
  bool PrepareAudioEngine(const RobotInterface::ExternalAudioPrepare& msg );
  bool AddAudioChunk(const RobotInterface::ExternalAudioChunk& msg );
  bool PostAudioEvent();    
  void OnAudioCompleted();
  void StopActiveAudio();
  void ClearActiveAudio();
  void CleanupAudioEngine();    
  void ClearOperationData();
  bool SetPlayerVolume(float volume) const;
  bool SendAnimToEngine(SDKAudioStreamingState audioState, uint32_t audioSent=0, uint32_t audioPlayed=0);
};

}   // end namespace Anki
}   // end namespace Vector

#endif // __Anki_cozmo_sdkAudioComponent_H__
