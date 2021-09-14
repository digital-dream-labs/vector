/**
 * File: textToSpeechProvider.h
 *
 * Description: Implementation-specific wrapper to generate audio data from a given string.
 * This class declares an interface common to all platform-specific implementations.
 * This is done to insulate engine and audio code from details of text-to-speech implementation.
 *
 * Copyright: Anki, Inc. 2017
 *
 */


#ifndef __Anki_cozmo_cozmoAnim_textToSpeech_textToSpeechProvider_H__
#define __Anki_cozmo_cozmoAnim_textToSpeech_textToSpeechProvider_H__

#include "coretech/common/shared/types.h"

#include "audioUtil/audioDataTypes.h"

// Forward declarations
namespace Anki {
  namespace Vector {
    namespace Anim {
      class AnimContext;
    }
    namespace TextToSpeech {
      class TextToSpeechProviderImpl;
    }
  }
}

namespace Json {
  class Value;
}

namespace Anki {
namespace Vector {
namespace TextToSpeech {

//
// TextToSpeechProviderData is used to hold audio returned from TTS provider to engine.
// Audio data is automatically released when the object is destroyed.
//
class TextToSpeechProviderData
{
public:
  using AudioChunk = AudioUtil::AudioChunk;
  using AudioSample = AudioUtil::AudioSample;

  TextToSpeechProviderData() {}
  ~TextToSpeechProviderData() {}

  int GetSampleRate() const { return _sampleRate; }
  int GetNumChannels() const { return _numChannels; }
  size_t GetNumSamples() const { return _chunk.size(); }
  const short * GetSamples() const { return _chunk.data(); }

  const AudioChunk& GetChunk() const { return _chunk; }
  AudioChunk& GetChunk() { return _chunk; }

  void Init(int sampleRate, int numChannels)
  {
    _sampleRate = sampleRate;
    _numChannels = numChannels;
    _chunk.clear();
  }

  void AppendSample(AudioSample sample, size_t numSamples = 1)
  {
    _chunk.insert(_chunk.end(), numSamples, sample);
  }

  void AppendSamples(const AudioSample * samples, size_t numSamples)
  {
    // There's got to be a better way to do this
    const size_t oldSize = _chunk.size();
    _chunk.resize(oldSize + numSamples);
    memcpy(&_chunk[oldSize], samples, numSamples * sizeof(AudioSample));
  }

private:
  int _sampleRate;
  int _numChannels;
  AudioChunk _chunk;
};

//
// TextToSpeechProvider defines a common interface for various platform-specific implementations.
//
class TextToSpeechProvider
{
public:
  TextToSpeechProvider(const Anim::AnimContext* ctx, const Json::Value& tts_config);
  ~TextToSpeechProvider();

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
  // Pointer to platform-specific implementation
  std::unique_ptr<TextToSpeechProviderImpl> _impl;

};


} // end namespace TextToSpeech
} // end namespace Vector
} // end namespace Anki

#endif //__Anki_cozmo_cozmoAnim_textToSpeech_textToSpeechProvider_H__
