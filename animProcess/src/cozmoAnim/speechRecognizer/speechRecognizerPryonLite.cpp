/**
* File: speechRecognizerPryonLite.cpp
*
* Author: Jordan Rivas
* Created: 12/27/2018
*
* Description: SpeechRecognizer implementation for Amazon's Pryon Lite. The cpp defines the impl struct that is only
* declared in the header, in order to encapsulate accessing outside headers to only be in this file.
*
* Copyright: Anki, Inc. 2018
*
*/

#include "speechRecognizerPryonLite.h"
#include "util/console/consoleInterface.h"
#include "util/helpers/ankiDefines.h"
#include "util/helpers/templateHelpers.h"
#include "util/logging/logging.h"
#include "util/math/math.h"
#include "util/math/numericCast.h"
#include "util/string/stringUtils.h"

#include <algorithm>
#include <array>
#include <fstream>
#include <locale>
#include <map>
#include <mutex>
#include <string>
#include <sstream>

// TODO: Waiting for Amazon to provide libs for Mac platform, until then compile out recognizer for other platforms
#if defined(ANKI_PLATFORM_VICOS)
#define PRYON_ENABLED 1
#else
#define PRYON_ENABLED 0
#endif

#if PRYON_ENABLED
#include "pryon_lite.h"
#endif

namespace Anki {
namespace Vector {

namespace {
  // must be saved + reboot
  CONSOLE_VAR_RANGED(int, kDefaultDetectThreshold, "SpeechRecognizer.Alexa", 250, 0, 1000);
  
  #define LOG_CHANNEL "SpeechRecognizer"
}

// Local definition of data used internally for more strict encapsulation
struct SpeechRecognizerPryonLite::SpeechRecognizerPryonLiteData
{
  #define ALIGN(n) __attribute__((aligned(n)))
  #if PRYON_ENABLED
  PryonLiteDecoderHandle            decoder = nullptr;
  PryonLiteDecoderConfig            config = PryonLiteDecoderConfig_Default;
  mutable std::recursive_mutex      recogMutex;
  ALIGN(4) const char*              modelBuffer = nullptr;
  bool                              disabled = false;
  bool                              ready = false;
  PryonLiteVadState                 vadState = PryonLiteVadState::PRYON_LITE_VAD_INACTIVE;
  #endif
};
  
SpeechRecognizerPryonLite::SpeechRecognizerPryonLite()
: _impl(new SpeechRecognizerPryonLiteData())
, _detectThreshold(kDefaultDetectThreshold)
{
}

SpeechRecognizerPryonLite::~SpeechRecognizerPryonLite()
{
  Cleanup();
}

SpeechRecognizerPryonLite::SpeechRecognizerPryonLite(SpeechRecognizerPryonLite&& other)
: SpeechRecognizer(std::move(other))
{
  SwapAllData(other);
}

SpeechRecognizerPryonLite& SpeechRecognizerPryonLite::operator=(SpeechRecognizerPryonLite&& other)
{
  SpeechRecognizer::operator=(std::move(other));
  SwapAllData(other);
  return *this;
}

bool SpeechRecognizerPryonLite::InitRecognizer(const std::string& modlePath, bool useVad)
{
#if PRYON_ENABLED
  // Destroy current recognizer before loading new model
  Cleanup();
  
  bool success = LoadPryonModel(modlePath, *_impl.get());
  if (!success) {
    return false;
  }
    
  // Query for the size of instance memory required by the decoder
  PryonLiteModelAttributes modelAttributes;
  PryonLiteError err = PryonLite_GetModelAttributes(_impl->config.model, _impl->config.sizeofModel, &modelAttributes);
  if (err != PRYON_LITE_ERROR_OK) {
    LOG_ERROR("SpeechRecognizerPryonLite.Init.PryonLite_GetModelAttributes", "PryonLite error %d", err);
    Cleanup();
    return false;
  }
  
  // Setup decoder
  PryonLiteSessionInfo sessionInfo;
  _impl->config.decoderMem        = new char[modelAttributes.requiredDecoderMem];
  _impl->config.sizeofDecoderMem  = modelAttributes.requiredDecoderMem;
  _impl->config.resultCallback    = SpeechRecognizerPryonLite::DetectionCallback;
  _impl->config.userData          = this;
  _impl->config.detectThreshold   = _detectThreshold;
  _impl->config.useVad            = useVad;
  if (useVad) {
    _impl->config.vadCallback     = SpeechRecognizerPryonLite::VadCallback;
  }
  
  err = PryonLiteDecoder_Initialize(&_impl->config, &sessionInfo, &_impl->decoder);
  if (err != PRYON_LITE_ERROR_OK) {
    LOG_ERROR("SpeechRecognizerPryonLite.Init.PryonLiteDecoder_Initialize", "PryonLite error %d", err);
    Cleanup();
    return false;
  }
  
  success = SetDetectionThreshold(_detectThreshold);
  if (!success) {
    LOG_ERROR("SpeechRecognizerPryonLite.Init.SetDetectionThreshold", "PryonLite error %d", err);
    Cleanup();
    return false;
  }
  _impl->ready = true;
  return true;
#else
  return false;
#endif

}

void SpeechRecognizerPryonLite::Update(const AudioUtil::AudioSample* audioData, unsigned int audioDataLen)
{
#if PRYON_ENABLED
  // Silence Audio data
  static const AudioUtil::AudioSample silenceData[160] = { 0 };
  
  const AudioUtil::AudioSample* data = _impl->disabled ? silenceData : audioData;
  // pass samples to decoder
  PryonLiteError err = PryonLiteDecoder_PushAudioSamples(_impl->decoder, data, audioDataLen);
  if (err != PRYON_LITE_ERROR_OK) {
    LOG_ERROR("SpeechRecognizerPryonLite.Update.PryonLiteDecoder_PushAudioSamples", "PryonLite error %d", err);
  }
#endif
}

bool SpeechRecognizerPryonLite::SetDetectionThreshold(int threshold)
{
  _detectThreshold = Util::Clamp(threshold, 1, 1000);
#if PRYON_ENABLED
  std::lock_guard<std::recursive_mutex> lock(_impl->recogMutex);
  if (!PryonLiteDecoder_IsDecoderInitialized(_impl->decoder)) {
    LOG_INFO("SpeechRecognizerPryonLite.SetDetectionThreshold.NotInitialized", "Detect threshold will be set on init");
    return true;
  }
  _impl->config.detectThreshold = _detectThreshold;
  const PryonLiteError err = PryonLiteDecoder_SetDetectionThreshold(_impl->decoder, "ALEXA", _impl->config.detectThreshold);
  if (err != PRYON_LITE_ERROR_OK) {
    LOG_ERROR("SpeechRecognizerPryonLite.SetDetectionThreshold", "PryonLite error %d", err);
    return false;
  }
  return true;
#endif
  return false;
}

bool SpeechRecognizerPryonLite::IsVadActive() const
{
#if PRYON_ENABLED
  std::lock_guard<std::recursive_mutex> lock(_impl->recogMutex);
  return (_impl->vadState == PryonLiteVadState::PRYON_LITE_VAD_ACTIVE);
#endif
  return false;
}

bool SpeechRecognizerPryonLite::IsReady() const
{
#if PRYON_ENABLED
  std::lock_guard<std::recursive_mutex> lock(_impl->recogMutex);
  return _impl->ready;
#elif defined(ANKI_PLATFORM_OSX)
  // since we keep the wake word engine samples coupled to those samples passed to alexa's mic input,
  // pretend this wake word engine is initialized on mac so that alexa receives any mic input at all
  return true;
#else
  return false;
#endif
}

bool SpeechRecognizerPryonLite::LoadPryonModel(const std::string& filePath, SpeechRecognizerPryonLiteData& data)
{
#if PRYON_ENABLED
  if (data.config.model != nullptr || data.config.sizeofModel > 0) {
    LOG_ERROR("SpeechRecognizerPryonLite.LoadPryonModel", "Model already loaded");
    return false;
  }
  
  bool success = false;
  std::ifstream fileStream(filePath, std::ifstream::binary);
  if (fileStream) {
    // Get the size of the file and load
    size_t length = 0;
    fileStream.seekg(0, fileStream.end);
    length = static_cast<size_t>( fileStream.tellg() );
    fileStream.seekg(0, fileStream.beg);
    char* buffer = new char[length]();
    fileStream.read(buffer, length);
    
    if (fileStream) {
      data.modelBuffer = buffer;
      data.config.model = static_cast<const char*>( data.modelBuffer );
      data.config.sizeofModel = length;
      success = true;
    }
    else {
      LOG_ERROR("SpeechRecognizerPryonLite.LoadPryonModel.ReadFile", "Error read %td of %zu bytes",
                fileStream.gcount(), length);
      Util::SafeDeleteArray(buffer);
    }
    fileStream.close();
  }
  return success;
#else
  return false;
#endif
}

void SpeechRecognizerPryonLite::DetectionCallback(PryonLiteDecoderHandle handle, const PryonLiteResult* result)
{
#if PRYON_ENABLED
  const auto* recContext = static_cast<SpeechRecognizerPryonLite*>(result->userData);
  if (recContext->_impl->disabled) {
    return;
  }
  
  const auto beginSampleIdx = result->beginSampleIndex + recContext->_alexaMicrophoneOffset;
  const auto endSampleIdx   = result->endSampleIndex + recContext->_alexaMicrophoneOffset;
  
  const AudioUtil::SpeechRecognizerCallbackInfo info {
    .result           = (result->keyword ? result->keyword : ""),
    .startTime_ms     = static_cast<int>( beginSampleIdx / 16 ), // 16 samples per ms
    .endTime_ms       = static_cast<int>( endSampleIdx / 16 ),
    .startSampleIndex = static_cast<unsigned long long>( beginSampleIdx ),
    .endSampleIndex   = static_cast<unsigned long long>( endSampleIdx ),
    .score            = static_cast<float>( result->confidence )
  };
  
  recContext->DoCallback(info);
#endif
}

void SpeechRecognizerPryonLite::VadCallback(PryonLiteDecoderHandle handle, const PryonLiteVadEvent* vadEvent)
{
#if PRYON_ENABLED
  auto* recContext = static_cast<SpeechRecognizerPryonLite*>(vadEvent->userData);
  recContext->_impl->vadState = vadEvent->vadState;
#endif
}

void SpeechRecognizerPryonLite::SwapAllData(SpeechRecognizerPryonLite& other)
{
  auto temp = std::move(other._impl);
  other._impl = std::move(this->_impl);
  this->_impl = std::move(temp);
}

void SpeechRecognizerPryonLite::HandleInitFail(const std::string& failMessage)
{
  PRINT_NAMED_ERROR("SpeechRecognizerPryonLite.Init.Fail", "%s", failMessage.c_str());
  Cleanup();
}

void SpeechRecognizerPryonLite::Cleanup()
{
#if PRYON_ENABLED
  std::lock_guard<std::recursive_mutex> lock(_impl->recogMutex);
  _impl->ready = false;
  if (_impl->decoder) {
    PryonLiteError err = PryonLiteDecoder_Destroy(&_impl->decoder);
    if (err != PRYON_LITE_ERROR_OK) {
      LOG_ERROR("SpeechRecognizerPryonLite.Cleanup", "PryonLite error %d", err);
    }
  }
  // Unload model
  _impl->config.model = nullptr;
  Util::SafeDeleteArray(_impl->modelBuffer);
  _impl->config.sizeofModel = 0;
  _impl->decoder = nullptr;
  _impl->config = PryonLiteDecoderConfig_Default;
  _impl->vadState = PryonLiteVadState::PRYON_LITE_VAD_INACTIVE;
  Util::SafeDeleteArray(_impl->config.decoderMem);
#endif
}

void SpeechRecognizerPryonLite::StartInternal()
{
#if PRYON_ENABLED
  _impl->disabled = false;
#endif
}

void SpeechRecognizerPryonLite::StopInternal()
{
#if PRYON_ENABLED
  _impl->disabled = true;
#endif
}


} // end namespace Vector
} // end namespace Anki
