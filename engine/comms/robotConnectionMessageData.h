/**
* File: RobotConnectionMessageData
*
* Author: Lee Crippen
* Created: 7/6/2016
*
* Description: A message arrived from the robot connection
*
* Copyright: Anki, inc. 2016
*
*/
#ifndef __Cozmo_Basestation_Comms_RobotConnectionMessageData_H_
#define __Cozmo_Basestation_Comms_RobotConnectionMessageData_H_

#include "util/transport/netTimeStamp.h"
#include "util/transport/transportAddress.h"
#include "util/math/numericCast.h"

#include <vector>
#include <cstdint>

#define TRACK_INCOMING_PACKET_LATENCY 1
#if TRACK_INCOMING_PACKET_LATENCY
#define TRACK_INCOMING_PACKET_LATENCY_TIMESTAMP_MS() ::Anki::Util::GetCurrentNetTimeStamp()
#else
#define TRACK_INCOMING_PACKET_LATENCY_TIMESTAMP_MS() ::Anki::Util::kNetTimeStampZero
#endif

namespace Anki {
namespace Vector {
  
enum class RobotConnectionMessageType
{
  Data,
  ConnectionRequest,
  ConnectionResponse,
  Disconnect
};

class RobotConnectionMessageData {
public:
  
  RobotConnectionMessageData() { };
  
  // This constructor handles messages with data
  RobotConnectionMessageData(const uint8_t* data, uint32_t numBytes, const Util::TransportAddress& address, Util::NetTimeStamp timeReceived_ms)
  : _rawMessageData(data, data + numBytes)
  , _address(address)
#if TRACK_INCOMING_PACKET_LATENCY
  , _timeReceived_ms(timeReceived_ms)
#endif // TRACK_INCOMING_PACKET_LATENCY
  { }
  
  // This constructor handles messages with information about connection state
  RobotConnectionMessageData(RobotConnectionMessageType newType, const Util::TransportAddress& address, Util::NetTimeStamp timeReceived_ms)
  : _messageType(newType)
  , _address(address)
#if TRACK_INCOMING_PACKET_LATENCY
  , _timeReceived_ms(timeReceived_ms)
#endif // TRACK_INCOMING_PACKET_LATENCY
  { }
  
  RobotConnectionMessageType GetType() const { return _messageType; }
  // Allows access to the stored data for std::move purposes, so is non-const
  std::vector<uint8_t>& GetData() { return _rawMessageData; }

  // Returns the size in bytes of the raw message data plus overhead
  uint32_t GetMemorySize() const { return Util::numeric_cast<uint32_t>(
      _rawMessageData.capacity() * sizeof(decltype(_rawMessageData)::value_type) + sizeof(*this) ); }
  
  const Util::TransportAddress& GetAddress() const { return _address; }
#if TRACK_INCOMING_PACKET_LATENCY
  Util::NetTimeStamp GetTimeReceived() const { return _timeReceived_ms; }
#endif // TRACK_INCOMING_PACKET_LATENCY
  
private:
  RobotConnectionMessageType  _messageType = RobotConnectionMessageType::Data;
  std::vector<uint8_t>        _rawMessageData;
  Util::TransportAddress      _address;
  
#if TRACK_INCOMING_PACKET_LATENCY
  Util::NetTimeStamp          _timeReceived_ms = Util::kNetTimeStampZero;
#endif // TRACK_INCOMING_PACKET_LATENCY
};

} // end namespace Vector
} // end namespace Anki


#endif //__Cozmo_Basestation_Comms_RobotConnectionMessageData_H_
