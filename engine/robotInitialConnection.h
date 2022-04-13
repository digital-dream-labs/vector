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

#ifndef __Anki_Cozmo_Basestation_RobotInitialConnection_H__
#define __Anki_Cozmo_Basestation_RobotInitialConnection_H__

#include "engine/events/ankiEvent.h"
#include "coretech/common/shared/types.h"
#include "clad/types/robotStatusAndActions.h"
#include "util/signals/signalHolder.h"

namespace Anki {
namespace Vector {

namespace RobotInterface {
class MessageHandler;
class RobotToEngine;

enum class RobotToEngineTag : uint8_t;
enum class EngineToRobotTag : uint8_t;
}

class IExternalInterface;
class CozmoContext;
enum class RobotConnectionResult : uint8_t;

class RobotInitialConnection : private Util::SignalHolder
{
public:
  RobotInitialConnection(const CozmoContext* context);

  // called when getting a disconnect message from robot
  // returns true if robot was in the process of connecting and we broadcasted a connection failed message
  bool HandleDisconnect(RobotConnectionResult connectionResult);

  // returns if we should filter out (not deliver) a given message type to/from this robot
  // if firmware doesn't match, we'll do this to almost every message
  bool ShouldFilterMessage(RobotInterface::RobotToEngineTag messageTag) const;
  bool ShouldFilterMessage(RobotInterface::EngineToRobotTag messageTag) const;
  
  void MakeFirmwareUntrusted() { _validFirmware = false; }

private:
  void HandleFactoryFirmware(const AnkiEvent<RobotInterface::RobotToEngine>&);
  void HandleFirmwareVersion(const AnkiEvent<RobotInterface::RobotToEngine>&);
  void HandleRobotAvailable(const AnkiEvent<RobotInterface::RobotToEngine>&);
  void OnNotified(RobotConnectionResult result);
  void SendConnectionResponse(RobotConnectionResult result);

  bool _notified;
  IExternalInterface* _externalInterface;
  const CozmoContext* _context;
  RobotInterface::MessageHandler* _robotMessageHandler;
  bool _validFirmware;
};

}
}

#endif
