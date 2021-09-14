/**
* File: textToSpeechComponent.h
*
* Author: Various Artists
*
* Description: Component wrapper to generate, cache and use wave data from a given string and style.
*
* Copyright: Anki, Inc. 2016-2018
*
*/

#ifndef __Anki_cozmo_cozmoAnim_textToSpeech_textToSpeechComponent_H__
#define __Anki_cozmo_cozmoAnim_textToSpeech_textToSpeechComponent_H__

#include "audioEngine/audioTools/standardWaveDataContainer.h"
#include "audioEngine/audioTools/streamingWaveDataInstance.h"
#include "audioEngine/audioTypes.h"
#include "coretech/common/shared/types.h"
#include "clad/audio/audioEventTypes.h"
#include "clad/audio/audioGameObjectTypes.h"
#include "clad/audio/audioSwitchTypes.h"
#include "clad/types/textToSpeechTypes.h"
#include "util/helpers/templateHelpers.h"
#include <deque>
#include <mutex>
#include <map>

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
      struct TextToSpeechPrepare;
      struct TextToSpeechPlay;
      struct TextToSpeechCancel;
    }
    namespace TextToSpeech {
      class TextToSpeechProvider;
    }
  }
  namespace Util {
    namespace Dispatch {
      class Queue;
    }
  }
}

namespace Anki {
namespace Vector {

class TextToSpeechComponent
{
public:
  // Public type declarations
  using TTSID_t = uint8_t;

  // Public constants
  static constexpr TTSID_t kInvalidTTSID = 0;

  // Constructor, destructor
  TextToSpeechComponent(const Anim::AnimContext* context);
  ~TextToSpeechComponent();

  // Reports active TTSID (if any), else kInvalidTTSID
  TTSID_t GetActiveTTSID() { return _activeTTSID; }

  //
  // CLAD message handlers are called on the main thread to handle incoming requests.
  //
  void HandleMessage(const RobotInterface::TextToSpeechPrepare& msg);
  void HandleMessage(const RobotInterface::TextToSpeechPlay& msg);
  void HandleMessage(const RobotInterface::TextToSpeechCancel& msg);

  //
  // Update method is called once per tick on main thread. This method responds to events
  // posted by worker thread and performs tasks in synchronization with the rest of the animation engine.
  //
  void Update();

  //
  // Called on main thread to set a new locale
  //
  void SetLocale(const std::string & locale);

  // Callbacks invoked by audio engine
  void OnAudioPlaying(const TTSID_t ttsID);
  void OnAudioComplete(const TTSID_t ttsID);
  void OnAudioError(const TTSID_t ttsID);

private:
  // -------------------------------------------------------------------------------------------------------------------
  // Private types
  // -------------------------------------------------------------------------------------------------------------------
  using AudioController = Vector::Audio::CozmoAudioController;
  using StreamingWaveDataPtr = std::shared_ptr<AudioEngine::StreamingWaveDataInstance>;
  using AudioTtsProcessingStyle = AudioMetaData::SwitchState::Robot_Vic_External_Processing;
  using TextToSpeechProvider = TextToSpeech::TextToSpeechProvider;
  using DispatchQueue = Util::Dispatch::Queue;
  using EventTuple = std::tuple<TTSID_t, TextToSpeechState, f32>;
  using EventQueue = std::deque<EventTuple>;

  // Audio creation state
  enum class AudioCreationState {
    None,       // No data available
    Preparing,  // Audio generation in progress
    Playable,   // Audio is ready to play
    Prepared    // Audio is complete
  };

  // TTS data bundle
  struct TtsBundle
  {
    // TTS request context
    TextToSpeechTriggerMode triggerMode = TextToSpeechTriggerMode::Invalid;
    AudioCreationState state = AudioCreationState::None;
    AudioTtsProcessingStyle style = AudioTtsProcessingStyle::Unprocessed;
    StreamingWaveDataPtr waveData;
  };

  // Shared pointer to data bundle
  using BundlePtr = std::shared_ptr<TtsBundle>;

  // -------------------------------------------------------------------------------------------------------------------
  // Private members
  // -------------------------------------------------------------------------------------------------------------------

  // Internal mutex
  mutable std::mutex _lock;

  // Map of data bundles
  std::map<TTSID_t, BundlePtr> _bundleMap;

  // Active TTSID, if any
  TTSID_t _activeTTSID;

  // Audio controller provided by context
  AudioController * _audioController = nullptr;

  // Worker thread
  DispatchQueue * _dispatchQueue = nullptr;

  // Platform-specific provider
  std::unique_ptr<TextToSpeechProvider> _pvdr;

  // Thread-safe event queue
  EventQueue _event_queue;
  std::mutex _event_mutex;

  // -------------------------------------------------------------------------------------------------------------------
  // Private methods
  // -------------------------------------------------------------------------------------------------------------------

  // Thread-safe event notifications
  void PushEvent(const EventTuple& event);
  bool PopEvent(EventTuple& event);

  // Initialize TTS utterance and get first chunk of TTS audio.
  // Returns RESULT_OK on success, else error code.
  // Sets done to true when audio generation is complete.
  Result GetFirstAudioData(const std::string & text,
                           float durationScalar,
                           float pitchScalar,
                           const StreamingWaveDataPtr & data,
                           bool & done);

  // Get next chunk of TTS audio.
  // Returns RESULT_OK on success, else error code.
  // Sets done to true when audio generation is complete.
  Result GetNextAudioData(const StreamingWaveDataPtr & data, bool & done);

  // Get bundle for given ID
  // Returns nullptr if ID is not found
  BundlePtr GetBundle(const TTSID_t ttsID);

  // Asynchronous create the wave data for the given text and style, to be played later
  // Use GetOperationState() to check if wave data is Ready
  // Return RESULT_OK on success
  Result CreateSpeech(const TTSID_t ttsID,
                      const TextToSpeechTriggerMode triggerMode,
                      const std::string& text,
                      const AudioTtsProcessingStyle style,
                      const float durationScalar,
                      const float pitchScalar);

  // Set up Audio Engine to play text's audio data
  // out_duration_ms provides approximate duration of event before processing in audio engine
  // Return false if the audio has NOT been created or is not yet ready. Output parameters will NOT be valid.
  bool PrepareAudioEngine(const TTSID_t ttsID, float& out_duration_ms);

  // Clear speech audio data from audio engine and clear operation data
  void CleanupAudioEngine(const TTSID_t ttsID);

  // Clear speech operation audio data from memory
  void ClearOperationData(const TTSID_t ttsID);

  // Clear ALL loaded text audio data from memory
  void ClearAllLoadedAudioData();

  //
  // State transition helpers
  // These methods are called by Update() on the main thread, in response to events posted by the worker thread.
  //
  void OnStateInvalid(const TTSID_t ttsID);
  void OnStatePreparing(const TTSID_t ttsID);
  void OnStatePlayable(const TTSID_t ttsID, const f32 duration_ms);
  void OnStatePrepared(const TTSID_t ttsID, const f32 duration_ms);

  // Audio helpers
  void SetAudioProcessingStyle(AudioTtsProcessingStyle style);
  bool PostAudioEvent(uint8_t ttsID);
  void StopActiveTTS();
  void ClearActiveTTS();

  // SWAG estimate of final duration
  f32 GetEstimatedDuration_ms(const std::string & text);
  f32 GetDuration_ms(const StreamingWaveDataPtr & waveData);
  f32 GetDuration_ms(const BundlePtr & bundle);

  // AudioEngine Callbacks
  void OnUtteranceCompleted(uint8_t ttsID);

}; // class TextToSpeechComponent


} // end namespace Vector
} // end namespace Anki


#endif //__Anki_cozmo_cozmoAnim_textToSpeech_textToSpeechComponent_H__
