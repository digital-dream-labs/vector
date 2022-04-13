/*
 * File:          UiGameController.cpp
 * Date:
 * Description:
 * Author:
 * Modifications:
 */

#include "simulator/game/uiGameController.h"
#include "simulator/controllers/shared/webotsHelpers.h"
#include "anki/cozmo/shared/cozmoEngineConfig.h"
#include "engine/aiComponent/behaviorComponent/behaviorTypesWrapper.h"
#include "engine/cozmoAPI/comms/gameComms.h"
#include "engine/cozmoAPI/comms/gameMessageHandler.h"
#include "clad/externalInterface/messageEngineToGame.h"
#include "clad/externalInterface/messageGameToEngine.h"
#include "util/cladHelpers/cladFromJSONHelpers.h"
#include "util/transport/udpTransport.h"
// includes for physics functions
#include "coretech/messaging/shared/UdpClient.h"
#include "clad/physicsInterface/messageSimPhysics.h"
// end of physics includes
#include <stdio.h>
#include <string.h>

#define LOG_CHANNEL "Keyboard"

namespace Anki {
  namespace Vector {

      // Private members:
      namespace {

      } // private namespace


    // ======== Message handler callbacks =======

    void UiGameController::AddOrUpdateObject(s32 objID,
                                             ObjectType objType,
                                             const PoseStruct3d& poseStruct,
                                             const uint32_t observedTimestamp,
                                             const bool isActive)
    {
      // If an object with the same ID already exists in the map, make sure that its type hasn't changed and update its
      // observed time
      auto it = std::find_if(_observedObjects.begin(),
                             _observedObjects.end(),
                             [&objID](const ObservedObject& obj) {
                               return (obj.id == objID);
                             });
      if (it != _observedObjects.end()) {
        if (it->type != objType) {
          PRINT_NAMED_WARNING("UiGameController.HandleRobotObservedObjectBase.ObjectChangedType", "");
        }
        // Update the observedTimestamp if it is nonzero
        if (observedTimestamp != 0) {
          it->observedTimestamp = observedTimestamp;
        }
        it->pose = CreatePoseHelper(poseStruct);
      } else {
        // Insert new object into container
        ObservedObject obj;
        obj.type = objType;
        obj.id = objID;
        obj.isActive = isActive;
        obj.observedTimestamp = observedTimestamp;
        obj.pose = CreatePoseHelper(poseStruct);
        _observedObjects.push_back(obj);
      }
    }

    Pose3d UiGameController::CreatePoseHelper(const PoseStruct3d& poseStruct)
    {
      if(!_poseOriginList.ContainsOriginID( poseStruct.originID ))
      {
        _poseOriginList.AddOriginWithID(poseStruct.originID);
      }

      Pose3d pose = Pose3d(poseStruct, _poseOriginList);
      return pose;
    }

    void UiGameController::HandlePingBase(const ExternalInterface::Ping& msg)
    {
      SendPing(true);
      HandlePing(msg);
    }

    void UiGameController::HandleRobotStateUpdateBase(const ExternalInterface::RobotState& msg)
    {
      _robotPose = CreatePoseHelper(msg.pose);
      _robotPose.SetName("RobotPose");

      // if localization has changed, update VizOrigin to the robot automatically
      // to better match the offsets
      const bool hasChangedLocalization =(_robotStateMsg.localizedToObjectID != msg.localizedToObjectID);
      if (hasChangedLocalization)
      {
        UpdateVizOriginToRobot();
      }

      _robotStateMsg = msg;

      HandleRobotStateUpdate(msg);
    }

    void UiGameController::HandleRobotDelocalizedBase(const ExternalInterface::RobotDelocalized& msg)
    {
      // the robot has delocalized, update VizOrigin to the robot automatically
      // (for example if we forceDeloc with a message)
      UpdateVizOriginToRobot();
    }

    void UiGameController::HandleRobotObservedObjectBase(const ExternalInterface::RobotObservedObject& msg)
    {
      AddOrUpdateObject(msg.objectID, msg.objectType, msg.pose, msg.timestamp, msg.isActive);

      HandleRobotObservedObject(msg);
    }

    void UiGameController::HandleRobotObservedFaceBase(const ExternalInterface::RobotObservedFace& msg)
    {
      _lastObservedFaceID = msg.faceID;

      HandleRobotObservedFace(msg);
    }

    void UiGameController::HandleRobotObservedPetBase(const ExternalInterface::RobotObservedPet& msg)
    {
      HandleRobotObservedPet(msg);
    }

    void UiGameController::HandleLoadedKnownFaceBase(const Vision::LoadedKnownFace& msg)
    {
      HandleLoadedKnownFace(msg);
    }

    void UiGameController::HandleCliffEventBase(const CliffEvent& msg)
    {
      HandleCliffEvent(msg);
    }

    void UiGameController::HandleSetCliffDetectThresholdsBase(const SetCliffDetectThresholds& msg)
    {
      HandleSetCliffDetectThresholds(msg);
    }

    void UiGameController::HandleEngineErrorCodeBase(const ExternalInterface::EngineErrorCodeMessage& msg)
    {
      HandleEngineErrorCode(msg);
    }

    void UiGameController::HandleRobotDeletedLocatedObjectBase(const ExternalInterface::RobotDeletedLocatedObject& msg)
    {
      PRINT_NAMED_INFO("UiGameController.HandleRobotDeletedObjectBase", "Robot reported deleting object %d", msg.objectID);

      _observedObjects.erase(std::remove_if(_observedObjects.begin(),
                                            _observedObjects.end(),
                                            [&msg](const ObservedObject& obj) {
                                              return (obj.id == msg.objectID);
                                            }),
                             _observedObjects.end());

      HandleRobotDeletedLocatedObject(msg);
    }

    void UiGameController::HandleUiDeviceAvailableBase(const ExternalInterface::UiDeviceAvailable& msgIn)
    {
      // Just send a message back to the game to connect to any UI device that's
      // advertising (since we don't have a selection mechanism here)
      PRINT_NAMED_INFO("UiGameController.HandleUiDeviceAvailableBase", "Sending message to command connection to %s device %d.",
                       EnumToString(msgIn.connectionType), msgIn.deviceID);
      ExternalInterface::ConnectToUiDevice msgOut(msgIn.connectionType, msgIn.deviceID);
      ExternalInterface::MessageGameToEngine message;
      message.Set_ConnectToUiDevice(msgOut);
      SendMessage(message);

      HandleUiDeviceAvailable(msgIn);
    }

    void UiGameController::HandleUiDeviceConnectedBase(const ExternalInterface::UiDeviceConnected& msg)
    {
      if (msg.connectionType == UiConnectionType::UI) {
        // Redirect Viz when connecting with Webots (which is a UI controller)
        webots::Field* redirectVizField = _root->getField("redirectViz");
        if (nullptr != redirectVizField) {
          if (redirectVizField->getSFBool()) {
            ExternalInterface::RedirectViz vizMsg;
            vizMsg.ipAddr = Util::UDPTransport::GetLocalIpAddress();
            ExternalInterface::MessageGameToEngine message;
            message.Set_RedirectViz(vizMsg);
            SendMessage(message);

            const uint8_t* ipBytes = (const uint8_t*)&vizMsg.ipAddr;
            PRINT_NAMED_INFO("UiGameController.Init.RedirectingViz",
                             "%u.%u.%u.%u",
                             ipBytes[0], ipBytes[1], ipBytes[2], ipBytes[3]);
          }
        }
      }

      HandleUiDeviceConnected(msg);
    }

    void UiGameController::HandleRobotConnectedBase(const ExternalInterface::RobotConnectionResponse& msg)
    {
      // Once robot connects, set resolution
      _firstRobotPoseUpdate = true;
      HandleRobotConnected(msg);

      if (msg.result == RobotConnectionResult::Success) {
        PRINT_NAMED_INFO("UiGameController.HandleRobotConnectedBase.ConnectSuccess", "");
      } else {
        PRINT_NAMED_WARNING("UiGameController.HandleRobotConnectedBase.ConnectFail", "* * * * * * %s * * * * * *", EnumToString(msg.result));
      }
    }

    void UiGameController::HandleRobotCompletedActionBase(const ExternalInterface::RobotCompletedAction& msg)
    {
      switch((RobotActionType)msg.actionType)
      {
        case RobotActionType::PICKUP_OBJECT_HIGH:
        case RobotActionType::PICKUP_OBJECT_LOW:
        {
          const ObjectInteractionCompleted info = msg.completionInfo.Get_objectInteractionCompleted();
          printf("Robot %s picking up object with ID: %d ",
                 ActionResultToString(msg.result),
                 info.objectID);
          printf("[Tag=%d]\n", msg.idTag);
        }
          break;

        case RobotActionType::PLACE_OBJECT_HIGH:
        case RobotActionType::PLACE_OBJECT_LOW:
        {
          const ObjectInteractionCompleted info = msg.completionInfo.Get_objectInteractionCompleted();
          printf("Robot %s placing object with ID: %d ",
                 ActionResultToString(msg.result),
                 info.objectID);
          printf("[Tag=%d]\n", msg.idTag);
        }
          break;

        case RobotActionType::PLAY_ANIMATION:
        {
          const AnimationCompleted info = msg.completionInfo.Get_animationCompleted();
          PRINT_NAMED_INFO("UiGameController.HandleRobotCompletedActionBase", "Robot finished playing animation %s with result %s. [Tag=%d]",
                 info.animationName.c_str(), ActionResultToString(msg.result), msg.idTag);
        }
          break;

        default:
        {
          PRINT_NAMED_INFO("UiGameController.HandleRobotCompletedActionBase", "Robot completed %s action with result %s [Tag=%d].",
                 EnumToString(msg.actionType), ActionResultToString(msg.result), msg.idTag);
        }
      }

      HandleRobotCompletedAction(msg);
    }

    // For processing image chunks arriving from robot.
    // Sends complete images to VizManager for visualization (and possible saving).
    void UiGameController::HandleImageChunkBase(const ImageChunk& msg)
    {
      HandleImageChunk(msg);
    } // HandleImageChunk()

    void UiGameController::HandleActiveObjectAccelBase(const ExternalInterface::ObjectAccel& msg)
    {
      //PRINT_NAMED_INFO("HandleActiveObjectAccel", "ObjectID %d, timestamp %d, accel {%.2f, %.2f, %.2f}",
      //                 msg.objectID, msg.timestamp, msg.accel.x, msg.accel.y, msg.accel.z);
      HandleActiveObjectAccel(msg);
    }

    void UiGameController::HandleActiveObjectAvailableBase(const ExternalInterface::ObjectAvailable& msg)
    {
      HandleActiveObjectAvailable(msg);
    }

    void UiGameController::HandleActiveObjectConnectionStateBase(const ExternalInterface::ObjectConnectionState& msg)
    {
      PRINT_NAMED_INFO("HandleActiveObjectConnectionState", "ObjectID %d (factoryID %s): %s",
                       msg.objectID, msg.factoryID.c_str(), msg.connected ? "CONNECTED" : "DISCONNECTED");
      HandleActiveObjectConnectionState(msg);
    }

    void UiGameController::HandleActiveObjectMovedBase(const ExternalInterface::ObjectMoved& msg)
    {
      PRINT_NAMED_INFO("HandleActiveObjectMovedWrapper", "Received message that object %d moved",
                       msg.objectID);

      HandleActiveObjectMoved(msg);
    }

    void UiGameController::HandleActiveObjectStoppedMovingBase(const ExternalInterface::ObjectStoppedMoving& msg)
    {
      PRINT_NAMED_INFO("HandleActiveObjectStoppedMoving", "Received message that object %d stopped moving",
                       msg.objectID);

      HandleActiveObjectStoppedMoving(msg);
    }

    void UiGameController::HandleActiveObjectTappedBase(const ExternalInterface::ObjectTapped& msg)
    {
      PRINT_NAMED_INFO("HandleActiveObjectTapped", "Received message that object %d was tapped.",
                       msg.objectID);

      HandleActiveObjectTapped(msg);
    }

    void UiGameController::HandleActiveObjectUpAxisChangedBase(const ExternalInterface::ObjectUpAxisChanged& msg)
    {
      PRINT_NAMED_INFO("HandleActiveObjectUpAxisChanged", "Received message that object %d's UpAxis has changed (new UpAxis = %s).",
                       msg.objectID, UpAxisToString(msg.upAxis));

      HandleActiveObjectUpAxisChanged(msg);
    }

    void UiGameController::HandleConnectedObjectStatesBase(const ExternalInterface::ConnectedObjectStates& msg)
    {
      for(auto & objectState : msg.objects)
      {
        PRINT_NAMED_INFO("HandleConnectedObjectStates",
                         "Received message about connected object %d (type: %s)",
                         objectState.objectID,
                         EnumToString(objectState.objectType));

        // TODO How do we visualize connected only objects since they don't have a pose?
      }

      HandleConnectedObjectStates(msg);
    }

    void UiGameController::HandleLocatedObjectStatesBase(const ExternalInterface::LocatedObjectStates& msg)
    {
      PRINT_NAMED_INFO("HandleObjectStates", "Clearing all objects before updating with %zu new objects",
                       msg.objects.size());

      _observedObjects.clear();

      for(const auto & objectState : msg.objects)
      {
        PRINT_NAMED_INFO("HandleLocatedObjectStates",
                         "Received message about known object %d (type: %s, poseState: %hhu)",
                         objectState.objectID,
                         EnumToString(objectState.objectType),
                         objectState.poseState);

        // observed timestamp of 0 indicates that we are not actually observing it here
        const uint32_t observedTimestamp = 0;

        AddOrUpdateObject(objectState.objectID,
                          objectState.objectType,
                          objectState.pose,
                          observedTimestamp,
                          objectState.isConnected);
      }

      HandleLocatedObjectStates(msg);
    }

    void UiGameController::HandleAnimationAvailableBase(const ExternalInterface::AnimationAvailable& msg)
    {
      PRINT_CH_INFO("Animations", "UiGameController.HandleAnimationAvailableBase.HandleAnimationAvailable",
                    "Animation available: %s", msg.animName.c_str());

      HandleAnimationAvailable(msg);
    }

    void UiGameController::HandleAnimationAbortedBase(const ExternalInterface::AnimationAborted& msg)
    {
      PRINT_NAMED_INFO("HandleAnimationAborted", "Tag: %u", msg.tag);

      HandleAnimationAborted(msg);
    }

    void UiGameController::HandleFactoryTestResultEntryBase(const FactoryTestResultEntry& msg)
    {
      PRINT_NAMED_INFO("HandleFactoryTestResultEntry",
                       "Test result: %s", EnumToString(msg.result));

      HandleFactoryTestResultEntry(msg);
    }


    void UiGameController::HandleEndOfMessageBase(const ExternalInterface::EndOfMessage& msg)
    {
      PRINT_NAMED_INFO("HandleEndOfMessage",
                       "messageType: %s", EnumToString(msg.messageType));

      HandleEndOfMessage(msg);
    }

    void UiGameController::HandleBehaviorTransitionBase(const ExternalInterface::BehaviorTransition& msg)
    {
      /**PRINT_NAMED_INFO("HandleBehaviorTransition", "Received message that behavior changed from %s to %s",
                       msg.oldBehaviorID,
                       msg.newBehaviorID);**/

      HandleBehaviorTransition(msg);
    }

    void UiGameController::HandleRobotOffTreadsStateChangedBase(const ExternalInterface::RobotOffTreadsStateChanged& msg)
    {
      PRINT_NAMED_INFO("HandleRobotOfftreadsStateChanged", "Received RobotPickedUp message.");
      HandleRobotOffTreadsStateChanged(msg);
      UpdateVizOriginToRobot();
    }

    void UiGameController::HandleDefinedCustomObjectBase(const ExternalInterface::DefinedCustomObject& msg)
    {
      HandleDefinedCustomObject(msg);
    }

    void UiGameController::HandleRobotDeletedAllCustomObjectsBase(const ExternalInterface::RobotDeletedAllCustomObjects& msg)
    {
      HandleRobotDeletedAllCustomObjects(msg);
    }

    void UiGameController::HandleRobotDeletedCustomMarkerObjectsBase(const ExternalInterface::RobotDeletedCustomMarkerObjects& msg)
    {
      HandleRobotDeletedCustomMarkerObjects(msg);
    }

    void UiGameController::HandleRobotDeletedFixedCustomObjectsBase(const ExternalInterface::RobotDeletedFixedCustomObjects& msg)
    {
      HandleRobotDeletedFixedCustomObjects(msg);
    }

    // ===== End of message handler callbacks ====


    UiGameController::UiGameController(s32 step_time_ms)
    : _webotsOrigin("WebotsOrigin")
    , _firstRobotPoseUpdate( true )
    {
      _stepTimeMS = step_time_ms;
      _robotNode = nullptr;
      _robotPose.SetTranslation({0.f, 0.f, 0.f});
      _robotPose.SetRotation(0, Z_AXIS_3D());
      _robotPoseActual.SetTranslation({0.f, 0.f, 0.f});
      _robotPoseActual.SetRotation(0, Z_AXIS_3D());
      _robotPoseActual.SetParent(_webotsOrigin);
    }

    UiGameController::~UiGameController()
    {
      if (_isStreamingImages) {
        SendImageRequest(ImageSendMode::Off);
      }

      if (_gameComms) {
        delete _gameComms;
      }
    }

    void UiGameController::Init()
    {
      // Setup the udp client for sending physics messages.
      _physicsControllerClient.Connect("127.0.0.1", (uint16_t)VizConstants::WEBOTS_PHYSICS_CONTROLLER_PORT);

      // Make root point to WebotsKeyBoardController node
      _root = _supervisor.getSelf();

      // Set deviceID
      // TODO: Get rid of this. The UI should not be assigning its own ID.
      int deviceID = 1;
      webots::Field* deviceIDField = _root->getField("deviceID");
      if (nullptr != deviceIDField) {
        deviceID = deviceIDField->getSFInt32();
      }

      // Get engine IP
      std::string engineIP = "127.0.0.1";
      webots::Field* engineIPField = _root->getField("engineIP");
      if (nullptr != engineIPField) {
        engineIP = engineIPField->getSFString();
      }

      // Get random seed
      webots::Field* randomSeedField = _root->getField("randomSeed");
      if (nullptr != randomSeedField) {
        _randomSeed = randomSeedField->getSFInt32();
      }

      // Get locale
      webots::Field* localeField = _root->getField("locale");
      if (nullptr != localeField) {
        _locale = localeField->getSFString();
      }

      // Startup comms with engine
      if (!_gameComms) {
        PRINT_NAMED_INFO("UiGameController.Init",
                          "Registering with advertising service at %s:%d",
                          engineIP.c_str(),
                          UI_ADVERTISEMENT_REGISTRATION_PORT);
        _gameComms = new GameComms(deviceID, UI_MESSAGE_SERVER_LISTEN_PORT,
                                   engineIP.c_str(),
                                   UI_ADVERTISEMENT_REGISTRATION_PORT);
      }


      while(!_gameComms->IsInitialized()) {
        PRINT_NAMED_INFO("UiGameController.Init",
                         "Waiting for gameComms to initialize...");
        _supervisor.step(_stepTimeMS);
        _gameComms->Update();
      }
      _msgHandler.Init(_gameComms);


      // Register callbacks for incoming messages from game
      // TODO: Have CLAD generate this?
      _msgHandler.RegisterCallbackForMessage([this](const ExternalInterface::MessageEngineToGame& message) {
        switch (message.GetTag()) {
          case ExternalInterface::MessageEngineToGame::Tag::RobotConnectionResponse:
            HandleRobotConnectedBase(message.Get_RobotConnectionResponse());
            break;
          case ExternalInterface::MessageEngineToGame::Tag::Ping:
            HandlePingBase(message.Get_Ping());
            break;
          case ExternalInterface::MessageEngineToGame::Tag::RobotState:
            HandleRobotStateUpdateBase(message.Get_RobotState());
            break;
          case ExternalInterface::MessageEngineToGame::Tag::RobotDelocalized:
            HandleRobotDelocalizedBase(message.Get_RobotDelocalized());
            break;
          case ExternalInterface::MessageEngineToGame::Tag::RobotObservedObject:
            HandleRobotObservedObjectBase(message.Get_RobotObservedObject());
            break;
          case ExternalInterface::MessageEngineToGame::Tag::RobotObservedFace:
            HandleRobotObservedFaceBase(message.Get_RobotObservedFace());
            break;
          case ExternalInterface::MessageEngineToGame::Tag::RobotObservedPet:
            HandleRobotObservedPetBase(message.Get_RobotObservedPet());
            break;
          case ExternalInterface::MessageEngineToGame::Tag::UiDeviceAvailable:
            HandleUiDeviceAvailableBase(message.Get_UiDeviceAvailable());
            break;
          case ExternalInterface::MessageEngineToGame::Tag::UiDeviceConnected:
            HandleUiDeviceConnectedBase(message.Get_UiDeviceConnected());
            break;
          case ExternalInterface::MessageEngineToGame::Tag::ImageChunk:
            HandleImageChunkBase(message.Get_ImageChunk());
            break;
          case ExternalInterface::MessageEngineToGame::Tag::RobotDeletedLocatedObject:
            HandleRobotDeletedLocatedObjectBase(message.Get_RobotDeletedLocatedObject());
            break;
          case ExternalInterface::MessageEngineToGame::Tag::RobotCompletedAction:
            HandleRobotCompletedActionBase(message.Get_RobotCompletedAction());
            break;
          case ExternalInterface::MessageEngineToGame::Tag::ObjectAccel:
            HandleActiveObjectAccelBase(message.Get_ObjectAccel());
            break;
          case ExternalInterface::MessageEngineToGame::Tag::ObjectAvailable:
            HandleActiveObjectAvailableBase(message.Get_ObjectAvailable());
            break;
          case ExternalInterface::MessageEngineToGame::Tag::ObjectConnectionState:
            HandleActiveObjectConnectionStateBase(message.Get_ObjectConnectionState());
            break;
          case ExternalInterface::MessageEngineToGame::Tag::ObjectMoved:
            HandleActiveObjectMovedBase(message.Get_ObjectMoved());
            break;
          case ExternalInterface::MessageEngineToGame::Tag::ObjectStoppedMoving:
            HandleActiveObjectStoppedMovingBase(message.Get_ObjectStoppedMoving());
            break;
          case ExternalInterface::MessageEngineToGame::Tag::ObjectTapped:
            HandleActiveObjectTappedBase(message.Get_ObjectTapped());
            break;
          case ExternalInterface::MessageEngineToGame::Tag::ObjectUpAxisChanged:
            HandleActiveObjectUpAxisChangedBase(message.Get_ObjectUpAxisChanged());
            break;
          case ExternalInterface::MessageEngineToGame::Tag::ConnectedObjectStates:
            HandleConnectedObjectStatesBase(message.Get_ConnectedObjectStates());
            break;
          case ExternalInterface::MessageEngineToGame::Tag::LocatedObjectStates:
            HandleLocatedObjectStatesBase(message.Get_LocatedObjectStates());
            break;
          case ExternalInterface::MessageEngineToGame::Tag::AnimationAvailable:
            HandleAnimationAvailableBase(message.Get_AnimationAvailable());
            break;
          case ExternalInterface::MessageEngineToGame::Tag::AnimationAborted:
            HandleAnimationAbortedBase(message.Get_AnimationAborted());
            break;
          case ExternalInterface::MessageEngineToGame::Tag::EndOfMessage:
            HandleEndOfMessageBase(message.Get_EndOfMessage());
            break;
          case ExternalInterface::MessageEngineToGameTag::BehaviorTransition:
            HandleBehaviorTransitionBase(message.Get_BehaviorTransition());
            break;
          case ExternalInterface::MessageEngineToGameTag::RobotOffTreadsStateChanged:
            HandleRobotOffTreadsStateChangedBase(message.Get_RobotOffTreadsStateChanged());
            break;
          case ExternalInterface::MessageEngineToGame::Tag::FactoryTestResultEntry:
            HandleFactoryTestResultEntryBase(message.Get_FactoryTestResultEntry());
            break;
          case ExternalInterface::MessageEngineToGameTag::LoadedKnownFace:
            HandleLoadedKnownFaceBase(message.Get_LoadedKnownFace());
            break;
          case ExternalInterface::MessageEngineToGameTag::EngineErrorCodeMessage:
            HandleEngineErrorCodeBase(message.Get_EngineErrorCodeMessage());
            break;
          case ExternalInterface::MessageEngineToGameTag::CliffEvent:
            HandleCliffEventBase(message.Get_CliffEvent());
            break;
          case ExternalInterface::MessageEngineToGameTag::SetCliffDetectThresholds:
            HandleSetCliffDetectThresholdsBase(message.Get_SetCliffDetectThresholds());
            break;
          case ExternalInterface::MessageEngineToGameTag::DefinedCustomObject:
            HandleDefinedCustomObjectBase(message.Get_DefinedCustomObject());
            break;
          case ExternalInterface::MessageEngineToGameTag::RobotDeletedAllCustomObjects:
            HandleRobotDeletedAllCustomObjectsBase(message.Get_RobotDeletedAllCustomObjects());
            break;
          case ExternalInterface::MessageEngineToGameTag::RobotDeletedCustomMarkerObjects:
            HandleRobotDeletedCustomMarkerObjectsBase(message.Get_RobotDeletedCustomMarkerObjects());
            break;
          case ExternalInterface::MessageEngineToGameTag::RobotDeletedFixedCustomObjects:
            HandleRobotDeletedFixedCustomObjectsBase(message.Get_RobotDeletedFixedCustomObjects());
            break;
          default:
            // ignore
            break;
        }
      });

      _uiState = UI_WAITING_FOR_GAME;

      InitInternal();
    }

    s32 UiGameController::Update()
    {
      s32 res = 0;

      if (_supervisor.step(_stepTimeMS) == -1) {
        PRINT_NAMED_INFO("UiGameController.Update.StepFailed", "");
        return -1;
      }

      _gameComms->Update();

      switch(_uiState) {
        case UI_WAITING_FOR_GAME:
        {
          if (!_gameComms->HasClient()) {
            return 0;
          } else {
            _uiState = UI_RUNNING;

            // Call step() here with a large-ish time to give engine time to initialize before calling OnEngineLoaded()
            const int timeToWait_ms = 2000;
            _supervisor.step(timeToWait_ms);
            OnEngineLoaded();
          }
          break;
        }

        case UI_RUNNING:
        {
          UpdateActualObjectPoses();

          _msgHandler.ProcessMessages();

          res = UpdateInternal();

          break;
        }

        default:
          PRINT_NAMED_ERROR("UiGameController.Update", "Reached default switch case.");

      } // switch(_uiState)

      return res;
    }

    void UiGameController::OnEngineLoaded()
    {
      // Set Render Enable in Map Component
      ExternalInterface::SetMemoryMapRenderEnabled m;
      m.enabled = true;
      ExternalInterface::MessageGameToEngine message(std::move(m));
      SendMessage(message);
    }

    void UiGameController::UpdateActualObjectPoses()
    {
      // Only look for the robot node once at the beginning
      if (_robotNode == nullptr) {
        auto cozmoBotNodeInfo = WebotsHelpers::GetFirstMatchingSceneTreeNode(GetSupervisor(), "CozmoBot");
        if (cozmoBotNodeInfo.nodePtr == nullptr) {
          // If there's no Vector, look for a Whiskey
          cozmoBotNodeInfo = WebotsHelpers::GetFirstMatchingSceneTreeNode(GetSupervisor(), "WhiskeyBot");
        }


        DEV_ASSERT(cozmoBotNodeInfo.nodePtr != nullptr, "UiGameController.UpdateActualObjectPoses.NoCozmoBot");
        DEV_ASSERT(cozmoBotNodeInfo.type == webots::Node::ROBOT, "UiGameController.UpdateActualObjectPoses.CozmoBotNotSupervisor");

        PRINT_NAMED_INFO("UiGameController.UpdateActualObjectPoses",
                         "Found robot with name %s", cozmoBotNodeInfo.typeName.c_str());
        _robotNode = cozmoBotNodeInfo.nodePtr;

        // Find any LightCube nodes in the world
        const auto& lightCubes = WebotsHelpers::GetMatchingSceneTreeNodes(GetSupervisor(), "LightCube");

        for (const auto& lightCubeNodeInfo : lightCubes) {
          _lightCubes.emplace_back(lightCubeNodeInfo.nodePtr);
          _lightCubeOriginIter = _lightCubes.begin();

          PRINT_NAMED_INFO("UiGameController.UpdateActualObjectPoses",
                           "Found LightCube with name %s", lightCubeNodeInfo.typeName.c_str());
        }
      }

      _robotPoseActual = GetPose3dOfNode(_robotNode);
      _robotPoseActual.SetName("RobotPoseActual");

      // if it's the first time that we set the proper pose for the robot, update the visualization origin to
      // the robot, since debug render expects to be centered around the robot
      if ( _firstRobotPoseUpdate )
      {
        PRINT_NAMED_INFO("UiGameController.UpdateVizOrigin",
                         "Auto aligning viz to match robot's pose. %f %f %f",
                         _robotPoseActual.GetTranslation().x(), _robotPoseActual.GetTranslation().y(), _robotPoseActual.GetTranslation().z());

        Pose3d initialWorldPose = _robotPoseActual * _robotPose.GetInverse();
        UpdateVizOrigin(initialWorldPose);
        _firstRobotPoseUpdate = false;
      }
    }

    void UiGameController::CycleVizOrigin()
    {
      auto UpdateVizOriginToRobotAndLog = [this]() {
        LOG_INFO("UiGameController.UpdateVizOrigin", "Aligning viz to match robot's pose.");
        UpdateVizOriginToRobot();
      };

      Pose3d correctionPose;
      if (_robotStateMsg.localizedToObjectID >= 0 && !_lightCubes.empty()) {
        // Cycle through the _lightCubes vector
        if (_lightCubeOriginIter == _lightCubes.end()) {
          _lightCubeOriginIter = _lightCubes.begin();
        } else {
          ++_lightCubeOriginIter;
        }

        if (_lightCubeOriginIter != _lightCubes.end()) {
          // If we haven't iterated through all the observed light cubes yet, localize to the newly
          // iterated light cube.
          LOG_INFO("UiGameController.UpdateVizOrigin",
                   "Aligning viz to match next known LightCube to object %d",
                   _robotStateMsg.localizedToObjectID);

          correctionPose = GetPose3dOfNode(*_lightCubeOriginIter)  * GetObjectPoseMap()[_robotStateMsg.localizedToObjectID].GetInverse();
          UpdateVizOrigin(correctionPose);
        } else {
          // We have cycled through all the available light cubes, so localize to robot now.
          UpdateVizOriginToRobotAndLog();
        }
      } else {
        // Robot haven't observed any cubes, so localize to robot.
        UpdateVizOriginToRobotAndLog();
      }
    }

    void UiGameController::UpdateVizOriginToRobot()
    {
      // set iterator to end
      _lightCubeOriginIter = _lightCubes.end();

      Pose3d correctionPose = _robotPoseActual * _robotPose.GetInverse();
      UpdateVizOrigin(correctionPose);
    }

    void UiGameController::UpdateVizOrigin(const Pose3d& originPose)
    {
      SetVizOrigin msg;
      const RotationVector3d Rvec(originPose.GetRotationVector());

      msg.rot_rad = Rvec.GetAngle().ToFloat();
      msg.rot_axis_x = Rvec.GetAxis().x();
      msg.rot_axis_y = Rvec.GetAxis().y();
      msg.rot_axis_z = Rvec.GetAxis().z();

      msg.trans_x_mm = originPose.GetTranslation().x();
      msg.trans_y_mm = originPose.GetTranslation().y();
      msg.trans_z_mm = originPose.GetTranslation().z();

      SendMessage(ExternalInterface::MessageGameToEngine(std::move(msg)));
    }

    void UiGameController::SetDataPlatform(const Util::Data::DataPlatform* dataPlatform) {
      _dataPlatform = dataPlatform;
    }

    const Util::Data::DataPlatform* UiGameController::GetDataPlatform() const
    {
      return _dataPlatform;
    }

    Result UiGameController::SendMessage(const ExternalInterface::MessageGameToEngine& msg)
    {
      UserDeviceID_t devID = 1; // TODO: Should this be a RobotID_t?
      const Result res = _msgHandler.SendMessage(devID, msg);
      if(res != RESULT_OK)
      {
        PRINT_NAMED_ERROR("UiGameController.SendMessage.Fail",
                          "Failed to send message %u with result %d",
                          (u32)msg.GetTag(),
                          res);
      }
      return res;
    }



    void UiGameController::SendPing(bool isResponse)
    {
      static ExternalInterface::Ping m;
      m.isResponse = isResponse;
      ExternalInterface::MessageGameToEngine message;
      message.Set_Ping(m);
      SendMessage(message);

      ++m.counter;
    }

    void UiGameController::SendDriveWheels(const f32 lwheel_speed_mmps, const f32 rwheel_speed_mmps, const f32 lwheel_accel_mmps2, const f32 rwheel_accel_mmps2)
    {
      ExternalInterface::DriveWheels m;
      m.lwheel_speed_mmps = lwheel_speed_mmps;
      m.rwheel_speed_mmps = rwheel_speed_mmps;
      m.lwheel_accel_mmps2 = lwheel_accel_mmps2;
      m.rwheel_accel_mmps2 = rwheel_accel_mmps2;
      ExternalInterface::MessageGameToEngine message;
      message.Set_DriveWheels(m);
      SendMessage(message);
    }

    void UiGameController::SendDriveArc(const f32 speed, const f32 accel, const s16 curvature_mm)
    {
      ExternalInterface::DriveArc m;
      m.speed = speed;
      m.accel = accel;
      m.curvatureRadius_mm = curvature_mm;
      ExternalInterface::MessageGameToEngine message;
      message.Set_DriveArc(m);
      SendMessage(message);
    }



    void UiGameController::SendDriveStraight(f32 speed_mmps, f32 dist_mm, bool shouldPlayAnimation)
    {
      ExternalInterface::DriveStraight m;
      m.speed_mmps = speed_mmps;
      m.dist_mm = dist_mm;
      m.shouldPlayAnimation = shouldPlayAnimation;
      ExternalInterface::MessageGameToEngine message;
      message.Set_DriveStraight(m);
      SendMessage(message);
    }

    uint32_t UiGameController::SendTurnInPlace(const f32 angle_rad,
                                               const f32 speed_radPerSec,
                                               const f32 accel_radPerSec2,
                                               const f32 tol_rad,
                                               const bool isAbsolute,
                                               const QueueActionPosition queueActionPosition)
    {
      ExternalInterface::QueueSingleAction m;
      m.idTag = ++_queueActionIdTag;
      m.position = queueActionPosition;
      m.numRetries = 1;
      m.action.Set_turnInPlace(ExternalInterface::TurnInPlace( angle_rad, speed_radPerSec, accel_radPerSec2, tol_rad, isAbsolute ));
      ExternalInterface::MessageGameToEngine message;
      message.Set_QueueSingleAction(m);
      SendMessage(message);
      return m.idTag;
    }

    void UiGameController::SendAction(const ExternalInterface::QueueSingleAction& msg_in)
    {
      ExternalInterface::QueueSingleAction m(msg_in);
      m.idTag = ++_queueActionIdTag;
      m.position = QueueActionPosition::NOW;
      m.numRetries = 1;
      ExternalInterface::MessageGameToEngine message;
      message.Set_QueueSingleAction(m);
      SendMessage(message);
    }

    void UiGameController::SendTurnInPlaceAtSpeed(const f32 speed_rad_per_sec, const f32 accel_rad_per_sec2)
    {
      ExternalInterface::TurnInPlaceAtSpeed m;
      m.speed_rad_per_sec = speed_rad_per_sec;
      m.accel_rad_per_sec2 = accel_rad_per_sec2;
      ExternalInterface::MessageGameToEngine message;
      message.Set_TurnInPlaceAtSpeed(m);
      SendMessage(message);
    }

    void UiGameController::SendMoveHead(const f32 speed_rad_per_sec)
    {
      ExternalInterface::MoveHead m;
      m.speed_rad_per_sec = speed_rad_per_sec;
      ExternalInterface::MessageGameToEngine message;
      message.Set_MoveHead(m);
      SendMessage(message);
    }

    void UiGameController::SendMoveLift(const f32 speed_rad_per_sec)
    {
      ExternalInterface::MoveLift m;
      m.speed_rad_per_sec = speed_rad_per_sec;
      ExternalInterface::MessageGameToEngine message;
      message.Set_MoveLift(m);
      SendMessage(message);
    }

    void UiGameController::SendMoveHeadToAngle(const f32 rad, const f32 speed, const f32 accel, const f32 duration_sec)
    {
      ExternalInterface::SetHeadAngle m;
      m.angle_rad = rad;
      m.max_speed_rad_per_sec = speed;
      m.accel_rad_per_sec2 = accel;
      m.duration_sec = duration_sec;
      ExternalInterface::MessageGameToEngine message;
      message.Set_SetHeadAngle(m);
      SendMessage(message);
    }

    void UiGameController::SendMoveLiftToHeight(const f32 mm, const f32 speed, const f32 accel, const f32 duration_sec)
    {
      ExternalInterface::SetLiftHeight m;
      m.height_mm = mm;
      m.max_speed_rad_per_sec = speed;
      m.accel_rad_per_sec2 = accel;
      m.duration_sec = duration_sec;
      ExternalInterface::MessageGameToEngine message;
      message.Set_SetLiftHeight(m);
      SendMessage(message);
    }

    void UiGameController::SendMoveLiftToAngle(const f32 angle_rad, const f32 speed, const f32 accel, const f32 duration_sec)
    {
      ExternalInterface::SetLiftAngle m;
      m.angle_rad = angle_rad,
      m.max_speed_rad_per_sec = speed;
      m.accel_rad_per_sec2 = accel;
      m.duration_sec = duration_sec;
      ExternalInterface::MessageGameToEngine message;
      message.Set_SetLiftAngle(m);
      SendMessage(message);
    }

    void UiGameController::SendEnableLiftPower(bool enable)
    {
      ExternalInterface::EnableLiftPower m;
      m.enable = enable;
      ExternalInterface::MessageGameToEngine message;
      message.Set_EnableLiftPower(m);
      SendMessage(message);
    }

    void UiGameController::SendStopAllMotors()
    {
      ExternalInterface::StopAllMotors m;
      ExternalInterface::MessageGameToEngine message;
      message.Set_StopAllMotors(m);
      SendMessage(message);
    }

    void UiGameController::SendImageRequest(ImageSendMode mode)
    {
      ExternalInterface::ImageRequest m;
      m.mode = mode;
      ExternalInterface::MessageGameToEngine message;
      message.Set_ImageRequest(m);
      SendMessage(message);

      _isStreamingImages = (mode == ImageSendMode::Stream);
    }

    void UiGameController::SendSaveImages(ImageSendMode imageMode, const std::string& path, const int8_t qualityOnRobot,
                                          const bool removeRadialDistortion)
    {
      using namespace ExternalInterface;
      SendMessage(MessageGameToEngine(SaveImages(imageMode, qualityOnRobot, removeRadialDistortion, path)));
    }

    void UiGameController::SendSaveState(bool enabled, const std::string& path)
    {
      using namespace ExternalInterface;
      SendMessage(MessageGameToEngine(SaveRobotState(enabled, path)));
    }

    void UiGameController::SendEnableDisplay(bool on)
    {
      ExternalInterface::EnableDisplay m;
      m.enable = on;
      ExternalInterface::MessageGameToEngine message;
      message.Set_EnableDisplay(m);
      SendMessage(message);
   }

    void UiGameController::SendExecutePathToPose(const Pose3d& p,
                                                 PathMotionProfile motionProf)
    {
      ExternalInterface::GotoPose m;
      m.x_mm = p.GetTranslation().x();
      m.y_mm = p.GetTranslation().y();
      m.rad = p.GetRotationAngle<'Z'>().ToFloat();
      m.motionProf = motionProf;
      m.level = 0;
      ExternalInterface::MessageGameToEngine message;
      message.Set_GotoPose(m);
      SendMessage(message);
    }

    void UiGameController::SendGotoObject(const s32 objectID,
                                          const f32 distFromObjectOrigin_mm,
                                          PathMotionProfile motionProf,
                                          const bool usePreDockPose)
    {
      ExternalInterface::GotoObject msg;
      msg.objectID = objectID;
      msg.distanceFromObjectOrigin_mm = distFromObjectOrigin_mm;
      msg.motionProf = motionProf;
      msg.usePreDockPose = usePreDockPose;

      ExternalInterface::MessageGameToEngine msgWrapper;
      msgWrapper.Set_GotoObject(msg);
      SendMessage(msgWrapper);
    }

    void UiGameController::SendAlignWithObject(const s32 objectID,
                                               const f32 distFromMarker_mm,
                                               PathMotionProfile motionProf,
                                               const bool usePreDockPose,
                                               const bool useApproachAngle,
                                               const f32 approachAngle_rad)
    {
      ExternalInterface::AlignWithObject msg;
      msg.objectID = objectID;
      msg.distanceFromMarker_mm = distFromMarker_mm;
      msg.motionProf = motionProf;
      msg.useApproachAngle = useApproachAngle;
      msg.approachAngle_rad = approachAngle_rad;
      msg.usePreDockPose = usePreDockPose;
      msg.alignmentType = AlignmentType::CUSTOM;

      ExternalInterface::MessageGameToEngine msgWrapper;
      msgWrapper.Set_AlignWithObject(msg);
      SendMessage(msgWrapper);
    }


    void UiGameController::SendPlaceObjectOnGroundSequence(const Pose3d& p,
                                                           PathMotionProfile motionProf,
                                                           const bool useExactRotation)
    {
      ExternalInterface::PlaceObjectOnGround m;
      m.x_mm = p.GetTranslation().x();
      m.y_mm = p.GetTranslation().y();
      m.level = 0;
      UnitQuaternion q(p.GetRotation().GetQuaternion());
      m.qw = q.w();
      m.qx = q.x();
      m.qy = q.y();
      m.qz = q.z();
      m.motionProf = motionProf;
      m.useExactRotation = useExactRotation;
      ExternalInterface::MessageGameToEngine message;
      message.Set_PlaceObjectOnGround(m);
      SendMessage(message);
    }


    void UiGameController::SendTrackToObject(const u32 objectID, bool headOnly)
    {
      ExternalInterface::TrackToObject m;
      m.objectID = objectID;
      m.headOnly = headOnly;
      m.moveEyes = false;

      ExternalInterface::MessageGameToEngine message;
      message.Set_TrackToObject(m);
      SendMessage(message);
    }

    void UiGameController::SendTrackToFace(const u32 faceID, bool headOnly)
    {
      ExternalInterface::TrackToFace m;
      m.faceID = faceID;
      m.headOnly = headOnly;
      m.moveEyes = false;

      ExternalInterface::MessageGameToEngine message;
      message.Set_TrackToFace(m);
      SendMessage(message);
    }


    void UiGameController::SendExecuteTestPlan(PathMotionProfile motionProf)
    {
      ExternalInterface::ExecuteTestPlan m;
      m.motionProf = motionProf;
      ExternalInterface::MessageGameToEngine message;
      message.Set_ExecuteTestPlan(m);
      SendMessage(message);
    }

    void UiGameController::SendFakeTriggerWordDetect()
    {
      SendMessage(ExternalInterface::MessageGameToEngine(ExternalInterface::FakeTriggerWordDetected()));
    }

    void UiGameController::SendForceDelocalize()
    {
      ExternalInterface::ForceDelocalizeRobot delocMsg;
      SendMessage(ExternalInterface::MessageGameToEngine(std::move(delocMsg)));
    }

    void UiGameController::SendSelectNextObject()
    {
      ExternalInterface::SelectNextObject m;
      ExternalInterface::MessageGameToEngine message;
      message.Set_SelectNextObject(m);
      SendMessage(message);
    }

    void UiGameController::SendPickupObject(const s32 objectID,
                                            PathMotionProfile motionProf,
                                            const bool usePreDockPose,
                                            const bool useApproachAngle,
                                            const f32 approachAngle_rad)
    {
      ExternalInterface::PickupObject m;
      m.objectID = objectID,
      m.motionProf = motionProf;
      m.usePreDockPose = usePreDockPose;
      m.useApproachAngle = useApproachAngle;
      m.approachAngle_rad = approachAngle_rad;
      ExternalInterface::MessageGameToEngine message;
      message.Set_PickupObject(m);
      SendMessage(message);
    }


    void UiGameController::SendPlaceOnObject(const s32 objectID,
                                             PathMotionProfile motionProf,
                                             const bool usePreDockPose,
                                             const bool useApproachAngle,
                                             const f32 approachAngle_rad)
    {
      ExternalInterface::PlaceOnObject m;
      m.objectID = objectID,
      m.motionProf = motionProf;
      m.usePreDockPose = usePreDockPose;
      m.useApproachAngle = useApproachAngle;
      m.approachAngle_rad = approachAngle_rad;
      ExternalInterface::MessageGameToEngine message;
      message.Set_PlaceOnObject(m);
      SendMessage(message);
    }

    void UiGameController::SendPlaceRelObject(const s32 objectID,
                                              PathMotionProfile motionProf,
                                              const bool usePreDockPose,
                                              const f32 placementOffsetX_mm,
                                              const bool useApproachAngle,
                                              const f32 approachAngle_rad)
    {
      ExternalInterface::PlaceRelObject m;
      m.objectID = objectID,
      m.motionProf = motionProf;
      m.usePreDockPose = usePreDockPose;
      m.placementOffsetX_mm = placementOffsetX_mm;
      m.useApproachAngle = useApproachAngle;
      m.approachAngle_rad = approachAngle_rad;
      ExternalInterface::MessageGameToEngine message;
      message.Set_PlaceRelObject(m);
      SendMessage(message);
    }

    void UiGameController::SendPickupSelectedObject(PathMotionProfile motionProf,
                                                    const bool usePreDockPose,
                                                    const bool useApproachAngle,
                                                    const f32 approachAngle_rad)
    {
      SendPickupObject(-1,
                       motionProf,
                       usePreDockPose,
                       useApproachAngle,
                       approachAngle_rad);
    }


    void UiGameController::SendPlaceOnSelectedObject(PathMotionProfile motionProf,
                                                     const bool usePreDockPose,
                                                     const bool useApproachAngle,
                                                     const f32 approachAngle_rad)
    {
      SendPlaceOnObject(-1,
                        motionProf,
                        usePreDockPose,
                        useApproachAngle,
                        approachAngle_rad);
    }

    void UiGameController::SendPlaceRelSelectedObject(PathMotionProfile motionProf,
                                                      const bool usePreDockPose,
                                                      const f32 placementOffsetX_mm,
                                                      const bool useApproachAngle,
                                                      const f32 approachAngle_rad)
    {
      SendPlaceRelObject(-1,
                         motionProf,
                         usePreDockPose,
                         placementOffsetX_mm,
                         useApproachAngle,
                         approachAngle_rad);
    }



    void UiGameController::SendRollObject(const s32 objectID,
                                          PathMotionProfile motionProf,
                                          const bool doDeepRoll,
                                          const bool usePreDockPose,
                                          const bool useApproachAngle,
                                          const f32 approachAngle_rad)
    {
      ExternalInterface::RollObject m;
      m.motionProf = motionProf;
      m.doDeepRoll = doDeepRoll;
      m.usePreDockPose = usePreDockPose;
      m.useApproachAngle = useApproachAngle,
      m.approachAngle_rad = approachAngle_rad,
      m.objectID = -1;
      ExternalInterface::MessageGameToEngine message;
      message.Set_RollObject(m);
      SendMessage(message);
    }

    void UiGameController::SendRollSelectedObject(PathMotionProfile motionProf,
                                                  const bool doDeepRoll,
                                                  const bool usePreDockPose,
                                                  const bool useApproachAngle,
                                                  const f32 approachAngle_rad)
    {
      SendRollObject(-1,
                     motionProf,
                     doDeepRoll,
                     usePreDockPose,
                     useApproachAngle,
                     approachAngle_rad);
    }

    void UiGameController::SendPopAWheelie(const s32 objectID,
                                           PathMotionProfile motionProf,
                                           const bool usePreDockPose,
                                           const bool useApproachAngle,
                                           const f32 approachAngle_rad)
    {
      ExternalInterface::PopAWheelie m;
      m.motionProf = motionProf;
      m.usePreDockPose = usePreDockPose;
      m.useApproachAngle = useApproachAngle,
      m.approachAngle_rad = approachAngle_rad,
      m.objectID = -1;
      ExternalInterface::MessageGameToEngine message;
      message.Set_PopAWheelie(m);
      SendMessage(message);
    }

    void UiGameController::SendFacePlant(const s32 objectID,
                                         PathMotionProfile motionProf,
                                         const bool usePreDockPose,
                                         const bool useApproachAngle,
                                         const f32 approachAngle_rad)
    {
      ExternalInterface::FacePlant m;
      m.motionProf = motionProf;
      m.usePreDockPose = usePreDockPose;
      m.useApproachAngle = useApproachAngle,
      m.approachAngle_rad = approachAngle_rad,
      m.objectID = -1;
      ExternalInterface::MessageGameToEngine message;
      message.Set_FacePlant(m);
      SendMessage(message);
    }

    void UiGameController::SendMountCharger(s32 objectID,
                                            PathMotionProfile motionProf,
                                            const bool useCliffSensorCorrection)
    {
      ExternalInterface::MountCharger m;
      m.objectID = objectID;
      m.motionProf = motionProf;
      m.useCliffSensorCorrection = useCliffSensorCorrection;
      ExternalInterface::MessageGameToEngine message;
      message.Set_MountCharger(m);
      SendMessage(message);
    }


    void UiGameController::SendMountSelectedCharger(PathMotionProfile motionProf,
                                                    const bool useCliffSensorCorrection)
    {
      SendMountCharger(-1, motionProf, useCliffSensorCorrection);
    }

    BehaviorClass UiGameController::GetBehaviorClass(const std::string& behaviorClass) const
    {
      return BehaviorTypesWrapper::BehaviorClassFromString(behaviorClass);
    }

    void UiGameController::SendAbortPath()
    {
      ExternalInterface::AbortPath m;
      ExternalInterface::MessageGameToEngine message;
      message.Set_AbortPath(m);
      SendMessage(message);
    }

    void UiGameController::SendAbortAll()
    {
      ExternalInterface::AbortAll m;
      ExternalInterface::MessageGameToEngine message;
      message.Set_AbortAll(m);
      SendMessage(message);
    }

    void UiGameController::SendDrawPoseMarker(const Pose3d& p)
    {
      ExternalInterface::DrawPoseMarker m;
      m.x_mm = p.GetTranslation().x();
      m.y_mm = p.GetTranslation().y();
      m.rad = p.GetRotationAngle<'Z'>().ToFloat();
      m.level = 0;
      ExternalInterface::MessageGameToEngine message;
      message.Set_DrawPoseMarker(m);
      SendMessage(message);
    }

    void UiGameController::SendErasePoseMarker()
    {
      ExternalInterface::ErasePoseMarker m;
      ExternalInterface::MessageGameToEngine message;
      message.Set_ErasePoseMarker(m);
      SendMessage(message);
    }

    void UiGameController::SendControllerGains(ControllerChannel channel, f32 kp, f32 ki, f32 kd, f32 maxErrorSum)
    {
      ExternalInterface::ControllerGains m;
      m.controller = channel;
      m.kp = kp;
      m.ki = ki;
      m.kd = kd;
      m.maxIntegralError = maxErrorSum;
      ExternalInterface::MessageGameToEngine message;
      message.Set_ControllerGains(m);
      SendMessage(message);
    }

    void UiGameController::SendRollActionParams(f32 liftHeight_mm, f32 driveSpeed_mmps, f32 driveAccel_mmps2, u32 driveDuration_ms, f32 backupDist_mm)
    {
      ExternalInterface::RollActionParams m;
      m.liftHeight_mm = liftHeight_mm;
      m.driveSpeed_mmps = driveSpeed_mmps;
      m.driveAccel_mmps2 = driveAccel_mmps2;
      m.driveDuration_ms = driveDuration_ms;
      m.backupDist_mm = backupDist_mm;
      ExternalInterface::MessageGameToEngine message;
      message.Set_RollActionParams(m);
      SendMessage(message);
    }

    void UiGameController::SendSetRobotVolume(const f32 volume)
    {
      ExternalInterface::SetRobotVolume m;
      m.volume = volume;
      ExternalInterface::MessageGameToEngine message;
      message.Set_SetRobotVolume(m);
      SendMessage(message);
    }

    void UiGameController::SendStartTestMode(TestMode mode, s32 p1, s32 p2, s32 p3)
    {
      ExternalInterface::StartTestMode m;
      m.mode = mode;
      m.p1 = p1;
      m.p2 = p2;
      m.p3 = p3;
      ExternalInterface::MessageGameToEngine message;
      message.Set_StartTestMode(m);
      SendMessage(message);
    }

    void UiGameController::SendIMURequest(u32 length_ms)
    {
      IMURequest m;
      m.length_ms = length_ms;
      ExternalInterface::MessageGameToEngine message;
      message.Set_IMURequest(m);
      SendMessage(message);
    }

    void UiGameController::SendLogCliffDataRequest(const u32 length_ms)
    {
      ExternalInterface::LogRawCliffData m;
      m.length_ms = length_ms;
      ExternalInterface::MessageGameToEngine message;
      message.Set_LogRawCliffData(m);
      SendMessage(message);
    }

    void UiGameController::SendLogProxDataRequest(const u32 length_ms)
    {
      ExternalInterface::LogRawProxData m;
      m.length_ms = length_ms;
      ExternalInterface::MessageGameToEngine message;
      message.Set_LogRawProxData(m);
      SendMessage(message);
    }

    void UiGameController::SendCubeAnimation(const u32 objectID, const CubeAnimationTrigger cubeAnimTrigger)
    {
      ExternalInterface::PlayCubeAnim m;
      m.objectID = objectID;
      m.trigger = cubeAnimTrigger;
      SendMessage(ExternalInterface::MessageGameToEngine(std::move(m)));
    }

    void UiGameController::SendStopCubeAnimation(const u32 objectID, const CubeAnimationTrigger cubeAnimTrigger)
    {
      ExternalInterface::StopCubeAnim m;
      m.objectID = objectID;
      m.trigger = cubeAnimTrigger;
      SendMessage(ExternalInterface::MessageGameToEngine(std::move(m)));
    }

    void UiGameController::SendAnimation(const char* animName, u32 numLoops, bool throttleMessages)
    {
      static double lastSendTime_sec = -1e6;

      // Don't send repeated animation commands within a half second
      if(!throttleMessages || _supervisor.getTime() > lastSendTime_sec + 0.5f)
      {
        PRINT_NAMED_INFO("SendAnimation", "sending %s", animName);
        ExternalInterface::PlayAnimation m;
        m.animationName = animName;
        m.numLoops = numLoops;
        ExternalInterface::MessageGameToEngine message;
        message.Set_PlayAnimation(m);
        SendMessage(message);
        lastSendTime_sec = _supervisor.getTime();
      } else {
        PRINT_NAMED_INFO("SendAnimation", "Ignoring duplicate SendAnimation keystroke.");
      }
    }

    void UiGameController::SendAnimationGroup(const char* animGroupName, u32 numLoops, bool throttleMessages)
    {
      static double lastSendTime_sec = -1e6;
      // Don't send repeated animation commands within a half second
      if(!throttleMessages || _supervisor.getTime() > lastSendTime_sec + 0.5f)
      {
        PRINT_NAMED_INFO("SendAnimationGroup", "sending %s", animGroupName);
        ExternalInterface::PlayAnimationGroup m;
        m.animationGroupName = animGroupName;
        m.numLoops = numLoops;
        ExternalInterface::MessageGameToEngine message;
        message.Set_PlayAnimationGroup(m);
        SendMessage(message);
        lastSendTime_sec = _supervisor.getTime();
      } else {
        PRINT_NAMED_INFO("SendAnimationGroup", "Ignoring duplicate SendAnimation keystroke.");
      }
    }

    void UiGameController::SendAnimationTrigger(const char* animTriggerName, u32 numLoops, bool throttleMessages)
    {
      static double lastSendTime_sec = -1e6;
      // Don't send repeated animation commands within a half second
      if(!throttleMessages || _supervisor.getTime() > lastSendTime_sec + 0.5f)
      {
        PRINT_NAMED_INFO("SendAnimationTrigger", "sending %s", animTriggerName);
        ExternalInterface::PlayAnimationTrigger m(numLoops,AnimationTriggerFromString(animTriggerName),false,false,false,false);
        ExternalInterface::MessageGameToEngine message;
        message.Set_PlayAnimationTrigger(m);
        SendMessage(message);
        lastSendTime_sec = _supervisor.getTime();
      } else {
        PRINT_NAMED_INFO("SendAnimationTrigger", "Ignoring duplicate SendAnimation keystroke.");
      }
    }

    void UiGameController::SendReadAnimationFile()
    {
      ExternalInterface::ReadAnimationFile m;
      ExternalInterface::MessageGameToEngine message;
      message.Set_ReadAnimationFile(m);
      SendMessage(message);
    }

    uint32_t UiGameController::SendQueuePlayAnimAction(const std::string &animName, u32 numLoops, QueueActionPosition pos) {
      ExternalInterface::QueueSingleAction msg;
      msg.idTag = ++_queueActionIdTag;
      msg.position = pos;
      msg.action.Set_playAnimation(ExternalInterface::PlayAnimation(numLoops, animName, false, false, false));

      ExternalInterface::MessageGameToEngine message;
      message.Set_QueueSingleAction(msg);
      SendMessage(message);
      return msg.idTag;
    }

    void UiGameController::SendCancelAction() {
      ExternalInterface::CancelAction msg;
      msg.actionType = RobotActionType::UNKNOWN;
      ExternalInterface::MessageGameToEngine message;
      message.Set_CancelAction(msg);
      SendMessage(message);
    }

    void UiGameController::SendSaveCalibrationImage()
    {
      ExternalInterface::SaveCalibrationImage msg;
      ExternalInterface::MessageGameToEngine message;
      message.Set_SaveCalibrationImage(msg);
      SendMessage(message);
    }

    void UiGameController::SendClearCalibrationImages()
    {
      ExternalInterface::ClearCalibrationImages msg;
      ExternalInterface::MessageGameToEngine message;
      message.Set_ClearCalibrationImages(msg);
      SendMessage(message);
    }

    void UiGameController::SendComputeCameraCalibration()
    {
      ExternalInterface::ComputeCameraCalibration msg;
      ExternalInterface::MessageGameToEngine message;
      message.Set_ComputeCameraCalibration(msg);
      SendMessage(message);
    }

    void UiGameController::SendCameraCalibration(f32 focalLength_x, f32 focalLength_y, f32 center_x, f32 center_y)
    {
      CameraCalibration msg;
      msg.focalLength_x = focalLength_x;
      msg.focalLength_y = focalLength_y;
      msg.center_x = center_x;
      msg.center_y = center_y;
      msg.skew = 0;
      msg.nrows = 240;
      msg.ncols = 320;
      ExternalInterface::MessageGameToEngine message;
      message.Set_CameraCalibration(msg);
      SendMessage(message);
    }

    void UiGameController::SendEnableVisionMode(VisionMode mode, bool enable)
    {
      ExternalInterface::EnableVisionMode m;
      m.mode = mode;
      m.enable = enable;
      SendMessage(ExternalInterface::MessageGameToEngine(std::move(m)));
    }

    void UiGameController::SendConnectToCube()
    {
      using namespace ExternalInterface;
      SendMessage(MessageGameToEngine(ConnectToCube()));
    }

    void UiGameController::SendDisconnectFromCube(const float gracePeriod_sec)
    {
      using namespace ExternalInterface;
      SendMessage(MessageGameToEngine(DisconnectFromCube(gracePeriod_sec)));
    }

    void UiGameController::SendForgetPreferredCube()
    {
      using namespace ExternalInterface;
      SendMessage(MessageGameToEngine(ForgetPreferredCube()));
    }

    void UiGameController::SendSetPreferredCube(const std::string& preferredCubeFactoryId)
    {
      using namespace ExternalInterface;
      SendMessage(MessageGameToEngine(SetPreferredCube(preferredCubeFactoryId)));
    }

    void UiGameController::SendBroadcastObjectAvailable(const bool enable) {
      using namespace ExternalInterface;
      SendMessage(MessageGameToEngine(SendAvailableObjects(enable)));
    }

    void UiGameController::SendSetActiveObjectLEDs(const u32 objectID,
                                                   const u32 onColor,
                                                   const u32 offColor,
                                                   const u32 onPeriod_ms,
                                                   const u32 offPeriod_ms,
                                                   const u32 transitionOnPeriod_ms,
                                                   const u32 transitionOffPeriod_ms,
                                                   const s32 offset,
                                                   const bool rotate,
                                                   const f32 relativeToX,
                                                   const f32 relativeToY,
                                                   const WhichCubeLEDs whichLEDs,
                                                   const MakeRelativeMode makeRelative,
                                                   const bool turnOffUnspecifiedLEDs)
    {
      ExternalInterface::SetActiveObjectLEDs m(
        objectID,
        onColor,
        offColor,
        onPeriod_ms,
        offPeriod_ms,
        transitionOnPeriod_ms,
        transitionOffPeriod_ms,
        offset,
        relativeToX,
        relativeToY,
        rotate,
        whichLEDs,
        makeRelative,
        turnOffUnspecifiedLEDs
      );

      SendMessage(ExternalInterface::MessageGameToEngine(std::move(m)));
    }

    void UiGameController::SendSetAllActiveObjectLEDs(const u32 objectID,
                                                      const std::array<u32, 4> onColor,
                                                      const std::array<u32, 4> offColor,
                                                      const std::array<u32, 4> onPeriod_ms,
                                                      const std::array<u32, 4> offPeriod_ms,
                                                      const std::array<u32, 4> transitionOnPeriod_ms,
                                                      const std::array<u32, 4> transitionOffPeriod_ms,
                                                      const std::array<s32, 4> offset,
                                                      const bool rotate,
                                                      const f32 relativeToX,
                                                      const f32 relativeToY,
                                                      const MakeRelativeMode makeRelative)
    {
      ExternalInterface::SetAllActiveObjectLEDs m(
        objectID,
        onColor,
        offColor,
        onPeriod_ms,
        offPeriod_ms,
        transitionOnPeriod_ms,
        transitionOffPeriod_ms,
        offset,
        relativeToX,
        relativeToY,
        rotate,
        makeRelative
      );

      SendMessage(ExternalInterface::MessageGameToEngine(std::move(m)));
    }

    void UiGameController::SendPushDrivingAnimations(const std::string& lockName,
                                                     const AnimationTrigger& startAnim,
                                                     const AnimationTrigger& loopAnim,
                                                     const AnimationTrigger& endAnim)
    {
      ExternalInterface::PushDrivingAnimations m;
      m.lockName = lockName;
      m.drivingStartAnim = startAnim;
      m.drivingLoopAnim = loopAnim;
      m.drivingEndAnim = endAnim;

      SendMessage(ExternalInterface::MessageGameToEngine(std::move(m)));
    }

    void UiGameController::SendRemoveDrivingAnimations(const std::string& lockName)
    {
      ExternalInterface::RemoveDrivingAnimations m;
      m.lockName = lockName;

      SendMessage(ExternalInterface::MessageGameToEngine(std::move(m)));
    }


    void UiGameController::QuitWebots(s32 status)
    {
      PRINT_NAMED_INFO("UiGameController.QuitWebots.Result", "%d", status);
      _supervisor.simulationQuit(status);
    }

    void UiGameController::QuitController(s32 status)
    {
      PRINT_NAMED_INFO("UiGameController.QuitController.Result", "%d", status);
      exit(status);
    }

    s32 UiGameController::GetStepTimeMS() const
    {
      return _stepTimeMS;
    }

    webots::Supervisor& UiGameController::GetSupervisor()
    {
      return _supervisor;
    }

    const Pose3d& UiGameController::GetRobotPose() const
    {
      return _robotPose;
    }

    const Pose3d& UiGameController::GetRobotPoseActual() const
    {
      return _robotPoseActual;
    }

    f32 UiGameController::GetRobotHeadAngle_rad() const
    {
      return _robotStateMsg.headAngle_rad;
    }

    f32 UiGameController::GetLiftHeight_mm() const
    {
      return _robotStateMsg.liftHeight_mm;
    }

    void UiGameController::GetWheelSpeeds_mmps(f32& left, f32& right) const
    {
      left = _robotStateMsg.leftWheelSpeed_mmps;
      right = _robotStateMsg.rightWheelSpeed_mmps;
    }

    s32 UiGameController::GetCarryingObjectID() const
    {
      return _robotStateMsg.carryingObjectID;
    }

    bool UiGameController::IsRobotStatus(RobotStatusFlag mask) const
    {
      return _robotStateMsg.status & (uint16_t)mask;
    }

    std::vector<s32> UiGameController::GetAllObjectIDs() const
    {
      std::vector<s32> v;
      for (const auto& obj : _observedObjects) {
        v.push_back(obj.id);
      }
      return v;
    }

    std::vector<s32> UiGameController::GetAllLightCubeObjectIDs() const
    {
      std::vector<s32> v;
      for (const auto& obj : _observedObjects) {
        if (IsValidLightCube(obj.type, false)) {
          v.push_back(obj.id);
        }
      }
      return v;
    }

    std::vector<s32> UiGameController::GetAllObjectIDsByType(const ObjectType& type) const
    {
      std::vector<s32> v;
      for (const auto& obj : _observedObjects) {
        if (obj.type == type) {
          v.push_back(obj.id);
        }
      }
      return v;
    }

    Result UiGameController::GetObjectType(s32 objectID, ObjectType& type) const
    {
      auto it = std::find_if(_observedObjects.begin(),
                             _observedObjects.end(),
                             [&objectID](const ObservedObject& obj) {
                               return (obj.id == objectID);
                             });
      if (it != _observedObjects.end()) {
        type = it->type;
        return RESULT_OK;
      }
      return RESULT_FAIL;
    }

    Result UiGameController::GetObjectPose(s32 objectID, Pose3d& pose) const
    {
      auto it = std::find_if(_observedObjects.begin(),
                             _observedObjects.end(),
                             [&objectID](const ObservedObject& obj) {
                               return (obj.id == objectID);
                             });
      if (it != _observedObjects.end()) {
        pose = it->pose;
        return RESULT_OK;
      }
      return RESULT_FAIL;
    }

    u32 UiGameController::GetNumObjects() const
    {
      return (u32)_observedObjects.size();
    }

    void UiGameController::ClearAllKnownObjects()
    {
      _observedObjects.clear();
    }

    std::map<s32, Pose3d> UiGameController::GetObjectPoseMap() {
      std::map<s32, Pose3d> map;
      for (const auto& obj : _observedObjects) {
        map[obj.id] = obj.pose;
      }
      return map;
    }

    UiGameController::ObservedObject UiGameController::GetLastObservedObject() const
    {
      auto it = std::max_element(_observedObjects.begin(),
                                 _observedObjects.end(),
                                 [](const ObservedObject& obj1, const ObservedObject& obj2) {
                                   return (obj1.observedTimestamp < obj2.observedTimestamp);
                                 });
      if (it == _observedObjects.end()) {
        return ObservedObject();
      } else {
        return *it;
      }
    }

    const Vision::FaceID_t UiGameController::GetLastObservedFaceID() const
    {
      return _lastObservedFaceID;
    }

    void UiGameController::PressBackpackButton(bool pressed)
    {
      if (_backpackButtonPressedField == nullptr) {
        if (_robotNode == nullptr) {
          PRINT_NAMED_ERROR("UiGameController.PressBackpackButton.NullRobotNode", "");
          return;
        } else {
          _backpackButtonPressedField = _robotNode->getField("backpackButtonPressed");
        }
      }
      _backpackButtonPressedField->setSFBool(pressed);
    }

    void UiGameController::TouchBackpackTouchSensor(bool touched)
    {
      if (_touchSensorTouchedField == nullptr) {
        if (_robotNode == nullptr) {
          PRINT_NAMED_ERROR("UiGameController.TouchBackpackTouchSensor.NullRobotNode", "");
          return;
        } else {
          _touchSensorTouchedField = _robotNode->getField("touchSensorTouched");
        }
      }
      _touchSensorTouchedField->setSFBool(touched);
    }

    void UiGameController::StartFreeplayMode()
    {
      using namespace ExternalInterface;
      SendMessage(MessageGameToEngine(SetDebugConsoleVarMessage("DevDispatchAfterShake", "1")));
    }

    void UiGameController::SetActualRobotPose(const Pose3d& newPose)
    {
      SetNodePose(_robotNode, newPose);
    }

    void SetActualObjectPose(const std::string& name, const Pose3d& newPose)
    {
      // TODO: Implement!
    }

    void UiGameController::SetNodePose(webots::Node* node, const Pose3d& newPose)
    {
      if(node != nullptr) {
        webots::Field* rotField = node->getField("rotation");
        assert(rotField != nullptr);

        webots::Field* transField = node->getField("translation");
        assert(transField != nullptr);

        const RotationVector3d& Rvec = newPose.GetRotationVector();
        const double rotation[4] = {
          Rvec.GetAxis().x(), Rvec.GetAxis().y(), Rvec.GetAxis().z(),
          Rvec.GetAngle().ToFloat()
        };
        rotField->setSFRotation(rotation);

        const double translation[3] = {
          MM_TO_M(newPose.GetTranslation().x()),
          MM_TO_M(newPose.GetTranslation().y()),
          MM_TO_M(newPose.GetTranslation().z())
        };
        transField->setSFVec3f(translation);
      }
    }

    void UiGameController::SetLightCubePose(ObjectType lightCubeType, const Pose3d& newPose)
    {
      webots::Node* lightCube = GetLightCubeByType(lightCubeType);

      assert(lightCube != nullptr);

      SetNodePose(lightCube, newPose);
    }

    const Pose3d UiGameController::GetLightCubePoseActual(ObjectType lightCubeType)
    {
      webots::Node* lightCube = GetLightCubeByType(lightCubeType);
      return GetPose3dOfNode(lightCube);
    }

    const std::string UiGameController::GetAnimationTestName() const
    {
      std::string animTestName;
      WebotsHelpers::GetFieldAsString(*_robotNode, "animationTestName", animTestName);
      return animTestName;
    }

    const Pose3d UiGameController::GetPose3dOfNode(webots::Node* node) const
    {
      const double* transActual = node->getPosition();
      const double* orientationActual = node->getOrientation();

      Pose3d pose;

      pose.SetTranslation({
        static_cast<f32>(M_TO_MM(transActual[0])),
        static_cast<f32>(M_TO_MM(transActual[1])),
        static_cast<f32>(M_TO_MM(transActual[2]))
      } );

      pose.SetRotation({
        static_cast<f32>(orientationActual[0]),
        static_cast<f32>(orientationActual[1]),
        static_cast<f32>(orientationActual[2]),
        static_cast<f32>(orientationActual[3]),
        static_cast<f32>(orientationActual[4]),
        static_cast<f32>(orientationActual[5]),
        static_cast<f32>(orientationActual[6]),
        static_cast<f32>(orientationActual[7]),
        static_cast<f32>(orientationActual[8])
      } );

      pose.SetParent(_webotsOrigin);

      return pose;
    }

    bool UiGameController::HasActualLightCubePose(ObjectType inType) const
    {
      for (auto lightCube : _lightCubes) {
        webots::Field* type = lightCube->getField("objectType");
        if (type && (ObjectTypeFromString(type->getSFString()) == inType)) {
          return true;
        }
      }
      return false;
    }

    webots::Node* UiGameController::GetLightCubeByType(ObjectType inType) const
    {
      for (auto lightCube : _lightCubes) {
        webots::Field* type = lightCube->getField("objectType");
        if (type && (ObjectTypeFromString(type->getSFString()) == inType)) {
          return lightCube;
        }
      }

      DEV_ASSERT_MSG(false, "UiGameController.GetLightCubeByType",
                     "Can't find the light cube with type '%s' in the world", ObjectTypeToString(inType));
      return nullptr;
    }

    bool UiGameController::RemoveLightCubeByType(ObjectType inType)
    {
      for (auto it = _lightCubes.begin(); it != _lightCubes.end(); ++it) {
        webots::Field* type = (*it)->getField("objectType");
        if (type && (ObjectTypeFromString(type->getSFString()) == inType)) {
          (*it)->remove();
          _lightCubes.erase(it);
          return true;
        }
      }

      DEV_ASSERT_MSG(false, "UiGameController.RemoveLightCubeById",
                     "Can't find the light cube of ObjectType '%s' in the world", ObjectTypeToString(inType));
      return false;
    }

    bool UiGameController::AddLightCubeByType(ObjectType inType, const Pose3d& p, const std::string& factoryID)
    {
      // Check if world already has a light cube with that type
      for (auto lightCube : _lightCubes) {
        webots::Field* type = lightCube->getField("objectType");
        if (type && (ObjectTypeFromString(type->getSFString()) == inType)) {
          PRINT_NAMED_WARNING("UiGameController.AddLightCubeByType.ObjectTypeAlreadyExists", "%s", ObjectTypeToString(inType));
          return false;
        }
      }

      // Import light cube proto instance into scene tree
      std::stringstream ss;
      ss << "LightCube { "
      << " objectType " << ObjectTypeToString(inType)
      << " factoryID " << factoryID
      << " translation "
      << 0.001f * p.GetTranslation().x() << " "
      << 0.001f * p.GetTranslation().y() << " "
      << 0.001f * p.GetTranslation().z() << " "
      << " rotation "
      << p.GetRotationAxis().x() << " " << p.GetRotationAxis().y() << " " << p.GetRotationAxis().z() << " "
      << p.GetRotationAngle().ToFloat() << " }";

      webots::Field* rootChildren = GetSupervisor().getRoot()->getField("children");
      int numRootChildren = rootChildren->getCount();
      rootChildren->importMFNodeFromString(numRootChildren, ss.str());

      // Find node and add it to _lightCubes
      webots::Node* lightCubeNode = rootChildren->getMFNode(numRootChildren);
      _lightCubes.emplace_back(lightCubeNode);

      return true;
    }

    void SetChargerPluggedIn(webots::Node* chargerNode, const bool pluggedIn)
    {
      DEV_ASSERT(chargerNode != nullptr, "UiGameController.SetChargerPluggedIn.NullNode");
      auto* isPluggedInField = chargerNode->getField("isPluggedIn");
      DEV_ASSERT(isPluggedInField != nullptr, "UiGameController.SetChargerPluggedIn.NoIsPluggedInField");
      isPluggedInField->setSFBool(pluggedIn);
    }

    const double UiGameController::GetSupervisorTime() const
    {
      return _supervisor.getTime();
    }

    const bool UiGameController::HasXSecondsPassedYet(double xSeconds)
    {
      if (_waitTimer < 0){
        _waitTimer = GetSupervisorTime();
      }

      if (GetSupervisorTime() > _waitTimer + xSeconds){
        // reset waitTimer so it can be reused next time.
        _waitTimer = -1;
        return true;
      } else {
        return false;
      }
    }

    webots::Node* UiGameController::GetNodeByDefName(const std::string& defName) const
    {
      return _supervisor.getFromDef(defName);
    }

    void UiGameController::SendApplyForce(const std::string& defName,
                                          int xForce, int yForce, int zForce)
    {
      PhysicsInterface::MessageSimPhysics message;
      PhysicsInterface::ApplyForce msg;
      msg.DefName = defName;
      msg.xForce = xForce;
      msg.yForce = yForce;
      msg.zForce = zForce;
      u8 buf[message.Size()];
      message.Set_ApplyForce(msg);
      size_t numBytes = message.Pack(buf, message.Size());
      _physicsControllerClient.Send((char*)buf, numBytes);
    }
  } // namespace Vector
} // namespace Anki
