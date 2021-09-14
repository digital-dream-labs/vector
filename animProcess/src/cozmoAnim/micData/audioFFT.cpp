/**
 * File: audioFFT.h
 *
 * Author: ross
 * Created: November 25 2018
 *
 * Description: Vector wrapper for a coretech-external FFT library.
 *              This is not a templated class (on, say, the window length) so that the
 *              library methods are not in the header
 *
 * Copyright: Anki, Inc. 2018
 *
 */

#include "cozmoAnim/micData/audioFFT.h"
#include "util/logging/logging.h"
#include "pffft.h"
#include <math.h>

namespace Anki {
namespace Vector {
  
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
AudioFFT::AudioFFT( unsigned int N )
: _N{ N }
, _buff{ N, N }
, _windowCoeffs(N, 0.0)
{
  Reset();
  
  // hann window coefficients
  for( int i=0; i<_N/2; i++ ) {
    DataType value = (1.0 - cos(2.0 * M_PI * i/(_N-1))) * 0.5;
    _windowCoeffs[i] = value;
    _windowCoeffs[_N-1-i] = value; // window is symmetric
  }
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
AudioFFT::~AudioFFT()
{
  Cleanup();
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void AudioFFT::AddSamples( const BuffType* samples, size_t numSamples )
{
  size_t available = _buff.Capacity() - _buff.Size();
  if( numSamples > available ) {
    _buff.AdvanceCursor( (unsigned int)(numSamples - available) );
  }
  const size_t numAdded = _buff.AddData( samples, (unsigned int) numSamples );
  DEV_ASSERT( numAdded == numSamples, "AudioFFT.AddSamples.CouldNotAdd" );
  
  if( !_hasEnoughSamples ) {
    _hasEnoughSamples = _buff.Size() >= _N;
  }
  
  _dirty |= (numSamples > 0);
  
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
std::vector<AudioFFT::DataType> AudioFFT::GetPower()
{
  std::vector<DataType> ret;
  
  if( !_hasEnoughSamples ) {
    return ret;
  }
  
  ret.reserve( _N/2 );
  
  // do dft if needed
  DoDFT();
  
  // compute power from _outData. real and imag components are interleaved
  static const DataType normFactor = 1.0 / (_N*_N);
  ret.push_back( (_outData[0]*_outData[0] + _outData[1]*_outData[1])*normFactor );
  for( int i=2; i<_N; i+=2 ) {
    const DataType mag = _outData[i]*_outData[i] + _outData[i+1]*_outData[i+1];
    ret.push_back( 2*normFactor*mag );
  }
  
  return ret;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void AudioFFT::Reset()
{
  Cleanup();

  _plan = (void*) PFFFT::pffft_new_setup( _N, PFFFT::PFFFT_REAL );
  
  int numBytes = _N * sizeof(float);
  _inData = (float*) PFFFT::pffft_aligned_malloc( numBytes );
  _outData = (float*) PFFFT::pffft_aligned_malloc( numBytes );
  
  _hasEnoughSamples = false;
  _dirty = false;
  _buff.Reset();
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void AudioFFT::DoDFT()
{
  if( !_dirty ) {
    return;
  }
  _dirty = false;
  
  // copy into aligned memory
  const BuffType* buffData = _buff.ReadData( _N );
  assert( buffData != nullptr );
  static const DataType factor = 1.0 / std::numeric_limits<BuffType>::max();
  for( int i=0; i<_N; ++i ) {
    const DataType fVal = *(buffData + i) * factor;
    _inData[i] = _windowCoeffs[i] * fVal;
  }
  // pffft docs say: "If 'work' is NULL, then stack will be used instead (this is probably the
  // best strategy for small FFTs, say for N < 16384)."
  float* work = nullptr;
  PFFFT::pffft_transform_ordered( (PFFFT::PFFFT_Setup*)_plan, _inData, _outData, work, PFFFT::PFFFT_FORWARD );
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void AudioFFT::Cleanup()
{
  if( _plan != nullptr ) {
    PFFFT::pffft_destroy_setup( (PFFFT::PFFFT_Setup*)_plan );
    _plan = nullptr;
  }
  if( _inData ) {
    PFFFT::pffft_aligned_free( (void*)_inData );
    _inData = nullptr;
  }
  if( _outData ) {
    PFFFT::pffft_aligned_free( (void*)_outData );
    _outData = nullptr;
  }
}

} // namespace Vector
} // namespace Anki
