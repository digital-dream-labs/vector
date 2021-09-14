/**
 * File: textToSpeechProviderConfig.cpp
 *
 * Description:
 *
 * Copyright: Anki, Inc. 2018
 *
 */

#include "textToSpeechProviderConfig.h"

#include "coretech/common/engine/jsonTools.h"

#include "util/console/consoleInterface.h"
#include "util/helpers/ankiDefines.h"
#include "util/logging/logging.h"
#include "util/random/randomGenerator.h"

// Log options
#define LOG_CHANNEL "TextToSpeech"

//
// Programmatic defaults. These values are used when unless overridden by configuration.
//
#define TTS_DEFAULT_LANGUAGE    "en"
#define TTS_DEFAULT_VOICE       "Ryan22k_CO"
#define TTS_DEFAULT_SPEED       100
#define TTS_DEFAULT_SHAPING     100
#define TTS_DEFAULT_PITCH       100
#define TTS_PAUSEPUNCTUATION_MS 1000
#define TTS_PAUSESEMICOLON_MS   500
#define TTS_PAUSECOMMA_MS       250
#define TTS_PAUSEBRACKET_MS     100
#define TTS_PAUSESPELLING_MS    100
#define TTS_ENABLEPAUSEPARAMS   true

//
// Platform-specific defaults
#ifdef ANKI_PLATFORM_OSX
#define TTS_LEADINGSILENCE_MS   50
#define TTS_TRAILINGSILENCE_MS  50
#else
#define TTS_LEADINGSILENCE_MS   10
#define TTS_TRAILINGSILENCE_MS  10
#endif

// Configuration keys
#define TTS_VOICE_KEY   "voice"
#define TTS_SPEED_KEY   "speed"
#define TTS_SHAPING_KEY "shaping"
#define TTS_PITCH_KEY   "pitch"

#define TTS_SPEEDTRAITS_KEY   "speedTraits"
#define TTS_TEXTLENGTHMIN_KEY "textLengthMin"
#define TTS_TEXTLENGTHMAX_KEY "textLengthMax"
#define TTS_RANGEMIN_KEY      "rangeMin"
#define TTS_RANGEMAX_KEY      "rangeMax"

// Console variables
#define CONSOLE_GROUP "TextToSpeech"

#if REMOTE_CONSOLE_ENABLED

namespace {
  CONSOLE_VAR_RANGED(s32, kVoiceSpeed, CONSOLE_GROUP, 100, 30, 300);
  CONSOLE_VAR_RANGED(s32, kVoiceShaping, CONSOLE_GROUP, 100, 70, 140);
  CONSOLE_VAR_RANGED(s32, kVoicePitch, CONSOLE_GROUP, 100, 70, 160);
  CONSOLE_VAR_RANGED(u32, kLeadingSilence_ms, CONSOLE_GROUP, TTS_LEADINGSILENCE_MS, 0, 5000);
  CONSOLE_VAR_RANGED(u32, kTrailingSilence_ms, CONSOLE_GROUP, TTS_TRAILINGSILENCE_MS, 0, 5000);
  CONSOLE_VAR_RANGED(u32, kPausePunctuation_ms, CONSOLE_GROUP, TTS_PAUSEPUNCTUATION_MS, 50, 4000);
  CONSOLE_VAR_RANGED(u32, kPauseSemicolon_ms, CONSOLE_GROUP, TTS_PAUSESEMICOLON_MS, 50, 4000);
  CONSOLE_VAR_RANGED(u32, kPauseComma_ms, CONSOLE_GROUP, TTS_PAUSECOMMA_MS, 50, 4000);
  CONSOLE_VAR_RANGED(u32, kPauseBracket_ms, CONSOLE_GROUP, TTS_PAUSEBRACKET_MS, 50, 4000);
  CONSOLE_VAR_RANGED(u32, kPauseSpelling_ms, CONSOLE_GROUP, TTS_PAUSESPELLING_MS, 50, 4000);
  CONSOLE_VAR(bool, kEnablePausePrams, CONSOLE_GROUP, TTS_ENABLEPAUSEPARAMS);
}

#endif

namespace Anki {
namespace Vector {
namespace TextToSpeech {

TextToSpeechProviderConfig::TextToSpeechProviderConfig(const std::string & language,
                                                       const Json::Value& platform_config)
{
  // Initialize with programmatic defaults
  _tts_language = TTS_DEFAULT_LANGUAGE;
  _tts_voice = TTS_DEFAULT_VOICE;
  _tts_speed = TTS_DEFAULT_SPEED;
  _tts_shaping = TTS_DEFAULT_SHAPING;
  _tts_pitch = TTS_DEFAULT_PITCH;

  // Allow language configuration to override programmatic defaults
  const auto & language_config = platform_config[language];
  if (!language_config.isNull()) {
    _tts_language = language;
    JsonTools::GetValueOptional(language_config, TTS_VOICE_KEY, _tts_voice);
    JsonTools::GetValueOptional(language_config, TTS_SPEED_KEY, _tts_speed);
    JsonTools::GetValueOptional(language_config, TTS_SHAPING_KEY, _tts_shaping);
    JsonTools::GetValueOptional(language_config, TTS_PITCH_KEY, _tts_pitch);
  }

  // Allow config traits to override language configuration
  const auto & speedTraits = language_config[TTS_SPEEDTRAITS_KEY];
  if (speedTraits.isArray()) {
    for (int i = 0; speedTraits[i].isObject(); ++i) {
      _speedTraits.emplace_back(ConfigTrait(speedTraits[i]));
    }
  }

  // Initialize sliders to base values for this language
  #if REMOTE_CONSOLE_ENABLED
  kVoiceSpeed = _tts_speed;
  kVoiceShaping = _tts_shaping;
  kVoicePitch = _tts_pitch;
  #endif

}

int TextToSpeechProviderConfig::GetSpeed() const
{
#if REMOTE_CONSOLE_ENABLED
  return kVoiceSpeed;
#else
  return _tts_speed;
#endif
}

int TextToSpeechProviderConfig::GetShaping() const
{
#if REMOTE_CONSOLE_ENABLED
  return kVoiceShaping;
#else
  return _tts_shaping;
#endif
}

int TextToSpeechProviderConfig::GetPitch() const
{
#if REMOTE_CONSOLE_ENABLED
  return kVoicePitch;
#else
  return _tts_pitch;
#endif
}

int TextToSpeechProviderConfig::GetLeadingSilence_ms() const
{
#if REMOTE_CONSOLE_ENABLED
  return kLeadingSilence_ms;
#else
  return TTS_LEADINGSILENCE_MS;
#endif
}

int TextToSpeechProviderConfig::GetTrailingSilence_ms() const
{
#if REMOTE_CONSOLE_ENABLED
  return kTrailingSilence_ms;
#else
  return TTS_TRAILINGSILENCE_MS;
#endif
}

int TextToSpeechProviderConfig::GetPausePunctuation_ms() const
{
#if REMOTE_CONSOLE_ENABLED
  return kPausePunctuation_ms;
#else
  return TTS_PAUSEPUNCTUATION_MS;
#endif
}

int TextToSpeechProviderConfig::GetPauseSemicolon_ms() const
{
#if REMOTE_CONSOLE_ENABLED
  return kPauseSemicolon_ms;
#else
  return TTS_PAUSESEMICOLON_MS;
#endif
}

int TextToSpeechProviderConfig::GetPauseComma_ms() const
{
#if REMOTE_CONSOLE_ENABLED
  return kPauseComma_ms;
#else
  return TTS_PAUSECOMMA_MS;
#endif
}

int TextToSpeechProviderConfig::GetPauseBracket_ms() const
{
#if REMOTE_CONSOLE_ENABLED
  return kPauseBracket_ms;
#else
  return TTS_PAUSEBRACKET_MS;
#endif
}

int TextToSpeechProviderConfig::GetPauseSpelling_ms() const
{
#if REMOTE_CONSOLE_ENABLED
  return kPauseSpelling_ms;
#else
  return TTS_PAUSESPELLING_MS;
#endif
}

bool TextToSpeechProviderConfig::GetEnablePauseParams() const
{
#if REMOTE_CONSOLE_ENABLED
  return kEnablePausePrams;
#else
  return TTS_ENABLEPAUSEPARAMS;
#endif
}

int TextToSpeechProviderConfig::GetSpeed(Anki::Util::RandomGenerator * rng, size_t textLength) const
{
  // RNG may not be null
  DEV_ASSERT(rng != nullptr, "TextToSpeechProviderConfig.GetSpeed.InvalidRNG");

  // Start with base speed, possibly modified by console var
  int speed = GetSpeed();

  // Look for matching trait
  for (const auto & trait : _speedTraits) {
    if (trait.textLengthMin <= textLength && textLength <= trait.textLengthMax) {
      // Note that matching trait will override console var.
      speed = rng->RandIntInRange(trait.rangeMin, trait.rangeMax);
      break;
    }
  }

  return speed;
}

TextToSpeechProviderConfig::ConfigTrait::ConfigTrait(const Json::Value & json)
{
  JsonTools::GetValueOptional(json, TTS_TEXTLENGTHMIN_KEY, textLengthMin);
  JsonTools::GetValueOptional(json, TTS_TEXTLENGTHMAX_KEY, textLengthMax);
  JsonTools::GetValueOptional(json, TTS_RANGEMIN_KEY, rangeMin);
  JsonTools::GetValueOptional(json, TTS_RANGEMAX_KEY, rangeMax);
}

} // end namespace TextToSpeech
} // end namespace Vector
} // end namespace Anki
