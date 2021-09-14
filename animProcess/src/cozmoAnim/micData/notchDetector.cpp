/**
 * File: notchDetector.h
 *
 * Author: ross
 * Created: November 25 2018
 *
 * Description: Checks recently added power spectrums for a noticable notch around a specific band.
 *              The power is computed periodically during AddSamples (if requested), and HasNotch()
 *              will analyze average power using some ad-hoc rules.
 *              Use of the Sliding DFT algorithm might be useful here. In some quick benchmarks,
 *              the library used in audioFFT.h was still faster than my quick n dirty sliding DFT,
 *              but I've since changed the window size and period.
 *
 * Copyright: Anki, Inc. 2018
 *
 */

#include "cozmoAnim/micData/notchDetector.h"
#include "util/console/consoleInterface.h"
#include "util/logging/logging.h"

// only needed for debugging
#include <cmath> // log10
#include <fstream>
#include <sstream>

namespace Anki {
namespace Vector {
  
namespace {
  // saves PSDs that dont contain a notch to a file
  CONSOLE_VAR(bool, kSaveNotches, "MicData", false);
  
  const unsigned int kNotchIndex1 = 3;
  const unsigned int kNotchIndex2 = 8;
  
  const unsigned int kNotchIndex3 = 8;
  const unsigned int kNotchIndex4 = 16;
  
  CONSOLE_VAR_RANGED(float, kNotchPower, "Alexa", -0.41f, -1.f, 0.f);
  
  #define LOG_CHANNEL "Alexa"
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
NotchDetector::NotchDetector()
: _audioFFT{ 2*kNumPowers }
{ }
  
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void NotchDetector::AddSamples( const short* samples, size_t numSamples, bool analyze )
{
  
  _audioFFT.AddSamples( samples, numSamples );
  _sampleIdx += numSamples;
  if( !analyze || !_audioFFT.HasEnoughSamples() ) {
    return;
  }
  
  // this may skip ffts if numSamples is larger than the period, but that's fine here, and it doesn't happen in
  // the this very specific ad-hoc scenario this class is designed for where numSamples is 80
  const size_t kPeriod = 320; // 50 ms (at 16kHz)
  if( _sampleIdx > kPeriod ) {
    
    _sampleIdx = _sampleIdx % kPeriod;
    
    _powers[_idx] = _audioFFT.GetPower();
    
    ++_idx;
    if( _idx >= _powers.size() ) {
      _hasEnoughData = true;
      _idx = 0;
    }
    _dirty = true;
  }
}
  
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
bool NotchDetector::HasNotch()
{
  if( !_hasEnoughData ) {
    return false;
  }
  if( !_dirty ) {
    return _hasNotch;
  }
  _dirty = false;
  
  static int sIdx = 0;
  if( kSaveNotches ) {
    std::ofstream fout("/data/data/com.anki.victor/cache/alexa/notch" + std::to_string(sIdx) + ".csv");
    ++sIdx;
    std::stringstream ss;
    for( size_t i=0; i<kNumPowers; ++i ) {
      ss << _powers[0][i] << ",";
      float avgPower = 0.0f;
      for( int j=0; j<kNumToAvg; ++j ) {
        avgPower += _powers[j][i];
      }
      avgPower *= kNumToAvgRecip;
      fout << log10(avgPower) << ",";
    }
    fout << std::endl;
    fout.close();
  }
  
  // compare power in a range where our speaker can't output (A) to a range where our speaker can output (B)
  
  float sumPowerA = 0.0f;
  for( int i=kNotchIndex1; i<=kNotchIndex2; ++i ) {
    for( int j=0; j<kNumToAvg; ++j ) {
      sumPowerA += _powers[j][i];
    }
  }

  float sumPowerB = 0.0f;
  for( int i=kNotchIndex3; i<=kNotchIndex4; ++i ) {
    for( int j=0; j<kNumToAvg; ++j ) {
      sumPowerB += _powers[j][i];
    }
  }

  // If I had tuned this using log(sumPower/ |range| ) rather than log(sumPower)/|range|, this would
  // reduce to something efficient. But I didn't
  const float diff = log10(sumPowerA)/(kNotchIndex2 - kNotchIndex1+1) - log10(sumPowerB)/(kNotchIndex4 - kNotchIndex3+1);
  
  const bool powerful = (diff >= kNotchPower);

#if ANKI_DEV_CHEATS
  LOG_INFO( "NotchDetector.HasNotch.Debug",
            "Idx=%d, PowerA=%f (%f) PowerB=%f (%f), diff=%f, HUMAN=%d",
            sIdx, log10(sumPowerA), log10(sumPowerA)/(kNotchIndex2 - kNotchIndex1+1), log10(sumPowerB),
            log10(sumPowerB)/(kNotchIndex4 - kNotchIndex3+1), diff, powerful );
#endif
  
  return !powerful;
}
 
  
} // Vector
} // Anki
