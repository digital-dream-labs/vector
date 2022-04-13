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
#include "engine/comms/robotConnectionData.h"
#include "util/logging/logging.h"


namespace Anki {
namespace Vector {

namespace {
static const uint32_t kQueueSizeWarningThreshold = 1024 * 1024;
}
  
bool RobotConnectionData::HasMessages()
{
  std::lock_guard<std::mutex> lockGuard(_messageMutex);
  return !_arrivedMessages.empty();
}
  
RobotConnectionMessageData RobotConnectionData::PopNextMessage()
{
  std::unique_lock<std::mutex> lockGuard(_messageMutex);
  
  DEV_ASSERT(!_arrivedMessages.empty(), "RobotConnectionData.PopNextMessage.NoMessages");
  if (_arrivedMessages.empty())
  {
    return RobotConnectionMessageData();
  }
  
  RobotConnectionMessageData nextMessage = std::move(_arrivedMessages.front());
  const uint32_t nextMessageSize = nextMessage.GetMemorySize();
  const bool queueSizeOK = _queueSize >= nextMessageSize;

  if( queueSizeOK ) {
    _queueSize -= nextMessageSize;
  }
  else {
    _queueSize = 0;
    // error will be printed below (outside lock)
  }
  
  _arrivedMessages.pop_front();
  
  // unlock mutex before doing logging and statistics
  lockGuard.unlock();

  ANKI_VERIFY(queueSizeOK, "RobotConnectionMessageData.PopNextMessage.NegativeSize",
              "Tracked queue size has gone negative! This is a bug");
  
  UpdateQueueSizeStatistics();

  return nextMessage;
}

void RobotConnectionData::PushArrivedMessage(const uint8_t* buffer, uint32_t numBytes, const Util::TransportAddress& address)
{
  RobotConnectionMessageType messageType = RobotConnectionMessageType::Data;
  if (Util::INetTransportDataReceiver::OnConnectRequest == buffer)
  {
    messageType = RobotConnectionMessageType::ConnectionRequest;
  }
  else if (Util::INetTransportDataReceiver::OnConnected == buffer)
  {
    messageType = RobotConnectionMessageType::ConnectionResponse;
  }
  else if (Util::INetTransportDataReceiver::OnDisconnected == buffer)
  {
    messageType = RobotConnectionMessageType::Disconnect;
  }

  {
    std::lock_guard<std::mutex> lockGuard(_messageMutex);
    if (messageType == RobotConnectionMessageType::Data)
    {
      // Note we don't bother passing the MessageType::Data into this constructor because that's the default
      _arrivedMessages.emplace_back(buffer, numBytes, address, TRACK_INCOMING_PACKET_LATENCY_TIMESTAMP_MS());
    }
    else
    {
      _arrivedMessages.emplace_back(messageType, address, TRACK_INCOMING_PACKET_LATENCY_TIMESTAMP_MS());
    }
    _queueSize += _arrivedMessages.back().GetMemorySize();
  }

  UpdateQueueSizeStatistics();    
}
  
void RobotConnectionData::ReceiveData(const uint8_t* buffer, unsigned int size, const Util::TransportAddress& sourceAddress)
{
  const bool isConnectionRequest = INetTransportDataReceiver::OnConnectRequest == buffer;
  DEV_ASSERT(!isConnectionRequest, "RobotConnectionManager.ReceiveData.ConnectionRequest.NotHandled");
  if (isConnectionRequest)
  {
    // We don't accept requests for connection!
    return;
  }
  
  // Otherwise we hold onto the message
  PushArrivedMessage(buffer, size, sourceAddress);
}
  
void RobotConnectionData::Clear()
{
  _currentState = State::Disconnected;
  _address = {};

  {
    std::lock_guard<std::mutex> lockGuard(_messageMutex);
    _arrivedMessages.clear();
    _queueSize = 0;
  }
  
  UpdateQueueSizeStatistics();
}
  
void RobotConnectionData::QueueConnectionDisconnect()
{
  PushArrivedMessage(Util::INetTransportDataReceiver::OnDisconnected, 0, _address);
}

uint32_t RobotConnectionData::GetIncomingQueueSize()
{
  std::lock_guard<std::mutex> lockGuard(_messageMutex);
  uint32_t queueSize = _queueSize;
  return queueSize;
}

void RobotConnectionData::UpdateQueueSizeStatistics()
{
  std::unique_lock<std::mutex> lockGuard(_messageMutex);
  uint32_t queueSize = _queueSize;

  if( queueSize > _maxQueueSize ) {
    _maxQueueSize = queueSize;
  }

  uint32_t maxQueueSize = _maxQueueSize;

  const bool inWarningZone = queueSize > kQueueSizeWarningThreshold;
  const bool shouldSetWarning = inWarningZone && !_hasSizeWarning;
  const bool shouldClearWarning = !inWarningZone && _hasSizeWarning;

  if( shouldSetWarning ) {
    _hasSizeWarning = true;
  }
  else if ( shouldClearWarning ) {
    _hasSizeWarning = false;
  }

  lockGuard.unlock();

  if( shouldSetWarning ) {
    PRINT_NAMED_WARNING("RobotConnectionManager.ArrivedMessageQueue.QueueTooLarge",
                        "Queue size is %u bytes",
                        queueSize);

  }
  else if( shouldClearWarning ) {
    // we're out of the warning zone now. Send up another warning to signify this
    PRINT_NAMED_WARNING("RobotConnectionManager.ArrivedMessageQueue.QueueNoLongerTooLarge",
                        "Queue size is down to %u bytes. Max this run is %u",
                        queueSize,
                        maxQueueSize);
  }
}
    

} // end namespace Vector
} // end namespace Anki
