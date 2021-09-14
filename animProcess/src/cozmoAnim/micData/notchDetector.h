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

#ifndef ANIMPROCESS_COZMO_MICDATA_NOTCH_DETECTOR_H
#define ANIMPROCESS_COZMO_MICDATA_NOTCH_DETECTOR_H
#pragma once

#include "cozmoAnim/micData/audioFFT.h"

#include <array>

namespace Anki {
namespace Vector {


class NotchDetector
{
public:
  
  NotchDetector();
  
  void AddSamples( const short* samples, size_t numSamples, bool analyze );
  
  bool HasNotch();
  
private:
  
  static constexpr unsigned int kNumPowers = 128; // should be a power of 2
  static constexpr unsigned int kNumToAvg = 10;
  static constexpr float kNumToAvgRecip = 1.0f/kNumToAvg;
  AudioFFT _audioFFT;
  size_t _sampleIdx=0;
  unsigned int _idx = 0; // idx into _powers
  std::array< std::vector<float>, kNumToAvg > _powers;
  bool _hasEnoughData = false;
  
  bool _hasNotch = false;
  bool _dirty = false;
  
};
  
} // Vector
} // Anki

#endif // ANIMPROCESS_COZMO_MICDATA_NOTCH_DETECTOR_H
