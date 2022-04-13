/*
 * File: audioBehaviorStackListener.h
 *
 * Author: Jordan Rivas
 * Created: 5/30/2018
 *
 * Description: Audio Behavior Stack listeners is used to post audio scene event for behavior stack updates. It loads a
 *              JSON metadata file that is maintained by the Audio Designers, formatted by the audio build server
 *              scripts and is delivered with the other audio assets. The file defines a behavior path and stack state
 *              audio events. The Path string is considered to be the tail of the path allowing designers to wildcard
 *              the beginning of the path. When the behavior stack has an update it sends a message containing the
 *              current behavior path and stack state. This class listens for those messages, finds the most relevant
 *              behavior node and plays the event corresponding with the stack state.
 *
 * Copyright: Anki, Inc. 2018
 */


#ifndef __Anki_Engine_AudioBehaviorStackListener__H_
#define __Anki_Engine_AudioBehaviorStackListener__H_


#include "clad/audio/audioEventTypes.h"
#include "engine/aiComponent/behaviorComponent/behaviors/iCozmoBehavior_fwd.h"
#include <map>
#include <memory>


namespace Anki {
namespace Util {
namespace Data {
class DataPlatform;
}
}
namespace Vector {
class BehaviorContainer;
class CozmoContext;
namespace ExternalInterface {
struct AudioBehaviorStackUpdate;
}
namespace Audio {
class EngineRobotAudioClient;

class AudioBehaviorStackListener
{
public:
  
  AudioBehaviorStackListener( EngineRobotAudioClient& audioClient, const CozmoContext* context );
  
  void HandleAudioBehaviorMessage( const ExternalInterface::AudioBehaviorStackUpdate& message );


private:
  
  struct BehaviorNode;
  using BehaviorTreeMap = std::map<BehaviorID, BehaviorNode>;
  using GenericEvent = AudioMetaData::GameEvent::GenericEvent;
  
  struct AudioEventNode {
    GenericEvent onActivate   = GenericEvent::Invalid;
    GenericEvent onDeactivate = GenericEvent::Invalid;
  };
  
  struct BehaviorNode {
    BehaviorID behaviorId;
    std::unique_ptr<AudioEventNode> audioEvents;
    BehaviorTreeMap childrenMap;
    BehaviorNode( BehaviorID behaviorId )
    : behaviorId( behaviorId ) {}
  };
  
  EngineRobotAudioClient& _audioClient;
  BehaviorTreeMap         _reversedPathBehaviorTree;
  
  // Read json metadata resorce file
  void LoadMetaData( Util::Data::DataPlatform* dataPlatform );
  
};

} // Audio
} // Cozmo
} // Anki



#endif /* __Anki_Engine_AudioBehaviorStackListener__H_ */
