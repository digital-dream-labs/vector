/**
* File: messageHandler
*
* Author: damjan stulic
* Created: 9/8/15
*
* Description:
*
* Copyright: Anki, inc. 2015
*
*/


#include "engine/robotInterface/messageHandler.h"
#include "engine/actions/actionContainers.h"
#include "engine/ankiEventUtil.h"
#include "engine/comms/robotConnectionManager.h"
#include "engine/cozmoContext.h"
#include "engine/debug/devLoggingSystem.h"
#include "engine/robot.h"
#include "engine/robotManager.h"
#include "engine/utils/parsingConstants/parsingConstants.h"
#include "anki/cozmo/shared/cozmoConfig.h"
#include "coretech/common/engine/utils/timer.h"
#include "coretech/messaging/engine/IComms.h"
#include "clad/externalInterface/messageGameToEngine.h"
#include "clad/robotInterface/messageRobotToEngine.h"
#include "clad/robotInterface/messageEngineToRobot.h"
#include "json/json.h"
#include "util/cpuProfiler/cpuProfiler.h"
#include "util/global/globalDefinitions.h"
#include "util/transport/transportAddress.h"

namespace Anki {
namespace Vector {
namespace RobotInterface {

MessageHandler::MessageHandler()
: _robotManager(nullptr)
, _isInitialized(false)
, _messageCountRobotToEngine(0)
, _messageCountEngineToRobot(0)
{
}

MessageHandler::~MessageHandler()
{
  #if ANKI_PROFILE_ENGINE_SOCKET_BUFFER_STATS
  if (_robotConnectionManager) {
    _robotConnectionManager->ReportSocketBufferStats();
  }
  #endif
}

void MessageHandler::Init(const Json::Value& config, RobotManager* robotMgr, const CozmoContext* context)
{
  _robotManager = robotMgr;
  _robotConnectionManager = std::make_unique<RobotConnectionManager>(_robotManager);
  _robotConnectionManager->Init();

  #if ANKI_PROFILE_ENGINE_SOCKET_BUFFER_STATS
  _robotConnectionManager->InitSocketBufferStats();
  #endif

  _isInitialized = true;
}

Result MessageHandler::ProcessMessages()
{
  ANKI_CPU_PROFILE("MessageHandler::ProcessMessages");

  if (_isInitialized)
  {
    DEV_ASSERT(_robotConnectionManager, "MessageHander.ProcessMessages.InvalidRobotConnectionManager");

    #if ANKI_PROFILE_ENGINE_SOCKET_BUFFER_STATS
    _robotConnectionManager->UpdateSocketBufferStats();
    #endif

    Result result = _robotConnectionManager->Update();
    if (RESULT_OK != result) {
      LOG_ERROR("MessageHandler.ProcessMessages", "Unable to update robot connection (result %d)", result);
      return result;
    }

    std::vector<uint8_t> nextData;
    while (_robotConnectionManager->PopData(nextData))
    {
      ++_messageCountRobotToEngine;

      // If we don't have a robot to care about this message, throw it away
      Robot* destRobot = _robotManager->GetRobot();
      if (nullptr == destRobot)
      {
        continue;
      }

      const size_t dataSize = nextData.size();
      if (dataSize <= 0)
      {
        PRINT_NAMED_ERROR("MessageHandler.ProcessMessages","Tried to process message of invalid size");
        continue;
      }

      // see if message type should be filtered out based on potential firmware mismatch
      const RobotInterface::RobotToEngineTag msgType = static_cast<RobotInterface::RobotToEngineTag>(nextData.data()[0]);
      if (_robotManager->ShouldFilterMessage(msgType)) {
        continue;
      }

      RobotInterface::RobotToEngine message;
      const size_t unpackSize = message.Unpack(nextData.data(), dataSize);
      if (unpackSize != nextData.size()) {
        PRINT_NAMED_ERROR("RobotMessageHandler.MessageUnpack", "Message unpack error, tag %s expecting %zu but have %zu",
                          RobotToEngineTagToString(msgType), unpackSize, dataSize);
        continue;
      }

      #if ANKI_DEV_CHEATS
      if (nullptr != DevLoggingSystem::GetInstance())
      {
        DevLoggingSystem::GetInstance()->LogMessage(message);
      }
      #endif
      Broadcast(std::move(message));
    }

    #if ANKI_PROFILE_ENGINE_SOCKET_BUFFER_STATS
    _robotConnectionManager->UpdateSocketBufferStats();
    #endif

  }

  return RESULT_OK;
}

Result MessageHandler::SendMessage(const RobotInterface::EngineToRobot& msg, bool reliable, bool hot)
{
  ++_messageCountEngineToRobot;

  if (!_isInitialized || !_robotConnectionManager->IsValidConnection())
  {
    return RESULT_FAIL;
  }

  if (_robotManager->ShouldFilterMessage(msg.GetTag()))
  {
    return RESULT_FAIL;
  }

#if ANKI_DEV_CHEATS
  if (nullptr != DevLoggingSystem::GetInstance())
  {
    DevLoggingSystem::GetInstance()->LogMessage(msg);
  }
#endif

  const auto expectedSize = msg.Size();
  std::vector<uint8_t> messageData(msg.Size());
  const auto packedSize = msg.Pack(messageData.data(), expectedSize);
  DEV_ASSERT(packedSize == expectedSize, "MessageHandler.SendMessage.MessageSizeMismatch");
  if (packedSize != expectedSize)
  {
    return RESULT_FAIL;
  }

  const bool success = _robotConnectionManager->SendData(messageData.data(), Util::numeric_cast<unsigned int>(packedSize));
  if (!success)
  {
    return RESULT_FAIL;
  }

  return RESULT_OK;
}

void MessageHandler::Broadcast(const RobotInterface::RobotToEngine& message)
{
  ANKI_CPU_PROFILE("Broadcast_R2E");

  u32 type = static_cast<u32>(message.GetTag());
  _eventMgr.Broadcast(AnkiEvent<RobotInterface::RobotToEngine>(BaseStationTimer::getInstance()->GetCurrentTimeInSeconds(), type, message));
}

void MessageHandler::Broadcast(RobotInterface::RobotToEngine&& message)
{
  ANKI_CPU_PROFILE("Broadcast_R2E");

  u32 type = static_cast<u32>(message.GetTag());
  _eventMgr.Broadcast(AnkiEvent<RobotInterface::RobotToEngine>(BaseStationTimer::getInstance()->GetCurrentTimeInSeconds(), type, std::move(message)));
}

bool MessageHandler::IsConnected(RobotID_t robotID)
{
  return _robotConnectionManager->IsConnected(robotID);
}

Result MessageHandler::AddRobotConnection(RobotID_t robotId)
{
  return _robotConnectionManager->Connect(robotId);
}

void MessageHandler::Disconnect()
{
  _robotConnectionManager->DisconnectCurrent();
}

const Util::Stats::StatsAccumulator& MessageHandler::GetQueuedTimes_ms() const
{
  return _robotConnectionManager->GetQueuedTimes_ms();
}

} // end namespace RobotInterface
} // end namespace Vector
} // end namespace Anki
