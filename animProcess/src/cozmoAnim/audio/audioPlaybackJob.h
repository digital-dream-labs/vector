/***********************************************************************************************************************
 *
 *  AudioPlaybackJob
 *  Victor / Anim
 *
 *  Created by Jarrod Hatfield on 4/10/2018
 *
 *  Description
 *  + an audio job is tasked with loading audio data from file
 *  + it then converts and hands over said audio data to the audio engine for playback
 *
 **********************************************************************************************************************/

#ifndef __AnimProcess_CozmoAnim_AudioPlaybackJob_H__
#define __AnimProcess_CozmoAnim_AudioPlaybackJob_H__

#include "coretech/common/shared/types.h"

#include <atomic>
#include <functional>
#include <string>


namespace Anki {
  namespace AudioEngine {
    struct StandardWaveDataContainer;
  }

  namespace Vector {
    namespace Audio {
      class CozmoAudioController;
    }
  }
}

namespace Anki {
namespace Vector {
namespace Audio {

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
class AudioPlaybackJob
{
public:

  // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
  AudioPlaybackJob( const std::string& filename );
  ~AudioPlaybackJob();

  AudioPlaybackJob() = delete;
  AudioPlaybackJob( const AudioPlaybackJob& other ) = delete;
  AudioPlaybackJob& operator=( const AudioPlaybackJob& other ) = delete;


  // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

  // only call from loading thread
  void LoadAudioData();

  // thread safe
  bool IsComplete() const;


  // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
  // Access
  
  const std::string&                        GetFilename() const { return _filename; }
  AudioEngine::StandardWaveDataContainer*   GetAudioData() const { return _data; }


private:

  // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

  bool IsDataLoaded() const { return ( nullptr != _data ); }
  void SetComplete();


  // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

  std::string                               _filename;
  AudioEngine::StandardWaveDataContainer*   _data;

  std::atomic<bool>                         _isComplete;
};

} //  namespace Audio
} // namespace Vector
} // namespace Anki

#endif // __AnimProcess_CozmoAnim_AudioPlaybackJob_H__
