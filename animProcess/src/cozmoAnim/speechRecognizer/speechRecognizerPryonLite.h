/**
* File: speechRecognizerPryonLite.h
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

#ifndef __Victor_VoiceCommands_SpeechRecognizerPryonLite_H__
#define __Victor_VoiceCommands_SpeechRecognizerPryonLite_H__

#include "audioUtil/speechRecognizer.h"
#include <memory>
#include <string>

// Forward declare Pryon Lite types
typedef void* PryonLiteDecoderHandle;
struct PryonLiteResult;
struct PryonLiteVadEvent;

namespace Anki {
namespace Vector {


class SpeechRecognizerPryonLite : public AudioUtil::SpeechRecognizer
{
public:
  
  SpeechRecognizerPryonLite();
  virtual ~SpeechRecognizerPryonLite();
  SpeechRecognizerPryonLite(SpeechRecognizerPryonLite&& other);
  SpeechRecognizerPryonLite& operator=(SpeechRecognizerPryonLite&& other);
  SpeechRecognizerPryonLite(const SpeechRecognizerPryonLite& other) = delete;
  SpeechRecognizerPryonLite& operator=(const SpeechRecognizerPryonLite& other) = delete;
  
  // Create recognizer with model file
  // Can recall method to change model
  // Note: When changing the model file the recognizer will be destroyed and re-created.
  bool InitRecognizer(const std::string& modelFilePath, bool useVad);
  
  // Stream audio data to recognizer
  // Note: Stream all data to recognizer to keep stream's time in sync with Alexa component, there is an internal VAD
  // to improve recognizer performance. When SpeechRecognizer is dissabled silence is streamed into recognizer.
  virtual void Update(const AudioUtil::AudioSample * audioData, unsigned int audioDataLen) override;
  
  // Set detection threshold for all keywords (this function can be called any time after decoder initialization)
  // Valid values 1-1000, 1 = lowest threshold, most detections, 1000 = highest threshold, fewest detections.
  bool SetDetectionThreshold(int threshold);
  
  // Get the state of the internal VAD
  // Return true when voice is detected
  bool IsVadActive() const;
  
  // Get state of recognizer
  // Return true when recognizer is initialized and model is loaded
  bool IsReady() const;
  
  void SetAlexaMicrophoneOffset(uint64_t offset) { _alexaMicrophoneOffset = offset; }
  
  // Pryon doesn't use recognizer indexs
  virtual void SetRecognizerIndex(IndexType index) override {}
  virtual void SetRecognizerFollowupIndex(IndexType index) override {}
  virtual IndexType GetRecognizerIndex() const override { return 0; }


private:

  struct SpeechRecognizerPryonLiteData;
  std::unique_ptr<SpeechRecognizerPryonLiteData> _impl;
  uint64_t _alexaMicrophoneOffset = 0;
  
  unsigned int _detectThreshold;
  
  static bool LoadPryonModel(const std::string& filePath, SpeechRecognizerPryonLiteData& data);
  
  // Pryon recognizer callbacks
  static void DetectionCallback(PryonLiteDecoderHandle handle, const PryonLiteResult* result);
  static void VadCallback(PryonLiteDecoderHandle handle, const PryonLiteVadEvent* vadEvent);
  
  void SwapAllData(SpeechRecognizerPryonLite& other);
  
  void HandleInitFail(const std::string& failMessage);
  void Cleanup();
  
  virtual void StartInternal() override;
  virtual void StopInternal() override;

}; // class SpeechRecognizer

} // end namespace Vector
} // end namespace Anki

#endif // __Victor_VoiceCommands_SpeechRecognizerPryonLite_H__
