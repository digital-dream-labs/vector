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

#include "cozmoAnim/beatDetector/beatDetector.h"

#include "util/console/consoleInterface.h"
#include "util/logging/logging.h"
#include "util/math/math.h"
#include "util/time/universalTime.h"

namespace Anki {
namespace Vector {

namespace {
  // This scale factor gets applied to the output tempo estimate of the aubio tempo detector. It has been seen that the
  // tempo detector usually reports a tempo ~1.2% higher than the actual tempo when using the _processed_ audio stream,
  // and about 1.7% higher than actual when using the _raw_ audio stream (see kBeatDetectorUseProcessedAudio).
  CONSOLE_VAR(float, kTempoCorrectionScaleFactor, "MicData", 0.988f);
  
  // Every once in a while, we reset the aubio tempo detection object,
  // just in case it is carrying some weird state or taking up memory.
  // (This is a recommendation from the library's author)
  const float kTempoDetectorResetTime_sec = 60.f * 60.f;
  
  const char* const kAubioTempoMethod = "default";
  
  BeatInfo kInvalidBeatInfo{
    .tempo_bpm  = -1.f,
    .confidence = -1.f,
    .time_sec   = -1.f
  };
}

BeatDetector::BeatDetector()
  : _latestBeat(kInvalidBeatInfo)
{
  Start();
}

BeatDetector::~BeatDetector()
{
  Stop();
}

bool BeatDetector::AddSamples(const AudioUtil::AudioSample* const samples, const uint32_t nSamples)
{
  if (!IsRunning()) {
    return false;
  }
  
  const auto now_sec = static_cast<float>(Util::Time::UniversalTime::GetCurrentTimeInSeconds());
  
  // If the tempo detector has been running for too long, time to reset it.
  if (now_sec - _tempoDetectionStartedTime_sec > kTempoDetectorResetTime_sec) {
    PRINT_NAMED_INFO("BeatDetector.AddSamples.ResettingBeatDetector",
                     "Resetting beat detector since it has been %.1f seconds",
                     kTempoDetectorResetTime_sec);
    Start();
  }
  
  DEV_ASSERT(_aubioTempoDetector != nullptr, "BeatDetector.AddSamples.NullAubioTempoObject");
  
  // Place new data into the staging buffer
  DEV_ASSERT(_aubioInputBuffer.capacity() - _aubioInputBuffer.size() >= nSamples, "BeatDetector.AddSamples.AubioInputBufferIsFull");
  _aubioInputBuffer.push_back(samples, nSamples);
  
  // Feed the aubio tempo detector correct-sized chunks (size kAubioTempoHopSize)
  bool beatDetected = false;
  while (_aubioInputBuffer.size() >= kAubioTempoHopSize) {
    for (int i=0 ; i<kAubioTempoHopSize ; i++) {
      // Copy from the front of the input buffer, and convert from signed int
      // to floating point [-1.0, 1.0)
      auto sample = static_cast<smpl_t>(_aubioInputBuffer.front()) / (std::numeric_limits<AudioUtil::AudioSample>::max() + 1);
      fvec_set_sample(_aubioInputVec, sample, i);
      _aubioInputBuffer.pop_front();
    }
    
    // pass this into the aubio tempo detector
    aubio_tempo_do(_aubioTempoDetector, _aubioInputVec, _aubioOutputVec);
    
    // Check the output to see if a beat was detected
    const bool isBeat = fvec_get_sample(_aubioOutputVec, 0) != 0.f;
    if (isBeat) {
      beatDetected = true;
      auto tempo = aubio_tempo_get_bpm(_aubioTempoDetector);
      // Note: We 'correct' the estimated tempo here since it seems to always
      // report a faster-than-reality tempo.
      tempo *= kTempoCorrectionScaleFactor;
      const auto conf = aubio_tempo_get_confidence(_aubioTempoDetector);
      {
        std::lock_guard<std::mutex> lock(_latestBeatMutex);
        _latestBeat.tempo_bpm = tempo;
        _latestBeat.confidence = conf;
        _latestBeat.time_sec = now_sec; // NOTE: this is approximate. Should really do math with aubio_tempo_get_last_ms()
      }
    }
  }
  
  return beatDetected;
}


BeatInfo BeatDetector::GetLatestBeat()
{
  std::lock_guard<std::mutex> lock(_latestBeatMutex);
  return _latestBeat;
}

  
bool BeatDetector::IsRunning()
{
  return (_aubioTempoDetector != nullptr);
}
  
void BeatDetector::Start()
{
  // Call Stop() to free/reset any existing objects
  Stop();
  
  _aubioTempoDetector = new_aubio_tempo(kAubioTempoMethod, kAubioTempoBufSize, kAubioTempoHopSize, kAubioTempoSampleRate);
  _aubioInputVec = new_fvec(kAubioTempoHopSize);
  _aubioOutputVec = new_fvec(1);
  
  const auto now_sec = static_cast<float>(Util::Time::UniversalTime::GetCurrentTimeInSeconds());
  _tempoDetectionStartedTime_sec = now_sec;
}


void BeatDetector::Stop()
{
  if (_aubioTempoDetector != nullptr) {
    del_aubio_tempo(_aubioTempoDetector);
    _aubioTempoDetector = nullptr;
  }
  
  if (_aubioInputVec != nullptr) {
    del_fvec(_aubioInputVec);
    _aubioInputVec = nullptr;
  }
  
  if (_aubioOutputVec != nullptr) {
    del_fvec(_aubioOutputVec);
    _aubioOutputVec = nullptr;
  }
  
  _aubioInputBuffer.clear();
  
  _latestBeat = kInvalidBeatInfo;
}


} // namespace Vector
} // namespace Anki

