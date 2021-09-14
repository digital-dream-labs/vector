/*
 * File:          animEngine.h
 * Date:          6/26/2017
 * Author:        Kevin Yoon
 *
 * Description:   A platform-independent container for spinning up all the pieces
 *                required to run Vector Animation Process.
 *
 */

#ifndef ANKI_VECTOR_ANIM_ENGINE_H
#define ANKI_VECTOR_ANIM_ENGINE_H

#include "json/json.h"

#include "coretech/common/shared/types.h"

// Forward declarations
namespace Anki {
  namespace Vector {
    namespace Anim {
      class AnimContext;
      class AnimationStreamer;
      class StreamingAnimationModifier;
    }
    class TextToSpeechComponent;
    class SdkAudioComponent;
    
    namespace Audio {
      class CozmoAudioController;
      class MicrophoneAudioClient;
    } // Audio
    namespace RobotInterface {
      struct SetLocale;
      struct ExternalAudioChunk;
      struct ExternalAudioPrepare;
      struct ExternalAudioComplete;
      struct ExternalAudioCancel;
      struct TextToSpeechPrepare;
      struct TextToSpeechPlay;
      struct TextToSpeechCancel;
    } // RobotInterface
  } // Vector
  namespace Util {
    namespace Data {
      class DataPlatform;
    }
  } // Util
} // Anki

namespace Anki {
namespace Vector {
namespace Anim {

class AnimEngine
{
public:

  AnimEngine(Util::Data::DataPlatform* dataPlatform);
  ~AnimEngine();

  Result Init();

  // Hook this up to whatever is ticking the game "heartbeat"
  Result Update(const BaseStationTime_t currTime_nanosec);

  void RegisterTickPerformance(const float tickDuration_ms,
                               const float tickFrequency_ms,
                               const float sleepDurationIntended_ms,
                               const float sleepDurationActual_ms) const;

  // Message handlers
  void HandleMessage(const RobotInterface::SetLocale& msg);
  void HandleMessage(const RobotInterface::ExternalAudioPrepare& msg);
  void HandleMessage(const RobotInterface::ExternalAudioChunk& msg);
  void HandleMessage(const RobotInterface::ExternalAudioComplete& msg);
  void HandleMessage(const RobotInterface::ExternalAudioCancel& msg);
  void HandleMessage(const RobotInterface::TextToSpeechPrepare& msg);
  void HandleMessage(const RobotInterface::TextToSpeechPlay& msg);
  void HandleMessage(const RobotInterface::TextToSpeechCancel& msg);
  
protected:

  bool                                          _isInitialized = false;
  std::unique_ptr<AnimContext>                  _context;
  std::unique_ptr<AnimationStreamer>            _animationStreamer;
  std::unique_ptr<StreamingAnimationModifier>   _streamingAnimationModifier;
  std::unique_ptr<TextToSpeechComponent>        _ttsComponent;
  std::unique_ptr<Audio::MicrophoneAudioClient> _microphoneAudioClient;
  std::unique_ptr<SdkAudioComponent>            _sdkAudioComponent;
  Audio::CozmoAudioController*                  _audioControllerPtr = nullptr;
  
}; // class AnimEngine

} // namespace Anim
} // namespace Vector
} // namespace Anki

#endif // ANKI_VECTOR_ANIM_ENGINE_H
