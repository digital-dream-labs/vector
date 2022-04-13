/*
 * File: audioBehaviorStackListener.cpp
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

#include "engine/audio/audioBehaviorStackListener.h"

#include "audioEngine/audioTypes.h"
#include "coretech/common/engine/jsonTools.h"
#include "coretech/common/engine/utils/data/dataPlatform.h"
#include "clad/audio/audioGameObjectTypes.h"
#include "clad/externalInterface/messageEngineToGame.h"
#include "engine/audio/engineRobotAudioClient.h"
#include "engine/cozmoContext.h"
#include "json/json.h"
#include "util/fileUtils/fileUtils.h"
#include "util/logging/logging.h"
#include <sstream>


namespace Anki {
namespace Vector {  
namespace Audio {
namespace {
const char* kAudioBehaviorMetadataFile = "audioBehaviorSceneEvents.json";
}
  
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
AudioBehaviorStackListener::AudioBehaviorStackListener( EngineRobotAudioClient& audioClient, const CozmoContext* context )
: _audioClient( audioClient )
{
  LoadMetaData( context->GetDataPlatform() );
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void AudioBehaviorStackListener::HandleAudioBehaviorMessage(const ExternalInterface::AudioBehaviorStackUpdate& message)
{
  using namespace ExternalInterface;
  if (message.branchPath.empty()) {
    PRINT_NAMED_WARNING("AudioBehaviorStackListener.HandleAudioBehaviorMessage", "message.branchPath.IsEmpty");
    return;
  }
  
  // Find best match for branch path
  // Start from at the end and walk forward
  auto revPathIt = message.branchPath.rbegin();
  const auto it = _reversedPathBehaviorTree.find(*revPathIt);
  if (it == _reversedPathBehaviorTree.end()) {
    // no events for leaf
    return;
  }

  // Keep walking towards the root of the path to find the best match
  const BehaviorNode* node = &it->second;
  while( ++revPathIt != message.branchPath.rend() ) {
    auto childIt = node->childrenMap.find(*revPathIt);
    if (childIt == node->childrenMap.end()) {
      // Current node is the best path match
      break;
    }
    // Set node to next best match
    node = &childIt->second;
  }
  
  if ( nullptr == node->audioEvents.get() ) {
    PRINT_NAMED_WARNING("AudioBehaviorStackListener.HandleAudioBehaviorMessage", "BehaviorStackNode.AudioEvents.IsNull");
    // Do nothing
    return;
  }
  
  // Get audio event from node for behavior stack state
  auto event = GenericEvent::Invalid;
  switch (message.state) {
    case BehaviorStackState::Active:
      event = node->audioEvents->onActivate;
      break;

    case BehaviorStackState::NotActive:
      event = node->audioEvents->onDeactivate;
      break;
  }
  
  // Post Audio Event
  if ( GenericEvent::Invalid != event ) {
    _audioClient.PostEvent( event, AudioMetaData::GameObjectType::Behavior );
  }
}
  
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// String parsing delimiter
template<char delimiter>
class PathDelimiter : public std::string
{};
template<char delimiter>
std::istream& operator>>(std::istream& is, PathDelimiter<delimiter>& output)
{
  std::getline(is, output, delimiter);
  return is;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void AudioBehaviorStackListener::LoadMetaData( Util::Data::DataPlatform* dataPlatform )
{
  ASSERT_NAMED(dataPlatform != nullptr, "AudioBehaviorStackListener.LoadMetaData.dataPlatform.IsNull");
  if (dataPlatform == nullptr) {
    return;
  }
  
  // Load file from disk
  const std::string fileName = Util::FileUtils::FullFilePath({ "sound", std::string(kAudioBehaviorMetadataFile)} );
  Json::Value behaviorNodeData;
  const bool readJson = dataPlatform->readAsJson(Anki::Util::Data::Scope::Resources, fileName, behaviorNodeData);
  if (!readJson || !behaviorNodeData.isArray() ) {
    PRINT_NAMED_WARNING("AudioBehaviorStackListener.LoadMetaData", "ErrorReadingJsonFile: '%s'", fileName.c_str());
    return;
  }
  
  // Parse file keys
  const std::string kActivateKey    = "activate";
  const std::string kDeactivateKey  = "deactivate";
  const std::string kPathKey        = "path";
  
  // Loop through JSON list of audio behavior nodes
  std::string pathStr;
  std::string activateEventStr;
  std::string deactivateEventStr;
  auto activateEvent   = GenericEvent::Invalid;
  auto deactivateEvent = GenericEvent::Invalid;
  
  for ( const auto& aNode : behaviorNodeData ) {
    
    if ( !JsonTools::GetValueOptional( aNode, kPathKey, pathStr ) ) {
      // Must have path
      PRINT_NAMED_WARNING("AudioBehaviorStackListener.LoadMetaData", "InvalidData.MissingPathKey");
      continue;
    }
    if ( pathStr.empty() ) {
      PRINT_NAMED_WARNING("AudioBehaviorStackListener.LoadMetaData", "InvalidData.Path.EmptyString: '%s'",
                          pathStr.c_str());
      continue;
    }
    
    std::unique_ptr<AudioEventNode> audioNode( new AudioEventNode() );
    // Set Activate audio event
    if ( JsonTools::GetValueOptional( aNode, kActivateKey, activateEventStr ) ) {
      if ( AudioMetaData::GameEvent::EnumFromString( activateEventStr, activateEvent ) ) {
        audioNode->onActivate = activateEvent;
      }
      else {
        PRINT_NAMED_WARNING("AudioBehaviorStackListener.LoadMetaData", "Path: '%s' has invalid '%s' audio event: '%s'",
                            pathStr.c_str(), kActivateKey.c_str(), activateEventStr.c_str());
      }
    }
    // Set Deactivate audio event
    if ( JsonTools::GetValueOptional( aNode, kDeactivateKey, deactivateEventStr ) ) {
      if ( AudioMetaData::GameEvent::EnumFromString( deactivateEventStr, deactivateEvent ) ) {
        audioNode->onDeactivate = deactivateEvent;
      }
      else {
        PRINT_NAMED_WARNING("AudioBehaviorStackListener.LoadMetaData", "Path: '%s' has invalid '%s' audio event: '%s'",
                            pathStr.c_str(), kDeactivateKey.c_str(), deactivateEventStr.c_str());
      }
    }
    
    // Store data into behavior tree
    // Behavior tree is built by storing the path in reverse order. First tail and traverse to the head of path

    // Parse path
    std::istringstream stream( pathStr );
    std::vector<std::string> results( (std::istream_iterator<PathDelimiter<'/'>>(stream)),
                                      std::istream_iterator<PathDelimiter<'/'>>() );

    // Start at tail of path
    auto revPathIt = results.rbegin();
    BehaviorNode* node = nullptr;
    BehaviorID behaviorId;
    bool validNode = true;
    if( !EnumFromString(*revPathIt, behaviorId) ) {
      PRINT_NAMED_WARNING("AudioBehaviorStackListener.LoadMetaData", "Invalid BehaviorId from string: '%s'",
                          revPathIt->c_str());
      continue;
    }
    
    const auto reversedPathBehaviorTreeNodeIt = _reversedPathBehaviorTree.find( behaviorId );
    if ( reversedPathBehaviorTreeNodeIt != _reversedPathBehaviorTree.end() ) {
      // Found Leaf
      node = &reversedPathBehaviorTreeNodeIt->second;
    }
    else {
      // Add Leaf
      const auto emplaceIt = _reversedPathBehaviorTree.emplace( behaviorId, behaviorId );
      node = &emplaceIt.first->second;
    }
    
    // Walk through behavior tree nodes towards the root of the path
    while( ++revPathIt != results.rend() ) {
      // Allow wildcard path prefix
      if (*revPathIt == "*") {
        break;
      }
      // Get next behavior node in path
      BehaviorID behaviorId;
      if( !EnumFromString(*revPathIt, behaviorId) ) {
        PRINT_NAMED_WARNING("AudioBehaviorStackListener.LoadMetaData", "Invalid BehaviorId from string: '%s'",
                            revPathIt->c_str());
        validNode = false;
        break;
      }
      else {
        // Get or create child node
        const auto childIt = node->childrenMap.find( behaviorId );
        if ( childIt != node->childrenMap.end() ) {
          // Child already exists
          node = &childIt->second;
        }
        else {
          // Add Child
          const auto emplaceIt = node->childrenMap.emplace(behaviorId, behaviorId);
          node = &emplaceIt.first->second;
        }
      }
    } // while ( ++revPathIt != results.rend() )
    
    if ( !validNode ) {
      // BehaviorId ERROR already printed, continue to next audio node
      PRINT_NAMED_WARNING("AudioBehaviorStackListener.LoadMetaData", "Invalid node path");
      continue;
    }
    
    // Add Audio Event to leaf
    if ( nullptr != node->audioEvents.get() ) {
      PRINT_NAMED_WARNING("AudioBehaviorStackListener.LoadMetaData", "AudioDataAlreadyExistOnNode");
    }
    // Add audio node to behavior node
    node->audioEvents.reset( audioNode.release() );
    
  } // for ( const auto& aNode : behaviorNodeData )
}

} // Audio
} // Cozmo
} // Anki
