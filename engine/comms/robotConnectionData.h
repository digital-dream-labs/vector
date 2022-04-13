/**
* File: RobotConnectionData
*
* Author: Lee Crippen
* Created: 7/6/2016
*
* Description: Has data related to a robot connection
*
* Copyright: Anki, inc. 2016
*
*/
#ifndef __Cozmo_Basestation_Comms_RobotConnectionData_H_
#define __Cozmo_Basestation_Comms_RobotConnectionData_H_

#include "engine/comms/robotConnectionMessageData.h"
#include "util/helpers/noncopyable.h"
#include "util/transport/iNetTransportDataReceiver.h"
#include "util/transport/transportAddress.h"

#include <deque>
#include <mutex>

namespace Anki {
namespace Vector {

class RobotConnectionData : Util::noncopyable, public Anki::Util::INetTransportDataReceiver {
public:
  enum class State
  {
    Disconnected,
    Waiting,
    Connected
  };
  
  State GetState() const { return _currentState; }
  void SetState(State newState) { _currentState = newState; }
  
  // These next members access arrived messages and use the associated mutex
  bool HasMessages();
  RobotConnectionMessageData PopNextMessage();
  void PushArrivedMessage(const uint8_t* buffer, uint32_t numBytes, const Util::TransportAddress& address);
  void Clear();
  void QueueConnectionDisconnect();

  // Return the current size of the incoming message queue in bytes
  uint32_t GetIncomingQueueSize();
  
  Util::TransportAddress GetAddress() const { return _address; };
  void SetAddress(const Util::TransportAddress& address) { _address = address; }
    
  virtual void ReceiveData(const uint8_t* buffer, unsigned int size, const Util::TransportAddress& sourceAddress) override;
    
private:
  State                                   _currentState = State::Disconnected;
  std::deque<RobotConnectionMessageData>  _arrivedMessages;
  uint32_t                                _queueSize = 0;
  uint32_t                                _maxQueueSize = 0;
  std::mutex                              _messageMutex;
  Util::TransportAddress                  _address;
  bool                                    _hasSizeWarning = false;

  // call after updating _queueSize to do some logging
  void UpdateQueueSizeStatistics();
};

} // end namespace Vector
} // end namespace Anki


#endif //__Cozmo_Basestation_Comms_RobotConnectionData_H_
