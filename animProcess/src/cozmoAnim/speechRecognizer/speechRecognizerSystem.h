/**
* File: speechRecognizerSystem.h
*
* Author: Jordan Rivas
* Created: 10/23/2018
*
* Description: Speech Recognizer System handles high level speech features, such as, locale and multiple triggers
*
* Copyright: Anki, Inc. 2018
*
*/

#ifndef __AnimProcess_VictorAnim_SpeechRecognizerSystem_H_
#define __AnimProcess_VictorAnim_SpeechRecognizerSystem_H_

#include "audioUtil/audioDataTypes.h"
#include "cozmoAnim/micData/micTriggerConfig.h"
#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <string>

namespace Anki {
  namespace AudioUtil {
    class SpeechRecognizer;
    struct SpeechRecognizerCallbackInfo;
    struct SpeechRecognizerIgnoreReason;
  }
  namespace Vector {
    class Alexa;
    class AlexaPlaybackRecognizerComponent;
    namespace Anim {
      class AnimContext;
    }
    namespace MicData {
      class MicDataSystem;
    }
    class NotchDetector;
    namespace Anim {
      class RobotDataLoader;
    }
    class SpeechRecognizerTHF;
    class SpeechRecognizerPryonLite;
    namespace {
      struct TriggerModelTypeData;
    }
  }
  namespace Util {
    class Locale;
  }
}

namespace Anki {
namespace Vector {

class SpeechRecognizerSystem
{
public:

  friend class AlexaPlaybackRecognizerComponent;

  SpeechRecognizerSystem(const Anim::AnimContext* context,
                         MicData::MicDataSystem* micDataSystem,
                         const std::string& triggerWordDataDir);
  
  ~SpeechRecognizerSystem();
  
  SpeechRecognizerSystem(const SpeechRecognizerSystem& other) = delete;
  SpeechRecognizerSystem& operator=(const SpeechRecognizerSystem& other) = delete;
  
  using TriggerWordDetectedCallback = std::function<void(const AudioUtil::SpeechRecognizerCallbackInfo& info)>;
  using AlexaTriggerWordDetectedCallback = std::function<void(const AudioUtil::SpeechRecognizerCallbackInfo& info,
                                                              const AudioUtil::SpeechRecognizerIgnoreReason& reason)>;
  
  // Init Vector trigger detector
  // Note: This always happens at boot
  void InitVector(const Anim::RobotDataLoader& dataLoader,
                  const Util::Locale& locale,
                  TriggerWordDetectedCallback callback);

  // set whether the notch detector should be active (for alexa keyword only). When active,
  // alexa triggers get dropped if we detect a notch.
  void ToggleNotchDetector(bool active);
  
  // add 'raw' audio samples
  void UpdateNotch(const AudioUtil::AudioSample* audioChunk, unsigned int audioDataLen);

  // Update recognizer audio
  // NOTE: Always call from the same thread
  void Update(const AudioUtil::AudioSample * audioData, unsigned int audioDataLen, bool vadActive);
  
  // Set Default models for locale
  // Use flag to describe what recognizer(s) to updated
  // Return true when locale file was found and is different then current locale
  // NOTE: Locale is not updated until the next Update() call
  enum RecognizerTypeFlag {
    None          = 0,
    VectorMic     = 1 << 0,
    AlexaMic      = 1 << 1,
    AlexaPlayback = 1 << 2,
    All           = VectorMic | AlexaMic | AlexaPlayback
  };
  
  bool UpdateTriggerForLocale(const Util::Locale& newLocale,
                              RecognizerTypeFlag recognizerFlags = RecognizerTypeFlag::All);
  
  // Alexa Methods
  // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
  // Alexa has been set active set current locale and callback for Alexa trigger recognitions
  void ActivateAlexa(const Util::Locale& locale, AlexaTriggerWordDetectedCallback callback);
  
  // Alexa has been disabled, turn off the "Alexa" recognizer
  void DisableAlexa();
  
  // Start/Stop playback recognizer when Alexa is in Speaking state
  void SetAlexaSpeakingState(bool isSpeaking);
  // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -


private:
  
  // Trigger context
  template <class SpeechRecognizerType>
  struct TriggerContext {
    std::string                                 name;
    std::unique_ptr<SpeechRecognizerType>       recognizer;
    std::unique_ptr<MicData::MicTriggerConfig>  micTriggerConfig;

    // For tracking and altering the trigger model being used
    MicData::MicTriggerConfig::TriggerDataPaths currentTriggerPaths;
    MicData::MicTriggerConfig::TriggerDataPaths nextTriggerPaths;
    
    bool                                        useVad;

    TriggerContext(const std::string& name, bool useVad)
    : name(name)
    , recognizer(std::make_unique<SpeechRecognizerType>())
    , micTriggerConfig(std::make_unique<MicData::MicTriggerConfig>())
    , useVad(useVad)
    { }
  };
  
  using TriggerContextThf = TriggerContext<SpeechRecognizerTHF>;
  using TriggerContextPryon = TriggerContext<SpeechRecognizerPryonLite>;
  
  const Anim::AnimContext*                    _context = nullptr;
  MicData::MicDataSystem*                     _micDataSystem = nullptr;
  std::unique_ptr<TriggerContextThf>          _victorTrigger;
  
  std::unique_ptr<TriggerContextPryon>        _alexaTrigger;
  Alexa*                                      _alexaComponent = nullptr;
  bool                                        _isAlexaActive = false;
  
  std::unique_ptr<TriggerContextPryon>        _alexaPlaybackTrigger;
  std::atomic_uint64_t                        _playbackTrigerSampleIdx{ 0 };
  std::atomic_bool                            _isDisableAlexaPending{ false };
  
  std::string                                 _triggerWordDataDir;
  
  std::mutex                                  _triggerModelMutex;
  std::atomic_bool                            _isPendingLocaleUpdate{ false };
  
  std::unique_ptr<AlexaPlaybackRecognizerComponent>   _alexaPlaybackRecognizerComponent;
  
  std::shared_ptr<NotchDetector>              _notchDetector;
  std::mutex                                  _notchMutex;
  bool                                        _notchDetectorActive = false;
  
  // Alexa Methods
  // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
  // Init Alexa trigger detector
  // Note: This is done after Alex user has been authicated
  void InitAlexa(const Util::Locale& locale,
                 const AlexaTriggerWordDetectedCallback callback);
  
  // Init Alex playback trigger detector
  void InitAlexaPlayback(const Util::Locale& locale,
                         TriggerWordDetectedCallback callback);
  
  // Check Alexa component states to update _isAlexaActive flag
  void UpdateAlexaActiveState();
  // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
  
  // Set custom model and search files for locale
  // Return true when locale file was found and is different then current locale
  // NOTE: This only sets the _nextTriggerPaths the locale will be updated in the Update() call
  template <class SpeechRecognizerType>
  bool UpdateTriggerForLocale(TriggerContext<SpeechRecognizerType>& trigger,
                              const Util::Locale newLocale,
                              const MicData::MicTriggerConfig::ModelType modelType,
                              const int searchFileIndex);
  
  // This should only be called from the Update() methods, needs to be performed on the same thread
  void ApplyLocaleUpdate();
  
  template <class SpeechRecognizerType>
  void ApplySpeechRecognizerLoacleUpdate(TriggerContext<SpeechRecognizerType>& aTrigger);
  
  bool UpdateRecognizerModel(TriggerContext<SpeechRecognizerTHF>& aTrigger);
  bool UpdateRecognizerModel(TriggerContext<SpeechRecognizerPryonLite>& aTrigger);
  
  // Console Var methods
  // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
  // NOTE: These methods provide no functionality when ANKI_DEV_CHEATS is turned off
  void SetupConsoleFuncs();
  
  template <class SpeechRecognizerType>
  std::string UpdateRecognizerHelper(size_t& inOut_modelIdx, size_t new_modelIdx,
                                     int& inOut_searchIdx, int new_searchIdx,
                                     const TriggerModelTypeData modelTypeDataList[],
                                     TriggerContext<SpeechRecognizerType>& trigger);
};


} // end namespace Vector
} // end namespace Anki

#endif // __AnimProcess_VictorAnim_SpeechRecognizerSystem_H_
