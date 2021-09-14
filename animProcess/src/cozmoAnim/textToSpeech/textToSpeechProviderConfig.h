/**
 * File: textToSpeechProviderConfig.h
 *
 * Description: Configuration settings common to all TTS providers
 *
 * Copyright: Anki, Inc. 2018
 *
 */


#ifndef __cozmo_textToSpeech_textToSpeechProviderConfig_h
#define __cozmo_textToSpeech_textToSpeechProviderConfig_h

#include <string>
#include <list>

// Forward declarations
namespace Anki {
  namespace Util {
    class RandomGenerator;
  }
}
namespace Json {
  class Value;
}

namespace Anki {
namespace Vector {
namespace TextToSpeech {

class TextToSpeechProviderConfig
{
public:

  TextToSpeechProviderConfig(const std::string & language, const Json::Value& json);

  const std::string & GetLanguage() const { return _tts_language; }
  const std::string & GetVoice() const { return _tts_voice; }

  // Base values, possibly modified by console vars
  int GetSpeed() const;
  int GetShaping() const;
  int GetPitch() const;
  int GetLeadingSilence_ms() const;
  int GetTrailingSilence_ms() const;
  int GetPausePunctuation_ms() const;
  int GetPauseSemicolon_ms() const;
  int GetPauseComma_ms() const;
  int GetPauseBracket_ms() const;
  int GetPauseSpelling_ms() const;
  bool GetEnablePauseParams() const;

  //
  // Get base speed, adjusted for length, possibly modified by configuration traits.
  // Note that configuration traits will override console vars!
  // This allows testing of randomness even when console vars are enabled.
  //
  // RNG may not be null.
  //
  int GetSpeed(Anki::Util::RandomGenerator* rng, size_t textLength) const;

private:

  // Base values
  std::string _tts_language;
  std::string _tts_voice;
  int _tts_speed;
  int _tts_shaping;
  int _tts_pitch;

  // Configurable traits
  struct ConfigTrait {
    ConfigTrait(const Json::Value & json);
    int textLengthMin = 0;
    int textLengthMax = 0;
    int rangeMin = 0;
    int rangeMax = 0;
  };

  std::list<ConfigTrait> _speedTraits;

};

} // end namespace TextToSpeech
} // end namespace Vector
} // end namespace Anki

#endif //__cozmo_textToSpeech_textToSpeechProviderConfig_h
