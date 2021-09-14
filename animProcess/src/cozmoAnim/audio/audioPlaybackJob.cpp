/***********************************************************************************************************************
 *
 *  AudioPlaybackJob
 *  Victor / Anim
 *
 *  Created by Jarrod Hatfield on 4/10/2018
 *
 *  Description
 *  + System to load and playback recordings/audio files/etc
 *
 **********************************************************************************************************************/

// #include "cozmoAnim/audio/audioPlaybackJob.h"
#include "audioPlaybackJob.h"
#include "audioEngine/audioTools/standardWaveDataContainer.h"
#include "audioEngine/audioTools/audioWaveFileReader.h"
#include "audioUtil/audioDataTypes.h"
#include "audioUtil/waveFile.h"
#include "micDataTypes.h"

#include "util/logging/logging.h"

#include "clad/audio/audioEventTypes.h"
#include "clad/audio/audioGameObjectTypes.h"


namespace Anki {
namespace Vector {
namespace Audio {

using namespace AudioUtil;
using namespace AudioEngine;

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
AudioPlaybackJob::AudioPlaybackJob( const std::string& filename ) :
  _filename( filename ),
  _data( nullptr ),
  _isComplete( false )
{

}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
AudioPlaybackJob::~AudioPlaybackJob()
{
  Anki::Util::SafeDelete( _data );
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void AudioPlaybackJob::SetComplete()
{
  _isComplete = true;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
bool AudioPlaybackJob::IsComplete() const
{
  return _isComplete;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void AudioPlaybackJob::LoadAudioData()
{
  if ( !IsDataLoaded() )
  {
    // load in our new wav file
    _data = AudioWaveFileReader::LoadWaveFile( _filename );

    if ( nullptr != _data )
    {
      PRINT_CH_DEBUG( "VoiceMessage", "AudioPlaybackJob",
                     "Successful loaded .wav file [rate:%u] [channels:%u] [samples:%u]",
                     _data->sampleRate,
                     (uint32_t)_data->numberOfChannels,
                     (uint32_t)_data->bufferSize );
    }
    else
    {
      PRINT_CH_DEBUG( "VoiceMessage", "AudioPlaybackJob", "Failed to load .wav file (%s)", _filename.c_str() );
    }
  }

  SetComplete();
}

} // Audio
} // namespace Vector
} // namespace Anki
