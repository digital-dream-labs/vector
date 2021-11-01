/*
 * File: engineRobotAudioClient.cpp
 *
 * Author: Jordan Rivas
 * Created: 9/14/2017
 *
 * Description: This is a subclass of AudioMuxClient which provides communication between itself and an
 *              EndingRobotAudioInput by means of EngineToRobot and RobotToEngine messages. It's purpose is to provide
 *              an interface to perform audio tasks and respond to audio callbacks sent from the audio engine in the
 *              animation process to engine process.
 *
 *
 * Copyright: Anki, Inc. 2017
 */

#include "engine/audio/engineRobotAudioClient.h"

#include "audioEngine/multiplexer/audioCladMessageHelper.h"
#include "engine/audio/audioBehaviorStackListener.h"
#include "engine/cozmoContext.h"
#include "engine/robot.h"
#include "engine/robotManager.h"
#include "engine/robotInterface/messageHandler.h"
#include "clad/externalInterface/messageGameToEngine.h"
#include "clad/externalInterface/messageEngineToGame.h"
#include "clad/robotInterface/messageEngineToRobot.h"
#include "clad/robotInterface/messageRobotToEngine.h"
#include "util/logging/logging.h"


namespace Anki {
namespace Vector {  
namespace Audio {

namespace AECH = AudioEngine::Multiplexer::CladMessageHelper; 
namespace AMD = AudioMetaData;

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
EngineRobotAudioClient::EngineRobotAudioClient()
: IDependencyManagedComponent( this, RobotComponentID::EngineAudioClient )
{
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void EngineRobotAudioClient::InitDependent(Vector::Robot* robot, const RobotCompMap& dependentComps)
{
  // Create & setup behavior listener
  _behaviorListener.reset( new AudioBehaviorStackListener( *this, robot->GetContext() ) );
  // Subscribe to audio messages
  SubscribeAudioCallbackMessages( robot );
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// Engine Robot Audio Client Helper Methods
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void EngineRobotAudioClient::SetRobotMasterVolume( external_interface::Volume volume )
{
  using AudioVolumeState = AudioMetaData::GameState::Robot_Vic_Volume;
  auto audioState = AudioVolumeState::Invalid;
  switch (volume) {
    case external_interface::Volume::MUTE:
      audioState = AudioVolumeState::Mute;
      break;
    case external_interface::Volume::LOW:
      audioState = AudioVolumeState::Low;
      break;
    case external_interface::Volume::MEDIUM_LOW:
      audioState = AudioVolumeState::Mediumlow;
      break;
    case external_interface::Volume::MEDIUM:
      audioState = AudioVolumeState::Medium;
      break;
    case external_interface::Volume::MEDIUM_HIGH:
      audioState = AudioVolumeState::Mediumhigh;
      break;
    case external_interface::Volume::HIGH:
      audioState = AudioVolumeState::High;
      break;
    default:
      break;
  }
  PostGameState( AudioMetaData::GameState::StateGroupType::Robot_Vic_Volume,
                 static_cast<AudioMetaData::GameState::GenericState>(audioState) );
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// Engine -> Robot Methods
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
EngineRobotAudioClient::CallbackIdType EngineRobotAudioClient::PostEvent( AMD::GameEvent::GenericEvent event,
                                                                          AMD::GameObjectType gameObject,
                                                                          CallbackFunc&& callback )
{
  if (_robot == nullptr) {
    PRINT_NAMED_WARNING("EngineRobotAudioClient.PostEvent", "_robot is NULL, can NOT send message");
    return kInvalidCallbackId;
  }
  // NOTE: Since we are using C++ Lite the CLAD structs variables need to be put in a different or then the interface
  const auto callbackId = ManageCallback( std::move( callback ) );
  _robot->SendMessage( RobotInterface::EngineToRobot( AECH::CreatePostAudioEvent( event, gameObject, callbackId ) ) );
  return callbackId;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void EngineRobotAudioClient::StopAllEvents( AMD::GameObjectType gameObject )
{
  if (_robot == nullptr) {
    PRINT_NAMED_WARNING("EngineRobotAudioClient.StopAllEvents", "_robot is NULL, can NOT send message");
    return;
  }
  _robot->SendMessage( RobotInterface::EngineToRobot( AECH::CreateStopAllAudioEvents( gameObject ) ) );
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void EngineRobotAudioClient::PostGameState( AMD::GameState::StateGroupType gameStateGroup,
                                            AMD::GameState::GenericState gameState )
{
  if (_robot == nullptr) {
    PRINT_NAMED_WARNING("EngineRobotAudioClient.PostGameState", "_robot is NULL, can NOT send message");
    return;
  }
  _robot->SendMessage( RobotInterface::EngineToRobot( AECH::CreatePostAudioGameState( gameStateGroup, gameState ) ) );
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void EngineRobotAudioClient::PostSwitchState( AMD::SwitchState::SwitchGroupType switchGroup,
                                              AMD::SwitchState::GenericSwitch switchState,
                                              AMD::GameObjectType gameObject )
{
  if (_robot == nullptr) {
    PRINT_NAMED_WARNING("EngineRobotAudioClient.PostSwitchState", "_robot is NULL, can NOT send message");
    return;
  }
  _robot->SendMessage( RobotInterface::EngineToRobot( AECH::CreatePostAudioSwitchState( switchGroup,
                                                                                        switchState,
                                                                                        gameObject ) ) );
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void EngineRobotAudioClient::PostParameter( AMD::GameParameter::ParameterType parameter,
                                            float parameterValue,
                                            AMD::GameObjectType gameObject,
                                            int32_t timeInMilliSeconds,
                                            CurveType curve ) const
{
  if (_robot == nullptr) {
    PRINT_NAMED_WARNING("EngineRobotAudioClient.PostParameter", "_robot is NULL, can NOT send message");
    return;
  }

  _robot->SendMessage( RobotInterface::EngineToRobot( AECH::CreatePostAudioParameter( parameter,
                                                                                      parameterValue,
                                                                                      gameObject,
                                                                                      timeInMilliSeconds,
                                                                                      curve ) ) );
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// Robot -> Engine Methods
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void EngineRobotAudioClient::SubscribeAudioCallbackMessages( Robot* robot )
{
  DEV_ASSERT(robot != nullptr, "EngineRobotAudioClient.Init._robot.IsNull");
  if (robot == nullptr) { return; }
  
  // Setup robot message handlers
  _robot = robot;
  RobotInterface::MessageHandler* messageHandler = _robot->GetContext()->GetRobotManager()->GetMsgHandler();
  
  // Subscribe to RobotToEngine messages
  using localHandlerType = void(EngineRobotAudioClient::*)(const AnkiEvent<RobotInterface::RobotToEngine>&);
  // Create a helper lambda for subscribing to a tag with a local handler
  auto doRobotSubscribe = [this, messageHandler] ( RobotInterface::RobotToEngineTag tagType,
                                                   localHandlerType handler )
  {
    _signalHandles.push_back(messageHandler->Subscribe( tagType,
                                                        std::bind( handler, this, std::placeholders::_1 ) ));
  };

  // Bind to specific handlers in the audio clients
  doRobotSubscribe(RobotInterface::RobotToEngineTag::audioCallbackDuration, &EngineRobotAudioClient::HandleRobotEngineMessage);
  doRobotSubscribe(RobotInterface::RobotToEngineTag::audioCallbackMarker, &EngineRobotAudioClient::HandleRobotEngineMessage);
  doRobotSubscribe(RobotInterface::RobotToEngineTag::audioCallbackComplete, &EngineRobotAudioClient::HandleRobotEngineMessage);
  doRobotSubscribe(RobotInterface::RobotToEngineTag::audioCallbackError, &EngineRobotAudioClient::HandleRobotEngineMessage);
  
  // Add Listeners to GameToEngine messages
  auto robotVolumeCallbackFunc = [this] ( const AnkiEvent<ExternalInterface::MessageGameToEngine>& message )
  {
    // TODO: Need to be sure this is only used for DEV and Factory work
    const ExternalInterface::SetRobotVolume& msg = message.GetData().Get_SetRobotVolume();
    DEV_ASSERT(((msg.volume >= 0.0f) && (msg.volume <= 1.0f)),
               "EngineRobotAudioClient.SetRobotMasterVolume.Volume.InvalidValue");
    PostParameter( AMD::GameParameter::ParameterType::Robot_Vic_Volume_Master, msg.volume);
  };
  
  // Add Listenters to EngineToGame messages
  auto audioBehaviorStackUpdateFunc = [this] ( const AnkiEvent<ExternalInterface::MessageEngineToGame>& message )
  {
    _behaviorListener->HandleAudioBehaviorMessage( message.GetData().Get_AudioBehaviorStackUpdate() );
  };
  
  IExternalInterface* externalInterface = _robot->GetContext()->GetExternalInterface();
  if ( externalInterface ) {
    _signalHandles.push_back(externalInterface->Subscribe(ExternalInterface::MessageGameToEngineTag::SetRobotVolume,
                                                          robotVolumeCallbackFunc));
    _signalHandles.push_back(externalInterface->Subscribe(ExternalInterface::MessageEngineToGameTag::AudioBehaviorStackUpdate,
                                                          audioBehaviorStackUpdateFunc));
  }
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void EngineRobotAudioClient::HandleRobotEngineMessage( const AnkiEvent<RobotInterface::RobotToEngine>& message )
{
  switch ( static_cast<RobotInterface::RobotToEngineTag>( message.GetType() ) ) {
    
    case RobotInterface::RobotToEngine::Tag::audioCallbackDuration:
      HandleCallbackEvent( message.GetData().Get_audioCallbackDuration() );
      break;
    
    case RobotInterface::RobotToEngine::Tag::audioCallbackMarker:
      HandleCallbackEvent( message.GetData().Get_audioCallbackMarker() );
      break;
      
    case RobotInterface::RobotToEngine::Tag::audioCallbackComplete:
      HandleCallbackEvent( message.GetData().Get_audioCallbackComplete() );
      break;
      
    case RobotInterface::RobotToEngine::Tag::audioCallbackError:
      HandleCallbackEvent( message.GetData().Get_audioCallbackError() );
      break;
      
    default:
      PRINT_NAMED_ERROR("EngineRobotAudioClient.HandleRobotEngineMessage", "Unexpected message type");
      break;
  }
}

} // Audio
} // Cozmo
} // Anki
