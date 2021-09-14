/**
 * File: textToSpeechProvider_mac.cpp
 *
 * Description: Implementation-specific details of text-to-speech conversion
 *
 * Copyright: Anki, Inc. 2017
 *
 */

#include "util/helpers/ankiDefines.h"

#if defined(ANKI_PLATFORM_OSX)

#include "textToSpeechProvider_acapela.h"
#include "textToSpeechProvider_mac.h"

#include "cozmoAnim/animContext.h"

#include "coretech/common/engine/jsonTools.h"
#include "coretech/common/engine/utils/data/dataPlatform.h"

#include "util/environment/locale.h"
#include "util/logging/logging.h"
#include "util/math/math.h"
#include "util/math/numericCast.h"

#include <cmath>

// Log options
#define LOG_CHANNEL    "TextToSpeech"

// Acapela declarations
#include "ioBabTts.h"

// Acapela linkage magic
#define _BABTTSDYN_IMPL_
#include "ifBabTtsDyn.h"
#undef _BABTTSDYN_IMPL_

/* Maximum length of Acapela voice name */
#define ACAPELA_VOICE_BUFSIZ 50

/* How many samples do we fetch in one call? */
#define ACAPELA_SAMPLE_BUFSIZ (16*1024)


namespace Anki {
namespace Vector {
namespace TextToSpeech {


TextToSpeechProviderImpl::TextToSpeechProviderImpl(const Anim::AnimContext* ctx,
                                                   const Json::Value& tts_platform_config)
{
  using Locale = Anki::Util::Locale;
  using DataPlatform = Anki::Util::Data::DataPlatform;

  // Check for valid data platform before we do any work
  const DataPlatform * dataPlatform = ctx->GetDataPlatform();
  if (nullptr == dataPlatform) {
    // This may happen during unit tests
    LOG_WARNING("TextToSpeechProvider.Initialize.NoDataPlatform",
                "Unable to initialize TTS provider");
    return;
  }

  // Check for valid locale before we do any work
  const Locale * locale = ctx->GetLocale();
  if (nullptr == locale) {
    // This may happen during unit tests
    LOG_WARNING("TextToSpeechProvider.Initialize.NoLocale",
                "Unable to initialize TTS provider");
    return;
  }

  _tts_resource_path = dataPlatform->GetResourcePath("tts");
  _tts_platform_config = tts_platform_config;
  _rng = ctx->GetRandom();

  // Initialize with current locale
  const std::string & localeString = locale->GetLocaleString();
  const Result result = Initialize(localeString);
  if (result != RESULT_OK) {
    LOG_WARNING("TextToSpeechProvider.Initialize",
                "Unable to initialize locale %s (error %d)",
                localeString.c_str(), result);
    return;
  }

}

TextToSpeechProviderImpl::~TextToSpeechProviderImpl()
{
  Cleanup();
}

Result TextToSpeechProviderImpl::Initialize(const std::string & locale)
{
  LOG_DEBUG("TextToSpeechProvider.Initialize", "Initializing locale %s", locale.c_str());

  if (locale == _locale) {
    LOG_DEBUG("TextToSpeechProvider.Initialize", "Already using locale %s", locale.c_str());
    return RESULT_OK;
  }

  // Release resources, if any
  Cleanup();

  std::string language = Anki::Util::Locale::LocaleFromString(locale).GetLanguageString();
  if (language.empty()) {
    LOG_ERROR("TextToSpeechProvider.Initialize", "Unable to get language from locale %s", locale.c_str());
    language = "en";
  }

  // Set up default parameters for requested language
  _tts_config = std::make_unique<TextToSpeechProviderConfig>(language, _tts_platform_config);

  const auto & voice = _tts_config->GetVoice();
  const auto speed = _tts_config->GetSpeed();
  const auto shaping = _tts_config->GetShaping();
  const auto pitch = _tts_config->GetPitch();
  const auto leadingSilence_ms = _tts_config->GetLeadingSilence_ms();
  const auto trailingSilence_ms = _tts_config->GetTrailingSilence_ms();

  LOG_INFO("TextToSpeechProviderImpl.Initialize",
           "language=%s voice=%s speed=%d shaping=%d pitch=%d",
           language.c_str(), voice.c_str(), speed, shaping, pitch);

  // Initialize Acapela DLL
  HMODULE h = BabTtsInitDllEx(_tts_resource_path.c_str());
  if (nullptr == h) {
    LOG_WARNING("TextToSpeechProvider.Initialize.InitDll",
              "Unable to initialize TTS provider DLL in '%s'",
              _tts_resource_path.c_str());
    return RESULT_FAIL_INVALID_PARAMETER;
  }

  bool ok = BabTTS_Init();
  if (!ok) {
    LOG_ERROR("TextToSpeechProvider.Initialize.Init",
              "Unable to initialize TTS provider");
    return RESULT_FAIL_INVALID_OBJECT;
  }

  _lpBabTTS = BabTTS_Create();
  if (nullptr == _lpBabTTS) {
    LOG_ERROR("TextToSpeechProvider.Initialize.Create",
              "Unable to create TTS provider handle");
    return RESULT_FAIL_INVALID_OBJECT;
  }

  BabTtsError err = BabTTS_Open(_lpBabTTS, voice.c_str(), BABTTS_USEDEFDICT);
  if (E_BABTTS_NOERROR == err) {
    /* licensed install */
    _tts_licensed = true;
  } else if (E_BABTTS_NOTVALIDLICENSE == err) {
    /* unlicensed install */
    LOG_WARNING("TextToSpeechProvider.Initialize.Open",
                "Unable to open TTS voice (%s)",
                BabTTS_GetErrorName(err));
    return RESULT_FAIL_INVALID_PARAMETER;
  } else {
    /* some other error */
    LOG_ERROR("TextToSpeechProvider.Initialize.Open",
              "Unable to open TTS voice (%s)",
              BabTTS_GetErrorName(err));
    return RESULT_FAIL_INVALID_PARAMETER;
  }

  err = BabTTS_SetSettings(_lpBabTTS, BABTTS_PARAM_SPEED, speed);
  if (E_BABTTS_NOERROR != err) {
    LOG_ERROR("TextToSpeechProvider.Initialize.SetSpeed",
              "Unable to set speed=%d (%s)",
              speed, BabTTS_GetErrorName(err));
  }

  err = BabTTS_SetSettings(_lpBabTTS, BABTTS_PARAM_VOCALTRACT, shaping);
  if (E_BABTTS_NOERROR != err) {
    LOG_ERROR("TextToSpeechProvider.Initialize.SetShaping",
              "Unable to set shaping=%d (%s)",
              shaping, BabTTS_GetErrorName(err));
  }

  err = BabTTS_SetSettings(_lpBabTTS, BABTTS_PARAM_PITCH, pitch);
  if (E_BABTTS_NOERROR != err) {
    LOG_ERROR("TextToSpeechProvider.Initialize.SetPitch",
              "Unable to set pitch=%d (%s)",
              pitch, BabTTS_GetErrorName(err));
  }

  err = BabTTS_SetSettings(_lpBabTTS, BABTTS_PARAM_LEADINGSILENCE, leadingSilence_ms);
  if (E_BABTTS_NOERROR != err) {
    LOG_ERROR("TextToSpeechProvider.Initialize.SetLeadingSilence",
              "Unable to set leading silence=%d (%s)",
              leadingSilence_ms, BabTTS_GetErrorName(err));
  }

  err = BabTTS_SetSettings(_lpBabTTS, BABTTS_PARAM_TRAILINGSILENCE, trailingSilence_ms);
  if (E_BABTTS_NOERROR != err) {
    LOG_ERROR("TextToSpeechProvider.Initialize.SetTrailingSilence",
              "Unable to set trailing silence=%d (%s)",
              trailingSilence_ms, BabTTS_GetErrorName(err));
  }

  _locale = locale;
  _language = language;

  LOG_DEBUG("TextToSpeechProvider.Initialize", "Now using locale %s language %s", _locale.c_str(), _language.c_str());

  return RESULT_OK;
}

void TextToSpeechProviderImpl::Cleanup()
{
  if (nullptr != _lpBabTTS) {
    BabTTS_Close(_lpBabTTS);
    BabTTS_Uninit();
    BabTtsUninitDll();
    _lpBabTTS = nullptr;
  }
  _locale.clear();
  _language.clear();
}

Result TextToSpeechProviderImpl::SetLocale(const std::string & locale)
{
  return Initialize(locale);
}


Result TextToSpeechProviderImpl::GetFirstAudioData(const std::string & text,
                                                   float durationScalar,
                                                   float pitchScalar,
                                                   TextToSpeechProviderData & data,
                                                   bool & done)
{
  if (nullptr == _lpBabTTS) {
    /* Log an error, return an error */
    LOG_ERROR("TextToSpeechProvider.GetFirstAudioData.NoProvider",
              "No provider handle");
    return RESULT_FAIL_INVALID_OBJECT;
  }

  if (!_tts_licensed) {
    /* Log a warning, return dummy data */
    LOG_WARNING("TextToSpeechProvider.GetFirstAudioData.NoLicense",
                "No license to generate speech");
    const int sampleRate = AcapelaTTS::GetSampleRate();
    const int numChannels = AcapelaTTS::GetNumChannels();
    data.Init(sampleRate, numChannels);
    data.AppendSample(0, sampleRate * numChannels);
    return RESULT_OK;
  }

  // TODO: VIC-6894 [Tech Debt] Update Text to Speech Mac provider to be consistent with Vicos

  // Get base speed for this utterance, then adjust by duration scalar
  const auto baseSpeed = _tts_config->GetSpeed(_rng, text.size());
  const auto adjustedSpeed = AcapelaTTS::GetSpeechRate(baseSpeed, durationScalar);
  const auto speed = Anki::Util::numeric_cast<int>(std::round(adjustedSpeed));

  // Get base pitch for this utterance, then adjust by pitch scalar
  const auto basePitch = _tts_config->GetPitch();
  const auto adjustedPitch = AcapelaTTS::GetAdjustedPitch(basePitch, pitchScalar);
  const auto pitch = Anki::Util::numeric_cast<int>(std::round(adjustedPitch));

  // Get shaping
  const int shaping = _tts_config->GetShaping();

  LOG_DEBUG("TextToSpeechProvider.GetFirstAudioData",
            "size=%zu speed=%d shaping=%d pitch=%d",
            text.size(), speed, shaping, pitch);

  // Update TTS engine to use new speed, shaping, pitch
  BabTtsError err = BabTTS_SetSettings(_lpBabTTS, BABTTS_PARAM_SPEED, speed);
  if (E_BABTTS_NOERROR != err) {
    LOG_ERROR("TextToSpeechProvider.GetFirstAudioData.SetSpeed",
              "Unable to set speed %d (%s)", speed, BabTTS_GetErrorName(err));
    return RESULT_FAIL_INVALID_PARAMETER;
  }

  err = BabTTS_SetSettings(_lpBabTTS, BABTTS_PARAM_VOCALTRACT, shaping);
  if (E_BABTTS_NOERROR != err) {
    LOG_ERROR("TextToSpeechProvider.GetFirstAudioData.SetShaping",
              "Unable to set shaping %d (%s)", shaping, BabTTS_GetErrorName(err));
    return RESULT_FAIL_INVALID_PARAMETER;
  }

  err = BabTTS_SetSettings(_lpBabTTS, BABTTS_PARAM_PITCH, pitch);
  if (E_BABTTS_NOERROR != err) {
    LOG_ERROR("TextToSpeechProvider.GetFirstAudioData.SetPitch",
              "Unable to set pitch %d (%s)", pitch, BabTTS_GetErrorName(err));
    return RESULT_FAIL_INVALID_PARAMETER;
  }

  // Start processing text
  DWORD dwTextFlags = BABTTS_TEXT | BABTTS_TXT_UTF8 | BABTTS_READ_DEFAULT | BABTTS_TAG_SAPI;
  err = BabTTS_InsertText(_lpBabTTS, text.c_str(), dwTextFlags);
  if (E_BABTTS_NOERROR != err) {
    LOG_ERROR("TextToSpeechProvider.GetFirstAudioData.InsertText",
              "Unable to insert text (%s)", BabTTS_GetErrorName(err));
    return RESULT_FAIL;
  }

  return GetNextAudioData(data, done);
}

Result TextToSpeechProviderImpl::GetNextAudioData(TextToSpeechProviderData & data, bool & done)
{
  // Initialize output buffer
  data.Init(AcapelaTTS::GetSampleRate(), AcapelaTTS::GetNumChannels());

  AudioUtil::AudioChunk & chunk = data.GetChunk();

  // Poll output buffer until we run out of data
  short buf[ACAPELA_SAMPLE_BUFSIZ] = {0};
  DWORD num_samples = 0;

  const BabTtsError err = BabTTS_ReadBuffer(_lpBabTTS, buf, ACAPELA_SAMPLE_BUFSIZ, &num_samples);

  if (E_BABTTS_NOERROR == err) {
    LOG_DEBUG("TextToSpeechProvider.GetNextAudioData.ReadBuffer",
              "%d new samples", num_samples);
    for (DWORD i = 0; i < num_samples; ++i) {
      chunk.push_back(buf[i]);
    }
    return RESULT_OK;
  }

  if (W_BABTTS_NOMOREDATA == err)  {
    LOG_DEBUG("TextToSpeechProvider.GetNextAudioData.ReadBuffer",
              "%d new samples, no more data", num_samples);
    for (DWORD i = 0; i < num_samples; ++i) {
      chunk.push_back(buf[i]);
    }
    done = true;
    return RESULT_OK;
  }

  LOG_ERROR("TextToSpeechProvider.GetNextAudioData.ReadBuffer",
            "Error %d (%s)", err, BabTTS_GetErrorName(err));
  return RESULT_FAIL;

}

} // end namespace TextToSpeech
} // end namespace Vector
} // end namespace Anki

#endif // ANKI_PLATFORM_OSX
