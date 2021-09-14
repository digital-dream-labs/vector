/**
 * File: textToSpeechProvider_vicos.h
 *
 * Description: Implementation-specific wrapper to generate audio data from a given string.
 * This class insulates engine and audio code from details of text-to-speech implementation.
 *
 * Copyright: Anki, Inc. 2017
 *
 */


#ifndef __Anki_cozmo_cozmoAnim_textToSpeech_textToSpeechProvider_vicos_H__
#define __Anki_cozmo_cozmoAnim_textToSpeech_textToSpeechProvider_vicos_H__

#include "util/helpers/ankiDefines.h"

#if defined(ANKI_PLATFORM_VICOS)

#include "textToSpeechProvider.h"
#include "textToSpeechProviderConfig.h"
#include "json/json.h"

#include <string>

// Acapela SDK declarations
#include "i_babile.h"

// Forward declarations
namespace Anki {
  namespace Util {
    class RandomGenerator;
  }
}

namespace Anki {
namespace Vector {
namespace TextToSpeech {

//
// TextToSpeechProviderImpl: Platform-specific implementation of text-to-speech provider
//

class TextToSpeechProviderImpl
{
public:
  TextToSpeechProviderImpl(const Anim::AnimContext* context, const Json::Value& tts_platform_config);
  ~TextToSpeechProviderImpl();

  Result SetLocale(const std::string & locale);

  // Initialize TTS utterance and get first chunk of TTS audio.
  // Returns RESULT_OK on success, else error code.
  // Sets done to true when audio generation is complete.
  Result GetFirstAudioData(const std::string & text,
                           float durationScalar,
                           float pitchScalar,
                           TextToSpeechProviderData & data,
                           bool & done);

  // Get next chunk of TTS audio.
  // Returns RESULT_OK on success, else error code.
  // Sets done to true when audio generation is complete.
  Result GetNextAudioData(TextToSpeechProviderData & data, bool & done);

private:
  // Path to TTS resources
  std::string _tts_resource_path;

  // Configuration options provided to constructor
  Json::Value _tts_platform_config;

  // RNG provided to constructor
  Anki::Util::RandomGenerator * _rng = nullptr;

 // Current locale, current language
  std::string _locale;
  std::string _language;

  // Current configuration options
  std::unique_ptr<TextToSpeechProviderConfig> _tts_config;

  //
  // BABILE Object State
  //
  // These structs must stay in memory while SDK is in use.
  // They will be allocated by class constructor and
  // freed by class destructor.
  //
  BB_DbLs * _BAB_LangDba = nullptr;
  BB_MemRec * _BAB_MemRec = nullptr;
  BABILE_MemParam * _BAB_MemParam = nullptr;
  BABILE_Obj * _BAB_Obj = nullptr;

  //
  // BABILE Voice State
  //
  // Stash some parameters describing current voice.
  // These are fetched when voice is loaded, then used
  // when generating speech.
  //
  BB_S32 _BAB_voicefreq = 0;
  BB_S32 _BAB_samplesize = 0;

  // State of current utterance
  std::string _str;
  size_t _strlen = 0;
  size_t _strpos = 0;
  bool _draining = false;

  //
  // Internal state management
  //
  Result Initialize(const std::string & locale);
  void Cleanup();

}; // class TextToSpeechProviderImpl

} // end namespace TextToSpeech
} // end namespace Vector
} // end namespace Anki

#endif // ANKI_PLATFORM_VICOS

#endif //__Anki_cozmo_cozmoAnim_textToSpeech_textToSpeechProvider_vicos_H__
