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

#ifndef __Anki_Cozmo_Basestation_RobotInterface_MessageHandler_H__
#define __Anki_Cozmo_Basestation_RobotInterface_MessageHandler_H__

#include "engine/events/ankiEventMgr.h"
#include "coretech/common/shared/types.h"
#include "clad/robotInterface/messageEngineToRobot.h"
#include "clad/robotInterface/messageRobotToEngine.h"
#include "util/signals/simpleSignal_fwd.h"
#include <memory>

namespace Json {
  class Value;
}

namespace Anki {

namespace Comms {
class IChannel;
struct IncomingPacket;
}
  
namespace Util {
  class TransportAddress;
  namespace Stats {
    class StatsAccumulator;
  }
}

namespace Vector {

class RobotManager;
class CozmoContext;
class RobotConnectionManager;

namespace RobotInterface {

class MessageHandler {
public:

  MessageHandler();
  virtual ~MessageHandler();

  virtual void Init(const Json::Value& config, RobotManager* robotMgr, const CozmoContext* context);

  virtual Result ProcessMessages();

  virtual Result SendMessage(const RobotInterface::EngineToRobot& msg, bool reliable = true, bool hot = false);

  Signal::SmartHandle Subscribe(const RobotInterface::RobotToEngineTag& tagType, std::function<void(const AnkiEvent<RobotInterface::RobotToEngine>&)> messageHandler) {
    return _eventMgr.Subscribe(static_cast<uint32_t>(tagType), messageHandler);
  }
  
  // Handle various event message types
  template<typename T>
  void HandleMessage(const T& msg);

  // Are we connected to this robot?
  bool IsConnected(RobotID_t robotID);

  Result AddRobotConnection(RobotID_t robotId);
  
  void Disconnect();
  
  const Util::Stats::StatsAccumulator& GetQueuedTimes_ms() const;

  uint32_t GetMessageCountRtE() const { return _messageCountRobotToEngine; }
  uint32_t GetMessageCountEtR() const { return _messageCountEngineToRobot; }
  void     ResetMessageCounts() { _messageCountRobotToEngine = 0; _messageCountEngineToRobot = 0; }

protected:
  void Broadcast(const RobotInterface::RobotToEngine& message);
  void Broadcast(RobotInterface::RobotToEngine&& message);
  
private:
  AnkiEventMgr<RobotInterface::RobotToEngine> _eventMgr;
  RobotManager* _robotManager;
  std::unique_ptr<RobotConnectionManager> _robotConnectionManager;
  bool _isInitialized;
  std::vector<Signal::SmartHandle> _signalHandles;
  uint32_t _messageCountRobotToEngine = 0;
  uint32_t _messageCountEngineToRobot = 0;
};


} // end namespace RobotInterface
} // end namespace Vector
} // end namespace Anki



#endif //__Anki_Cozmo_Basestation_RobotInterface_MessageHandler_H__
