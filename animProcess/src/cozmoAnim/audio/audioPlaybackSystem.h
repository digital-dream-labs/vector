/***********************************************************************************************************************
 *
 *  AudioPlaybackSystem
 *  Victor / Anim
 *
 *  Created by Jarrod Hatfield on 4/10/2018
 *
 *  Description
 *  + System to load and playback recordings/audio files/etc
 *
 **********************************************************************************************************************/

#ifndef __AnimProcess_CozmoAnim_AudioPlaybackSystem_H__
#define __AnimProcess_CozmoAnim_AudioPlaybackSystem_H__

#include "audioUtil/audioDataTypes.h"
#include "clad/robotInterface/messageRobotToEngine.h"
#include "coretech/common/shared/types.h"

#include <queue>


namespace Anki {
namespace Vector {
namespace Anim {
  class AnimContext;
}
}
}

namespace Anki {
namespace Vector {
namespace Audio {

class AudioPlaybackJob;

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
class AudioPlaybackSystem
{
public:

  // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
  AudioPlaybackSystem( const Anim::AnimContext* context );
  ~AudioPlaybackSystem();

  AudioPlaybackSystem() = delete;
  AudioPlaybackSystem( const AudioPlaybackSystem& other ) = delete;
  AudioPlaybackSystem& operator=( const AudioPlaybackSystem& other ) = delete;

  // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

  void Update( BaseStationTime_t currTime_nanosec );
  void PlaybackAudio( const std::string& path );


private:

  // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
  bool IsValidFile( const std::string& path ) const;

  // audio loading jobs
  void StartNextJobInQueue();
  static void LoadAudioPlaybackData( std::shared_ptr<AudioPlaybackJob> audiojob );

  // audio playback
  void BeginAudioPlayback();
  void OnAudioPlaybackBegin();
  void OnAudioPlaybackEnd();


  // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
  // Member Data

  const Anim::AnimContext*            _animContext;

  std::shared_ptr<AudioPlaybackJob>   _currentJob;
  std::queue<AudioPlaybackJob*>       _jobQueue;
  bool                                _isJobLoading;
};

} //  namespace Audio
} // namespace Vector
} // namespace Anki

#endif // __AnimProcess_CozmoAnim_AudioPlaybackSystem_H__
