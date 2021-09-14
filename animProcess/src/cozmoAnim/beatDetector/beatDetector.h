/**
 * File: beatDetector
 *
 * Author: Matt Michini
 * Created: 5/11/2018
 *
 * Description: Beats-per-minute and beat onset detection using aubio library
 *
 * Copyright: Anki, Inc. 2018
 *
 */

#ifndef __AnimProcess_CozmoAnim_BeatDetector_H_
#define __AnimProcess_CozmoAnim_BeatDetector_H_

#include "micDataTypes.h"
#include "aubio/aubio.h"
#include "audioUtil/audioDataTypes.h"
#include "clad/types/beatDetectorTypes.h"
#include "coretech/common/shared/types.h"
#include "util/container/fixedCircularBuffer.h"

#include <mutex>

namespace Anki {
namespace Vector {

class BeatDetector
{
public:
  BeatDetector();
  ~BeatDetector();
  BeatDetector(const BeatDetector& other) = delete;
  BeatDetector& operator=(const BeatDetector& other) = delete;
  
  // Feed raw audio samples into the beat detector. Returns true if a beat
  // was detected in the input samples.
  bool AddSamples(const AudioUtil::AudioSample* const samples, const uint32_t nSamples);
  
  // Return info about the most recent detected beat
  BeatInfo GetLatestBeat();
  
  // Is beat detection currently running?
  bool IsRunning();
  
  // Start or reset beat detection
  void Start();
  
  // Stop the beat detector and delete the associated objects
  void Stop();
  
private:
  
  static const uint_t kAubioTempoBufSize = 512;
  static const uint_t kAubioTempoHopSize = 256;
#ifdef SIMULATOR
  static const uint_t kAubioTempoSampleRate = AudioUtil::kSampleRate_hz;
#else
  // The downsampling process in syscon results in an actual sample rate of 15625 Hz
  static const uint_t kAubioTempoSampleRate = 15625;
#endif
  
  // Aubio beat detector object:
  aubio_tempo_t* _aubioTempoDetector = nullptr;
  
  // Aubio beat detector input/output vectors:
  fvec_t* _aubioInputVec = nullptr;
  fvec_t* _aubioOutputVec = nullptr;
  
  // System time when the aubio tempo detector started running
  float _tempoDetectionStartedTime_sec = 0.f;
  
  BeatInfo _latestBeat;
  std::mutex _latestBeatMutex;
  
  // Stages audio data to be piped into the aubio detector at the correct chunk size.
  // Use twice the capacity we actually need just to be safe.
  Util::FixedCircularBuffer<AudioUtil::AudioSample, 2 * (kAubioTempoHopSize + MicData::kSamplesPerBlockPerChannel)> 
      _aubioInputBuffer;
};

} // namespace Vector
} // namespace Anki

#endif // __AnimProcess_CozmoAnim_BeatDetector_H_

