/**
 * File: audioFFT.h
 *
 * Author: ross
 * Created: November 25 2018
 *
 * Description: Vector wrapper for an FFT library.
 *              This is not a templated class (on, say, the window length) so that the
 *              library methods are not in the header
 *
 * Copyright: Anki, Inc. 2018
 *
 */

#ifndef ANIMPROCESS_COZMO_MICDATA_AUDIOFFT_H
#define ANIMPROCESS_COZMO_MICDATA_AUDIOFFT_H
#pragma once

#include "audioUtil/audioDataTypes.h"
#include "util/container/ringBuffContiguousRead.h"
#include "util/helpers/noncopyable.h"
#include <array>
#include <cstddef>

namespace Anki {
namespace Vector {

class AudioFFT : private Anki::Util::noncopyable
{
  using DataType = float;
  using BuffType = AudioUtil::AudioSample;
public:
  
  explicit AudioFFT( unsigned int N );
  
  ~AudioFFT();
  
  void AddSamples( const BuffType* samples, size_t numSamples );
  
  bool HasEnoughSamples() const { return _hasEnoughSamples; }
  
  // returns power of the last N samples in a vector of size N/2+1. Only call this if HasEnoughSamples().
  std::vector<DataType> GetPower();
  
  void Reset();
  
private:
  
  // Computes the DFT of _buff, saving to _outData, only if _buff has changed
  void DoDFT();
  
  void Cleanup();
  
  const unsigned int _N;
  Anki::Util::RingBuffContiguousRead<BuffType> _buff;
  
  bool _hasEnoughSamples = false;
  bool _dirty = false;
  
  DataType* _inData = nullptr;
  DataType* _outData = nullptr;
  void* _plan = nullptr; // pointer to library context
  
  std::vector<DataType> _windowCoeffs;
  
};
  
} // namespace Vector
} // namespace Anki

#endif // ANIMPROCESS_COZMO_MICDATA_AUDIOFFT_H

