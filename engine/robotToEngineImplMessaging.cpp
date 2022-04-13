/**
 * File: robotImplMessaging
 *
 * Author: damjan stulic
 * Created: 9/9/15
 *
 * Description:
 * robot class methods specific to message handling
 *
 * Copyright: Anki, inc. 2015
 *
 */

#include <opencv2/imgproc.hpp>

#include "coretech/common/engine/utils/data/dataPlatform.h"
#include "util/helpers/printByteArray.h"
#include "coretech/common/engine/utils/timer.h"
#include "engine/robotToEngineImplMessaging.h"
#include "engine/actions/actionContainers.h"
#include "engine/actions/animActions.h"
#include "engine/actions/basicActions.h"
#include "engine/ankiEventUtil.h"
#include "engine/blockWorld/blockWorld.h"
#include "engine/charger.h"
#include "engine/components/battery/batteryComponent.h"
#include "engine/components/blockTapFilterComponent.h"
#include "engine/components/carryingComponent.h"
#include "engine/components/sensors/cliffSensorComponent.h"
#include "engine/components/dockingComponent.h"
#include "engine/components/movementComponent.h"
#include "engine/components/visionComponent.h"
#include "engine/cozmoContext.h"
#include "engine/externalInterface/externalInterface.h"
#include "engine/components/mics/micComponent.h"
#include "engine/components/mics/micDirectionHistory.h"
#include "engine/pathPlanner.h"
#include "engine/robot.h"
#include "engine/robotInterface/messageHandler.h"
#include "engine/utils/cozmoExperiments.h"
#include "engine/utils/parsingConstants/parsingConstants.h"

#include "clad/robotInterface/messageEngineToRobot.h"
#include "clad/robotInterface/messageEngineToRobot_hash.h"
#include "clad/robotInterface/messageRobotToEngine.h"
#include "clad/robotInterface/messageRobotToEngine_hash.h"
#include "clad/types/robotStatusAndActions.h"

#include "audioUtil/audioDataTypes.h"
#include "audioUtil/waveFile.h"

#include "util/cpuProfiler/cpuProfiler.h"
#include "util/debug/messageDebugging.h"
#include "util/fileUtils/fileUtils.h"
#include "util/helpers/includeFstream.h"
#include "util/logging/DAS.h"
#include "util/signals/signalHolder.h"

#include "webServerProcess/src/webService.h"

#include "anki/cozmo/shared/factory/emrHelper.h"

#include <functional>

#define LOG_CHANNEL "RobotState"

// Prints the IDs of the active blocks that are on but not currently
// talking to a robot whose rssi is less than this threshold.
// Prints roughly once/sec.
#define DISCOVERED_OBJECTS_RSSI_PRINT_THRESH 50

// Filter that makes chargers not discoverable
#define IGNORE_CHARGER_DISCOVERY 0

// How often do we send power level updates to DAS?
#define POWER_LEVEL_INTERVAL_SEC 600

namespace Anki {
namespace Vector {

using GameToEngineEvent = AnkiEvent<ExternalInterface::MessageGameToEngine>;

RobotToEngineImplMessaging::RobotToEngineImplMessaging()
: IDependencyManagedComponent(this, RobotComponentID::RobotToEngineImplMessaging)
{
   _faceImageRGB565.Allocate(FACE_DISPLAY_HEIGHT, FACE_DISPLAY_WIDTH);
}

RobotToEngineImplMessaging::~RobotToEngineImplMessaging()
{
}


void RobotToEngineImplMessaging::InitRobotMessageComponent(RobotInterface::MessageHandler* messageHandler, Robot* const robot)
{
  using localHandlerType = void(RobotToEngineImplMessaging::*)(const AnkiEvent<RobotInterface::RobotToEngine>&);
  // Create a helper lambda for subscribing to a tag with a local handler
  auto doRobotSubscribe = [this, messageHandler] (RobotInterface::RobotToEngineTag tagType, localHandlerType handler)
  {
    GetSignalHandles().push_back(messageHandler->Subscribe(tagType, std::bind(handler, this, std::placeholders::_1)));
  };

  using localHandlerTypeWithRoboRef = void(RobotToEngineImplMessaging::*)(const AnkiEvent<RobotInterface::RobotToEngine>&, Robot* const);
  auto doRobotSubscribeWithRoboRef = [this, messageHandler, robot] (RobotInterface::RobotToEngineTag tagType, localHandlerTypeWithRoboRef handler)
  {
    GetSignalHandles().push_back(messageHandler->Subscribe(tagType, std::bind(handler, this, std::placeholders::_1, robot)));
  };

  // bind to specific handlers in the robotImplMessaging class
  doRobotSubscribeWithRoboRef(RobotInterface::RobotToEngineTag::pickAndPlaceResult,             &RobotToEngineImplMessaging::HandlePickAndPlaceResult);
  doRobotSubscribeWithRoboRef(RobotInterface::RobotToEngineTag::fallingEvent,                   &RobotToEngineImplMessaging::HandleFallingEvent);
  doRobotSubscribeWithRoboRef(RobotInterface::RobotToEngineTag::fallImpactEvent,                &RobotToEngineImplMessaging::HandleFallImpactEvent);
  doRobotSubscribeWithRoboRef(RobotInterface::RobotToEngineTag::goalPose,                       &RobotToEngineImplMessaging::HandleGoalPose);
  doRobotSubscribeWithRoboRef(RobotInterface::RobotToEngineTag::robotStopped,                   &RobotToEngineImplMessaging::HandleRobotStopped);
  doRobotSubscribeWithRoboRef(RobotInterface::RobotToEngineTag::cliffEvent,                     &RobotToEngineImplMessaging::HandleCliffEvent);
  doRobotSubscribeWithRoboRef(RobotInterface::RobotToEngineTag::potentialCliff,                 &RobotToEngineImplMessaging::HandlePotentialCliffEvent);
  doRobotSubscribeWithRoboRef(RobotInterface::RobotToEngineTag::imuDataChunk,                   &RobotToEngineImplMessaging::HandleImuData);
  doRobotSubscribeWithRoboRef(RobotInterface::RobotToEngineTag::imuRawDataChunk,                &RobotToEngineImplMessaging::HandleImuRawData);
  doRobotSubscribeWithRoboRef(RobotInterface::RobotToEngineTag::syncRobotAck,                   &RobotToEngineImplMessaging::HandleSyncRobotAck);
  doRobotSubscribeWithRoboRef(RobotInterface::RobotToEngineTag::motorCalibration,               &RobotToEngineImplMessaging::HandleMotorCalibration);
  doRobotSubscribeWithRoboRef(RobotInterface::RobotToEngineTag::motorAutoEnabled,               &RobotToEngineImplMessaging::HandleMotorAutoEnabled);
  doRobotSubscribe(RobotInterface::RobotToEngineTag::dockingStatus,                             &RobotToEngineImplMessaging::HandleDockingStatus);
  doRobotSubscribeWithRoboRef(RobotInterface::RobotToEngineTag::micDirection,                   &RobotToEngineImplMessaging::HandleMicDirection);
  doRobotSubscribeWithRoboRef(RobotInterface::RobotToEngineTag::micDataState,                   &RobotToEngineImplMessaging::HandleMicDataState);
  doRobotSubscribeWithRoboRef(RobotInterface::RobotToEngineTag::streamCameraImages,             &RobotToEngineImplMessaging::HandleStreamCameraImages);
  doRobotSubscribeWithRoboRef(RobotInterface::RobotToEngineTag::displayedFaceImageRGBChunk,     &RobotToEngineImplMessaging::HandleDisplayedFaceImage);
  doRobotSubscribeWithRoboRef(RobotInterface::RobotToEngineTag::robotPoked,                     &RobotToEngineImplMessaging::HandleRobotPoked);

  // lambda wrapper to call internal handler
  GetSignalHandles().push_back(messageHandler->Subscribe(RobotInterface::RobotToEngineTag::state,
                                                     [robot](const AnkiEvent<RobotInterface::RobotToEngine>& message){
                                                       ANKI_CPU_PROFILE("RobotTag::state");
                                                       const RobotState& payload = message.GetData().Get_state();
                                                       robot->UpdateFullRobotState(payload);
                                                     }));

  GetSignalHandles().push_back(messageHandler->Subscribe(RobotInterface::RobotToEngineTag::chargerMountCompleted,
                                                         [robot](const AnkiEvent<RobotInterface::RobotToEngine>& message){
                                                           ANKI_CPU_PROFILE("RobotTag::chargerMountCompleted");
                                                           const bool didSucceed = message.GetData().Get_chargerMountCompleted().didSucceed;
                                                           LOG_INFO("RobotMessageHandler.ProcessMessage",
                                                                    "Charger mount %s.",
                                                                    didSucceed ? "SUCCEEDED" : "FAILED" );
                                                           if (didSucceed) {
                                                             robot->SetPoseOnCharger();
                                                           }
                                                         }));

  GetSignalHandles().push_back(messageHandler->Subscribe(RobotInterface::RobotToEngineTag::imuTemperature,
                                                     [robot](const AnkiEvent<RobotInterface::RobotToEngine>& message){
                                                       ANKI_CPU_PROFILE("RobotTag::imuTemperature");

                                                       const auto temp_degC = message.GetData().Get_imuTemperature().temperature_degC;
                                                       // This prints an info every time we receive this message. This is useful for gathering data
                                                       // in the prototype stages, and could probably be removed in production.
                                                       LOG_DEBUG("RobotMessageHandler.ProcessMessage.MessageImuTemperature",
                                                                "IMU temperature: %.3f degC",
                                                                temp_degC);
                                                       robot->SetImuTemperature(temp_degC);
                                                     }));

  GetSignalHandles().push_back(messageHandler->Subscribe(RobotInterface::RobotToEngineTag::enterPairing,
                                                     [robot](const AnkiEvent<RobotInterface::RobotToEngine>& message){
                                                       // Forward to switchboard
                                                       LOG_INFO("RobotMessageHandler.ProcessMessage.EnterPairing","");
                                                       robot->Broadcast(ExternalInterface::MessageEngineToGame(SwitchboardInterface::EnterPairing()));
                                                     }));

  GetSignalHandles().push_back(messageHandler->Subscribe(RobotInterface::RobotToEngineTag::exitPairing,
                                                     [robot](const AnkiEvent<RobotInterface::RobotToEngine>& message){
                                                       // Forward to switchboard
                                                       robot->Broadcast(ExternalInterface::MessageEngineToGame(SwitchboardInterface::ExitPairing()));
                                                     }));

  GetSignalHandles().push_back(messageHandler->Subscribe(RobotInterface::RobotToEngineTag::prepForShutdown,
                                                     [robot](const AnkiEvent<RobotInterface::RobotToEngine>& message){
                                                       LOG_INFO("RobotMessageHandler.ProcessMessage.Shutdown","");
                                                       robot->Shutdown(message.GetData().Get_prepForShutdown().reason);
                                                     }));

  
  if (robot->HasExternalInterface())
  {
    using namespace ExternalInterface;
    auto helper = MakeAnkiEventUtil(*robot->GetExternalInterface(), *robot, GetSignalHandles());
    helper.SubscribeGameToEngine<MessageGameToEngineTag::RequestRobotSettings>();
  }
}

void RobotToEngineImplMessaging::HandleMotorCalibration(const AnkiEvent<RobotInterface::RobotToEngine>& message, Robot* const robot)
{
  ANKI_CPU_PROFILE("Robot::HandleMotorCalibration");

  const MotorCalibration& payload = message.GetData().Get_motorCalibration();
  LOG_INFO("HandleMotorCalibration.Recvd", "Motor %s, started %d, autoStarted %d",
           EnumToString(payload.motorID), payload.calibStarted, payload.autoStarted);

  if (payload.motorID == MotorID::MOTOR_LIFT &&
      payload.calibStarted && robot->GetCarryingComponent().IsCarryingObject())
  {
    // if this was a lift calibration, we are no longer holding a cube
    const bool deleteObjects = true; // we have no idea what happened to the cube, so remove completely from the origin
    robot->GetCarryingComponent().SetCarriedObjectAsUnattached(deleteObjects);
  }

  if (payload.motorID == MotorID::MOTOR_HEAD) {
    robot->SetHeadCalibrated(!payload.calibStarted);
  }

  if (payload.motorID == MotorID::MOTOR_LIFT) {
    robot->SetLiftCalibrated(!payload.calibStarted);
  }

  robot->Broadcast(ExternalInterface::MessageEngineToGame(MotorCalibration(payload)));
}

void RobotToEngineImplMessaging::HandleMotorAutoEnabled(const AnkiEvent<RobotInterface::RobotToEngine>& message, Robot* const robot)
{
  ANKI_CPU_PROFILE("Robot::HandleMotorAutoEnabled");

  const MotorAutoEnabled& payload = message.GetData().Get_motorAutoEnabled();
  LOG_INFO("HandleMotorAutoEnabled.Recvd", "Motor %d, enabled %d", (int)payload.motorID, payload.enabled);

  if (!payload.enabled) {
    // Burnout protection triggered.
    // Somebody is probably messing with the lift
    LOG_INFO("HandleMotorAutoEnabled.MotorDisabled", "%s", EnumToString(payload.motorID));
  } else {
    LOG_INFO("HandleMotorAutoEnabled.MotorEnabled", "%s", EnumToString(payload.motorID));
  }

  // This probably applies here as it does in HandleMotorCalibration.
  // Seems reasonable to expect whatever object the robot may have been carrying to no longer be there.
  if (payload.motorID == MotorID::MOTOR_LIFT &&
      !payload.enabled && robot->GetCarryingComponent().IsCarryingObject()) {
    const bool deleteObjects = true; // we have no idea what happened to the cube, so remove completely from the origin
    robot->GetCarryingComponent().SetCarriedObjectAsUnattached(deleteObjects);
  }

  robot->Broadcast(ExternalInterface::MessageEngineToGame(MotorAutoEnabled(payload)));
}

void RobotToEngineImplMessaging::HandlePickAndPlaceResult(const AnkiEvent<RobotInterface::RobotToEngine>& message,
                                                          Robot* const robot)
{
  ANKI_CPU_PROFILE("Robot::HandlePickAndPlaceResult");

  const PickAndPlaceResult& payload = message.GetData().Get_pickAndPlaceResult();
  const char* successStr = (payload.didSucceed ? "succeeded" : "failed");

  robot->GetDockingComponent().SetLastPickOrPlaceSucceeded(payload.didSucceed);

  switch(payload.blockStatus)
  {
    case BlockStatus::NO_BLOCK:
    {
      LOG_INFO("RobotMessageHandler.ProcessMessage.HandlePickAndPlaceResult.NoBlock",
               "Robot reported it %s doing something without a block. Stopping docking and turning on Look-for-Markers mode.",
               successStr);
      break;
    }
    case BlockStatus::BLOCK_PLACED:
    {
      LOG_INFO("RobotMessageHandler.ProcessMessage.HandlePickAndPlaceResult.BlockPlaced",
               "Robot reported it %s placing block. Stopping docking and turning on Look-for-Markers mode.",
               successStr);

      if (payload.didSucceed) {
        robot->GetCarryingComponent().SetCarriedObjectAsUnattached();
      }

      break;
    }
    case BlockStatus::BLOCK_PICKED_UP:
    {
      const char* resultStr = EnumToString(payload.result);

      LOG_INFO("RobotMessageHandler.ProcessMessage.HandlePickAndPlaceResult.BlockPickedUp",
               "Robot %d reported it %s picking up block with %s. Stopping docking and turning on Look-for-Markers mode.",
               robot->GetID(), successStr, resultStr);

      if (payload.didSucceed) {
        robot->GetCarryingComponent().SetDockObjectAsAttachedToLift();
      }

      break;
    }
  }
}

void RobotToEngineImplMessaging::HandleDockingStatus(const AnkiEvent<RobotInterface::RobotToEngine>& message)
{
  ANKI_CPU_PROFILE("Robot::HandleDockingStatus");

  // TODO: Do something with the docking status message like play sound or animation
  //const DockingStatus& payload = message.GetData().Get_dockingStatus();

  // Log event to help us track whether backup or "Hanns Manuever" is being used
  LOG_INFO("robot.docking.status", "%s", EnumToString(message.GetData().Get_dockingStatus().status));
}


void RobotToEngineImplMessaging::HandleFallingEvent(const AnkiEvent<RobotInterface::RobotToEngine>& message, Robot* const robot)
{
  const auto& msg = message.GetData().Get_fallingEvent();

  LOG_INFO("Robot.HandleFallingEvent.FallingEvent",
           "timestamp: %u duration: %u",
           msg.timestamp,
           msg.duration_ms);

  robot->Broadcast(ExternalInterface::MessageEngineToGame(ExternalInterface::RobotFallingEvent(msg.duration_ms)));
}

void RobotToEngineImplMessaging::HandleFallImpactEvent(const AnkiEvent<RobotInterface::RobotToEngine>& message, Robot* const robot)
{
  LOG_INFO("Robot.HandleFallImpactEvent", "");

  // webviz counter for the number of detected fall impacts
  static size_t webvizFallImpactCounter = 0;
  webvizFallImpactCounter++;
  const auto* context = robot->GetContext();
  if (context != nullptr) {
    auto* webService = context->GetWebService();
    if (webService != nullptr) {
      Json::Value toSendJson;
      toSendJson["fall_impact_count"] = (int)webvizFallImpactCounter;
      webService->SendToWebViz("imu", toSendJson);
    }
  }
}

void RobotToEngineImplMessaging::HandleGoalPose(const AnkiEvent<RobotInterface::RobotToEngine>& message, Robot* const robot)
{
  ANKI_CPU_PROFILE("Robot::HandleGoalPose");

  const GoalPose& payload = message.GetData().Get_goalPose();
  Anki::Pose3d p(payload.pose.angle, Z_AXIS_3D(),
                 Vec3f(payload.pose.x, payload.pose.y, payload.pose.z));
  //PRINT_INFO("Goal pose: x=%f y=%f %f deg (%d)", msg.pose_x, msg.pose_y, RAD_TO_DEG(msg.pose_angle), msg.followingMarkerNormal);
  if (payload.followingMarkerNormal) {
    robot->GetContext()->GetVizManager()->DrawPreDockPose(100, p, ::Anki::NamedColors::RED);
  } else {
    robot->GetContext()->GetVizManager()->DrawPreDockPose(100, p, ::Anki::NamedColors::GREEN);
  }
}

void RobotToEngineImplMessaging::HandleRobotStopped(const AnkiEvent<RobotInterface::RobotToEngine>& message, Robot* const robot)
{
  ANKI_CPU_PROFILE("Robot::HandleRobotStopped");
  
  RobotInterface::RobotStopped payload = message.GetData().Get_robotStopped();
  
  DASMSG(robot_impl_messaging.handle_robot_stopped,
         "robot_impl_messaging.handle_robot_stopped",
         "Received RobotStopped message");
  DASMSG_SET(s1, EnumToString(payload.reason), "Stop reason");
  DASMSG_SEND();

  // This is a somewhat overloaded use of enableCliffSensor, but currently only cliffs
  // trigger this RobotStopped message so it's not too crazy.
  if( !(robot->GetCliffSensorComponent().IsCliffSensorEnabled()) ) {
    return;
  }

  // Stop whatever we were doing
  robot->GetActionList().Cancel();

  // Let robot process know that it can re-enable wheels
  robot->SendMessage(RobotInterface::EngineToRobot(RobotInterface::RobotStoppedAck()));

  // Forward on with EngineToGame event
  robot->Broadcast(
    ExternalInterface::MessageEngineToGame(
      ExternalInterface::RobotStopped(
        payload.reason,
        payload.cliffDetectedFlags,
        payload.whiteDetectedFlags)));
}

void RobotToEngineImplMessaging::HandlePotentialCliffEvent(const AnkiEvent<RobotInterface::RobotToEngine>& message, Robot* const robot)
{
  ANKI_CPU_PROFILE("Robot::HandlePotentialCliffEvent");

  // Ignore potential cliff events while on the charger platform because we expect them
  // while driving off the charger
  if (robot->GetBatteryComponent().IsOnChargerPlatform())
  {
    LOG_DEBUG("Robot.HandlePotentialCliffEvent.OnChargerPlatform",
              "Ignoring potential cliff event while on charger platform");
    return;
  }

  if (robot->GetIsCliffReactionDisabled()){
    // Special case handling of potential cliff event when in drone/explorer mode...

    // TODO: Don't try to play this special cliff event animation for drone/explorer mode if it is already
    //       running. Consider adding support for a 'canBeInterrupted' flag or something similar and then
    //       set canBeInterrupted = false before queueing this action to run now (VIC-796). FYI, a different
    //       solution was used for Cozmo (see COZMO-15326 and https://github.com/anki/cozmo-one/pull/6467)

    // Trigger the cliff event animation for drone/explorer mode if it is not already running and:
    // - set interruptRunning = true so any currently-streaming animation will be aborted in favor of this
    // - set a timeout value of 3 seconds for this animation
    // - set strictCooldown = true so we do NOT simply choose the animation closest to being off
    //   cooldown when all animations in the group are on cooldown
    IActionRunner* action = new TriggerLiftSafeAnimationAction(AnimationTrigger::AudioOnlyHuh, 1,
                                                               true, (u8)AnimTrackFlag::NO_TRACKS, 3.f, true);
    robot->GetActionList().QueueAction(QueueActionPosition::NOW, action);
  }
}

void RobotToEngineImplMessaging::HandleCliffEvent(const AnkiEvent<RobotInterface::RobotToEngine>& message, Robot* const robot)
{
  ANKI_CPU_PROFILE("Robot::HandleCliffEvent");

  CliffEvent cliffEvent = message.GetData().Get_cliffEvent();
  const auto& cliffComp = robot->GetCliffSensorComponent();
  // always listen to events which say we aren't on a cliff, but ignore ones which say we are (so we don't
  // get "stuck" on a cliff
  if (!cliffComp.IsCliffSensorEnabled() && (cliffEvent.detectedFlags != 0)) {
    return;
  }

  if (cliffEvent.detectedFlags != 0) {
    Pose3d cliffPose;
    const bool isValidPose = cliffComp.ComputeCliffPose(cliffEvent.timestamp, cliffEvent.detectedFlags, cliffPose);
    if (isValidPose) {
      cliffComp.UpdateNavMapWithCliffAt(cliffPose, cliffEvent.timestamp);
    }
    LOG_INFO("RobotImplMessaging.HandleCliffEvent.Detected",
             "at %.3f,%.3f. DetectedFlags = 0x%02X. %s cliff into nav map",
             cliffPose.GetTranslation().x(),
             cliffPose.GetTranslation().y(),
             cliffEvent.detectedFlags,
             isValidPose ? "Inserting" : "NOT inserting");
  } else {
    LOG_INFO("RobotImplMessaging.HandleCliffEvent.Undetected", "");
  }

  // Forward on with EngineToGame event
  robot->Broadcast(ExternalInterface::MessageEngineToGame(std::move(cliffEvent)));
}

// For processing imu data chunks arriving from robot.
// Writes the entire log of 3-axis accelerometer and 3-axis
// gyro readings to a .m file in kP_IMU_LOGS_DIR so they
// can be read in from Matlab.
void RobotToEngineImplMessaging::HandleImuData(const AnkiEvent<RobotInterface::RobotToEngine>& message, Robot* const robot)
{
  ANKI_CPU_PROFILE("Robot::HandleImuData");

  const RobotInterface::IMUDataChunk& payload = message.GetData().Get_imuDataChunk();

  // If seqID has changed, then start a new log file
  if (payload.seqId != _imuSeqID) {
    _imuSeqID = payload.seqId;

    // Make sure imu capture folder exists
    std::string imuLogsDir = robot->GetContextDataPlatform()->pathToResource(Util::Data::Scope::Cache, AnkiUtil::kP_IMU_LOGS_DIR);
    if (!Util::FileUtils::CreateDirectory(imuLogsDir, false, true)) {
      LOG_ERROR("Robot.HandleImuData.CreateDirFailed","%s", imuLogsDir.c_str());
    }

    // Open imu log file
    std::string imuLogFileName = std::string(imuLogsDir.c_str()) + "/imuLog_" + std::to_string(_imuSeqID) + ".dat";

    LOG_INFO("Robot.HandleImuData.OpeningLogFile", "%s", imuLogFileName.c_str());

    _imuLogFileStream.open(imuLogFileName.c_str());
    _imuLogFileStream << "aX aY aZ gX gY gZ\n";
  }

  for (u32 s = 0; s < (u32)IMUConstants::IMU_CHUNK_SIZE; ++s) {
    _imuLogFileStream << payload.aX.data()[s] << " "
    << payload.aY.data()[s] << " "
    << payload.aZ.data()[s] << " "
    << payload.gX.data()[s] << " "
    << payload.gY.data()[s] << " "
    << payload.gZ.data()[s] << "\n";
  }

  // Close file when last chunk received
  if (payload.chunkId == payload.totalNumChunks - 1) {
    LOG_INFO("Robot.HandleImuData.ClosingLogFile", "");
    _imuLogFileStream.close();
  }
}

void RobotToEngineImplMessaging::HandleImuRawData(const AnkiEvent<RobotInterface::RobotToEngine>& message, Robot* const robot)
{
  ANKI_CPU_PROFILE("Robot::HandleImuRawData");

  const RobotInterface::IMURawDataChunk& payload = message.GetData().Get_imuRawDataChunk();

  if (payload.order == 0) {

    // Make sure imu capture folder exists
    std::string imuLogsDir = robot->GetContextDataPlatform()->pathToResource(Util::Data::Scope::Cache, AnkiUtil::kP_IMU_LOGS_DIR);
    if (!Util::FileUtils::CreateDirectory(imuLogsDir, false, true)) {
      LOG_ERROR("Robot.HandleImuRawData.CreateDirFailed","%s", imuLogsDir.c_str());
    }

    // Open imu log file
    std::string imuLogFileName = "";
    do {
      ++_imuSeqID;
      imuLogFileName = std::string(imuLogsDir.c_str()) + "/imuRawLog_" + std::to_string(_imuSeqID) + ".dat";
    } while (Util::FileUtils::FileExists(imuLogFileName));

    LOG_INFO("Robot.HandleImuRawData.OpeningLogFile",
                     "%s", imuLogFileName.c_str());

    _imuLogFileStream.open(imuLogFileName.c_str());
    _imuLogFileStream << "timestamp aX aY aZ gX gY gZ\n";
  }

  _imuLogFileStream
  << static_cast<int>(payload.timestamp) << " "
  << payload.a.data()[0] << " "
  << payload.a.data()[1] << " "
  << payload.a.data()[2] << " "
  << payload.g.data()[0] << " "
  << payload.g.data()[1] << " "
  << payload.g.data()[2] << "\n";

  // Close file when last chunk received
  if (payload.order == 2) {
    LOG_INFO("Robot.HandleImuRawData.ClosingLogFile", "");
    _imuLogFileStream.close();
  }
}

void RobotToEngineImplMessaging::HandleSyncRobotAck(const AnkiEvent<RobotInterface::RobotToEngine>& message, Robot* const robot)
{
  ANKI_CPU_PROFILE("Robot::HandleSyncRobotAck");
  LOG_INFO("Robot.HandleSyncRobotAck","");
  robot->SetSyncRobotAcked();

  // Move the head up when we sync time so that the customer can see the face easily
  if(FACTORY_TEST && Factory::GetEMR()->fields.PACKED_OUT_FLAG)
  {
    // Move head up
    const f32 kLookUpSpeed_radps = 2;
    auto moveHeadUpAction = new MoveHeadToAngleAction(MAX_HEAD_ANGLE);
    moveHeadUpAction->SetMaxSpeed(kLookUpSpeed_radps);
    moveHeadUpAction->SetAccel(MAX_HEAD_ACCEL_RAD_PER_S2);

    // Set calm mode
    auto setCalmFunc = [](Robot& robot) {
      robot.SendMessage(RobotInterface::EngineToRobot(RobotInterface::CalmPowerMode(true)));
      return true;
    };
    auto setCalmModeAction = new WaitForLambdaAction(setCalmFunc);

    // Command sequential action
    auto moveHeadThenCalm = new CompoundActionSequential();
    moveHeadThenCalm->AddAction(moveHeadUpAction);
    moveHeadThenCalm->AddAction(setCalmModeAction);
    robot->GetActionList().QueueAction(QueueActionPosition::NOW, moveHeadThenCalm);
  }
}

void RobotToEngineImplMessaging::HandleMicDirection(const AnkiEvent<RobotInterface::RobotToEngine>& message, Robot* const robot)
{
  const auto & payload = message.GetData().Get_micDirection();
  robot->GetMicComponent().GetMicDirectionHistory().AddMicSample(payload);
}

void RobotToEngineImplMessaging::HandleMicDataState(const AnkiEvent<RobotInterface::RobotToEngine>& message, Robot* const robot)
{
  const auto & payload = message.GetData().Get_micDataState();
  robot->GetMicComponent().SetBufferFullness(payload.rawBufferFullness);
}

void RobotToEngineImplMessaging::HandleDisplayedFaceImage(const AnkiEvent<RobotInterface::RobotToEngine>& message, Robot* const robot)
{
  const auto & msg = message.GetData().Get_displayedFaceImageRGBChunk();
  if (msg.imageId != _faceImageRGBId) {
    if (_faceImageRGBChunksReceivedBitMask != 0) {
      PRINT_NAMED_WARNING("AnimationStreamer.Process_displayFaceImageRGBChunk.UnfinishedFace",
                          "Overwriting ID %d with ID %d",
                          _faceImageRGBId, msg.imageId);
    }
    _faceImageRGBId = msg.imageId;
    _faceImageRGBChunksReceivedBitMask = 1 << msg.chunkIndex;
  } else {
    _faceImageRGBChunksReceivedBitMask |= 1 << msg.chunkIndex;
  }

  static const u16 kMaxNumPixelsPerChunk = sizeof(msg.faceData) / sizeof(msg.faceData[0]);
  const auto numPixels = std::min(msg.numPixels, kMaxNumPixelsPerChunk);
  // Not user why copy_n wasn't working here, but just going ahead and doing an extra copy to fix the issue
  auto unnecessaryCopy = msg.faceData;
  std::copy_n(unnecessaryCopy.begin(), numPixels, _faceImageRGB565.GetRawDataPointer() + (msg.chunkIndex * kMaxNumPixelsPerChunk));

  if (_faceImageRGBChunksReceivedBitMask == kAllFaceImageRGBChunksReceivedMask) {
    Vision::ImageRGB fullImage;
    fullImage.SetFromRGB565(_faceImageRGB565);

    #if SHOULD_SEND_DISPLAYED_FACE_TO_ENGINE
    robot->GetComponent<FullRobotPose>().SetDisplayImg(fullImage);
    #endif

    _faceImageRGBId = 0;
    _faceImageRGBChunksReceivedBitMask = 0;
  }

}

void RobotToEngineImplMessaging::HandleStreamCameraImages(const AnkiEvent<RobotInterface::RobotToEngine>& message,
                                                          Robot* const robot)
{
  const auto & payload = message.GetData().Get_streamCameraImages();
  robot->GetVisionComponent().EnableMirrorMode(payload.enable);
}

void RobotToEngineImplMessaging::HandleRobotPoked(const AnkiEvent<RobotInterface::RobotToEngine>& message, Robot* const robot)
{
  ANKI_CPU_PROFILE("Robot::HandleRobotPoked");
  LOG_INFO("Robot.HandleRobotPoked","");
  robot->HandlePokeEvent();
}

} // end namespace Vector
} // end namespace Anki
