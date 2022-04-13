/**
 * File: robotInitialConnection
 *
 * Author: baustin
 * Created: 8/1/2016
 *
 * Description: Monitors initial events after robot connection to determine result to report
 *
 * Copyright: Anki, Inc. 2016
 *
 **/

#include "engine/cozmoContext.h"
#include "engine/perfMetricEngine.h"
#include "engine/robotInitialConnection.h"
#include "engine/robotManager.h"
#include "engine/externalInterface/externalInterface.h"
#include "engine/robotInterface/messageHandler.h"
#include "engine/utils/cozmoExperiments.h"
#include "clad/externalInterface/messageEngineToGame.h"
#include "clad/robotInterface/messageEngineToRobot.h"
#include "util/console/consoleInterface.h"
#include "util/logging/logging.h"
#include <json/json.h>

#include "util/string/stringUtils.h"

namespace Anki {
namespace Vector {

namespace RobotInitialConnectionConsoleVars
{
  CONSOLE_VAR(bool, kSkipFirmwareAutoUpdate, "Firmware", false);
  CONSOLE_VAR(bool, kAlwaysDoFirmwareUpdate, "Firmware", false);
}

using namespace ExternalInterface;
using namespace RobotInterface;
using namespace RobotInitialConnectionConsoleVars;

RobotInitialConnection::RobotInitialConnection(const CozmoContext* context)
: _notified(false)
, _externalInterface(context != nullptr ? context->GetExternalInterface() : nullptr)
, _context(context)
, _robotMessageHandler(context != nullptr ? context->GetRobotManager()->GetMsgHandler() : nullptr)
, _validFirmware(false) // guilty until proven innocent
{
  if (_externalInterface == nullptr) {
    return;
  }

  auto handleAvailableFunc = std::bind(&RobotInitialConnection::HandleRobotAvailable, this, std::placeholders::_1);
  AddSignalHandle(_robotMessageHandler->Subscribe(RobotToEngineTag::robotAvailable, handleAvailableFunc));
}

bool RobotInitialConnection::ShouldFilterMessage(RobotToEngineTag messageTag) const
{
  if (_validFirmware) {
    return false;
  }

  switch (messageTag) {
    // these messages are ok on outdated firmware
    case RobotToEngineTag::robotAvailable:
      return false;

    default:
      return true;
  }
}

bool RobotInitialConnection::ShouldFilterMessage(EngineToRobotTag messageTag) const
{
  if (_validFirmware) {
    return false;
  }

  return true;
}

bool RobotInitialConnection::HandleDisconnect(RobotConnectionResult connectionResult)
{
  if (_notified || _externalInterface == nullptr) {
    return false;
  }

  PRINT_NAMED_INFO("RobotInitialConnection.HandleDisconnect", "robot connection failed due to %s", RobotConnectionResultToString(connectionResult));

  OnNotified(connectionResult);
  return true;
}

void RobotInitialConnection::HandleFactoryFirmware(const AnkiEvent<RobotToEngine>&)
{
  if (_notified || _externalInterface == nullptr) {
    return;
  }

  PRINT_NAMED_INFO("RobotInitialConnection.HandleFactoryFirmware", "robot has factory firmware");

  const auto result = RobotConnectionResult::OutdatedFirmware;
  OnNotified(result);
}

void RobotInitialConnection::HandleRobotAvailable(const AnkiEvent<RobotInterface::RobotToEngine>& message)
{
  if (_notified || _externalInterface == nullptr) {
    return;
  }

  OnNotified(RobotConnectionResult::Success);

  // In general, with PerfMetric's 'auto record', we are not interested in frames until we are fully running
  const auto pm = _context->GetPerfMetric();
  if (pm->GetAutoRecord())
  {
    pm->Start();
  }
}

void RobotInitialConnection::OnNotified(RobotConnectionResult result)
{
  switch (result) {
    case RobotConnectionResult::OutdatedFirmware:
    case RobotConnectionResult::OutdatedApp:
      _validFirmware = false;
      break;
    default:
      _validFirmware = true;
      break;
  }

  SendConnectionResponse(result);
}

void RobotInitialConnection::SendConnectionResponse(RobotConnectionResult result)
{
  _notified = true;
  ClearSignalHandles();

  _externalInterface->Broadcast(MessageEngineToGame{RobotConnectionResponse{result}});
}

}
}
