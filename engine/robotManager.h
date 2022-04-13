//
//  robotManager.h
//  Products_Cozmo
//
//     RobotManager class for keeping up with available robots, by their ID.
//
//  Created by Andrew Stein on 8/23/13.
//  Copyright (c) 2013 Anki, Inc. All rights reserved.
//

#ifndef ANKI_COZMO_BASESTATION_ROBOTMANAGER_H
#define ANKI_COZMO_BASESTATION_ROBOTMANAGER_H

#include "engine/robotEventHandler.h"
#include "clad/types/robotStatusAndActions.h"
#include "util/helpers/noncopyable.h"
#include <memory>


namespace Json {
  class Value;
}

namespace Anki {

namespace Vector {

// Forward declarations:
namespace RobotInterface {
class MessageHandler;
enum class EngineToRobotTag : uint8_t;
enum class RobotToEngineTag : uint8_t;
}
class Robot;
class IExternalInterface;
class CozmoContext;
class RobotInitialConnection;

class RobotManager : Util::noncopyable
{
public:

  RobotManager(CozmoContext* context);

  ~RobotManager();

  void Init(const Json::Value& config);
  void Shutdown(ShutdownReason reason);

  // Return raw pointer to robot
  Robot* GetRobot();

  // Check whether a robot exists
  bool DoesRobotExist(const RobotID_t withID) const;

  // Add / remove robot
  void AddRobot(const RobotID_t withID);
  void RemoveRobot(bool robotRejectedConnection);

  // Call Robot's Update() function
  Result UpdateRobot();

  // Update robot connection state
  Result UpdateRobotConnection();

  RobotInterface::MessageHandler* GetMsgHandler() const { return _robotMessageHandler.get(); }
  RobotEventHandler& GetRobotEventHandler() { return _robotEventHandler; }

  bool ShouldFilterMessage(RobotInterface::RobotToEngineTag msgType) const;
  bool ShouldFilterMessage(RobotInterface::EngineToRobotTag msgType) const;

protected:
  std::unique_ptr<Robot> _robot;
  CozmoContext* _context;
  RobotEventHandler _robotEventHandler;
  std::unique_ptr<RobotInterface::MessageHandler> _robotMessageHandler;
  std::unique_ptr<RobotInitialConnection> _initialConnection;

private:

}; // class RobotManager

} // namespace Vector
} // namespace Anki


#endif // ANKI_COZMO_BASESTATION_ROBOTMANAGER_H
