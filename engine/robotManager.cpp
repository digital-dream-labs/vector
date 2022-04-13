//
//  robotManager.cpp
//  Products_Cozmo
//
//  Created by Andrew Stein on 8/23/13.
//  Copyright (c) 2013 Anki, Inc. All rights reserved.
//

#include "engine/components/battery/batteryComponent.h"
#include "engine/cozmoContext.h"
#include "engine/externalInterface/externalMessageRouter.h"
#include "engine/externalInterface/gatewayInterface.h"
#include "engine/robot.h"
#include "engine/robotInitialConnection.h"
#include "engine/robotInterface/messageHandler.h"
#include "engine/robotManager.h"
#include "coretech/common/robot/config.h"
#include "util/cpuProfiler/cpuProfiler.h"
#include "util/fileUtils/fileUtils.h"
#include "util/logging/logging.h"
#include "util/time/stepTimers.h"

#include "osState/osState.h"

#include "anki/cozmo/shared/factory/faultCodes.h"

#include "util/global/globalDefinitions.h"
#include "util/logging/DAS.h"

#define LOG_CHANNEL "RobotState"

namespace Anki {
namespace Vector {

RobotManager::RobotManager(CozmoContext* context)
: _robot(nullptr)
, _context(context)
, _robotEventHandler(context)
, _robotMessageHandler(new RobotInterface::MessageHandler())
{
}

RobotManager::~RobotManager()
{
}

void RobotManager::Init(const Json::Value& config)
{
  auto startTime = std::chrono::steady_clock::now();

  Anki::Util::Time::PushTimedStep("RobotManager::Init");
  _robotMessageHandler->Init(config, this, _context);
  Anki::Util::Time::PopTimedStep(); // RobotManager::Init

  Anki::Util::Time::PrintTimedSteps();
  Anki::Util::Time::ClearSteps();

  auto endTime = std::chrono::steady_clock::now();
  auto timeSpent_millis = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();

  if (ANKI_DEBUG_LEVEL >= ANKI_DEBUG_ERRORS_AND_WARNS)
  {
    constexpr auto maxInitTime_millis = 3000;
    if (timeSpent_millis > maxInitTime_millis)
    {
      LOG_WARNING("RobotManager.Init.TimeSpent",
                  "%lld milliseconds spent initializing, expected %d",
                  timeSpent_millis,
                  maxInitTime_millis);
    }
  }

  PRINT_NAMED_INFO("robot.init.time_spent_ms", "%lld", timeSpent_millis);
}

void RobotManager::Shutdown(ShutdownReason reason)
{
  // Order of destruction matters! Robot actions call back into robot manager, so
  // they must be released before robot manager itself.
  LOG_INFO("RobotManager.Shutdown", "Shutting down");

  if(_robot != nullptr)
  {
    const auto battFilt_mV = static_cast<int>(1000 * _robot->GetBatteryComponent().GetBatteryVolts());
    const auto battRaw_mV  = static_cast<int>(1000 * _robot->GetBatteryComponent().GetBatteryVoltsRaw());

    _robot.reset();

    // SHUTDOWN_UNKNOWN can occur when the process is being stopped
    // so ignore it for the purposes of DAS and fault codes
    if(reason != ShutdownReason::SHUTDOWN_UNKNOWN)
    {
      // Write DAS message
      float idleTime_sec;
      auto upTime_sec   = static_cast<uint32_t>(OSState::getInstance()->GetUptimeAndIdleTime(idleTime_sec));
      auto numFreeBytes = Util::FileUtils::GetDirectoryFreeSize("/data");

      LOG_INFO("Robot.Shutdown.ShuttingDown",
               "Reason: %s, upTime: %u, numFreeBytes: %llu",
               EnumToString(reason), upTime_sec, numFreeBytes);

      DASMSG(robot_power_off, "robot.power_off", "Reason why robot powered off during the previous run");
      DASMSG_SET(s1, EnumToString(reason), "Reason for shutdown");
      DASMSG_SET(i1, upTime_sec,           "Uptime (seconds)");
      DASMSG_SET(i2, numFreeBytes,         "Free space in /data (bytes)");
      DASMSG_SET(i3, battFilt_mV,          "Battery voltage (mV) - filtered");
      DASMSG_SET(i4, battRaw_mV,           "Battery voltage (mV) - raw");
      DASMSG_SEND();
  
      // Send fault code
      // Fault code handler will kill vic-dasMgr and do other stuff as necessary

      // For simplicity, make sure that the ShutdownReason and corresponding
      // FaultCode are named the same.
#define SHUTDOWN_CASE(x) case ShutdownReason::x:        \
      shutdownCode = FaultCode::x;                      \
      break;
      auto shutdownCode = 0;
      switch (reason) {
        SHUTDOWN_CASE(SHUTDOWN_BATTERY_CRITICAL_VOLT)
        SHUTDOWN_CASE(SHUTDOWN_BATTERY_CRITICAL_TEMP)
        SHUTDOWN_CASE(SHUTDOWN_GYRO_NOT_CALIBRATING)
        SHUTDOWN_CASE(SHUTDOWN_BUTTON)
        default:
          LOG_ERROR("Robot.Shutdown.UnknownFaultCode", "reason: %s", EnumToString(reason));
          break;
      }
#undef SHUTDOWN_CASE

      if (shutdownCode != 0) {
        FaultCode::DisplayFaultCode(shutdownCode);
      }
    }
  }
}

void RobotManager::AddRobot(const RobotID_t withID)
{
  if (_robot == nullptr) {
    LOG_INFO("RobotManager.AddRobot.Adding", "Adding robot with ID=%d", withID);
    _robot.reset(new Robot(withID, _context));
    _initialConnection.reset(new RobotInitialConnection(_context));
  } else {
    LOG_WARNING("RobotManager.AddRobot.AlreadyAdded", "Robot already exists. Must remove first.");
  }
}

void RobotManager::RemoveRobot(bool robotRejectedConnection)
{
  if(_robot != nullptr) {
    LOG_INFO("RobotManager.RemoveRobot.Removing", "Removing robot with ID=%d", _robot->GetID());

    if (_initialConnection) {
      const auto result = robotRejectedConnection ? RobotConnectionResult::ConnectionRejected : RobotConnectionResult::ConnectionFailure;
      _initialConnection->HandleDisconnect(result);
    }

    _robot.reset();
    _initialConnection.reset();

    // Clear out the global DAS values that contain the robot hardware IDs.
    Anki::Util::sSetGlobal(DPHYS, nullptr);
    Anki::Util::sSetGlobal(DGROUP, nullptr);
  } else {
    LOG_WARNING("RobotManager.RemoveRobot.NoRobotToRemove", "");
  }
}

// for when you don't care and you just want a damn robot
Robot* RobotManager::GetRobot()
{
  return _robot.get();
}

bool RobotManager::DoesRobotExist(const RobotID_t withID) const
{
  if (_robot) {
    return _robot->GetID() == withID;
  }
  return false;
}


Result RobotManager::UpdateRobot()
{
  ANKI_CPU_PROFILE("RobotManager::UpdateRobot");

  if (_robot)
  {
    _robot->Update();

    if (_robot->HasReceivedRobotState()) {
      _context->GetExternalInterface()->Broadcast(ExternalInterface::MessageEngineToGame(_robot->GetRobotState()));
      _context->GetGatewayInterface()->Broadcast(ExternalMessageRouter::Wrap(_robot->GenerateRobotStateProto()));
    }
    else {
      LOG_PERIODIC_INFO(10, "RobotManager.UpdateRobot",
                        "Not sending robot %d state (none available).", _robot->GetID());
    }

    // If the robot got a message to shutdown
    ShutdownReason shutdownReason = ShutdownReason::SHUTDOWN_UNKNOWN;
    if(_robot->ToldToShutdown(shutdownReason))
    {
      LOG_INFO("RobotManager.UpdateRobot.Shutdown","");
      Shutdown(shutdownReason);
      return RESULT_SHUTDOWN;
    }
  }

  return RESULT_OK;
}

Result RobotManager::UpdateRobotConnection()
{
  ANKI_CPU_PROFILE("RobotManager::UpdateRobotConnection");
  return _robotMessageHandler->ProcessMessages();
}

bool RobotManager::ShouldFilterMessage(const RobotInterface::RobotToEngineTag msgType) const
{
  if (_initialConnection) {
    return _initialConnection->ShouldFilterMessage(msgType);
  }
  return false;
}

bool RobotManager::ShouldFilterMessage(const RobotInterface::EngineToRobotTag msgType) const
{
  if (_initialConnection) {
    return _initialConnection->ShouldFilterMessage(msgType);
  }
  return false;
}

} // namespace Vector
} // namespace Anki
