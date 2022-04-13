/**
* File: RobotConnectionManager
*
* Author: Lee Crippen
* Created: 7/6/2016
*
* Description: Holds onto current RobotConnections
*
* Copyright: Anki, inc. 2016
*
*/
#include "engine/comms/robotConnectionManager.h"
#include "engine/actions/actionContainers.h"
#include "engine/comms/robotConnectionData.h"

#include "engine/robot.h"
#include "engine/robotManager.h"

#include "anki/cozmo/shared/cozmoConfig.h"
#include "coretech/messaging/shared/socketConstants.h"

#include "util/cpuProfiler/cpuProfiler.h"
#include "util/histogram/histogram.h"
#include "util/logging/DAS.h"
#include "util/logging/logging.h"

#define LOG_CHANNEL "RobotConnectionManager"

// Maximum size of one message
#define MAX_PACKET_BUFFER_SIZE 2048


namespace Anki {
namespace Vector {

namespace {
static const int kNumQueueSizeStatsToSendToDas = 4000;
}

RobotConnectionManager::RobotConnectionManager(RobotManager* robotManager)
: _currentConnectionData(new RobotConnectionData())
, _robotManager(robotManager)
{

}

RobotConnectionManager::~RobotConnectionManager()
{
  DisconnectCurrent();
}

void RobotConnectionManager::Init()
{
}

Result RobotConnectionManager::Update()
{
  ANKI_CPU_PROFILE("RobotConnectionManager::Update");

  // Update queue stats before processing messages so we get stats about how big the queue was prior to it
  // being cleared
  if( _queueSizeAccumulator.GetNum() >= kNumQueueSizeStatsToSendToDas ) {
    SendAndResetQueueStats();
  }

  // If we lose connection to robot, report connection closed
  if (!_udpClient.IsConnected()) {
    return RESULT_FAIL_IO_CONNECTION_CLOSED;
  }

  ProcessArrivedMessages();

  return RESULT_OK;
}

void RobotConnectionManager::SendAndResetQueueStats()
{
  // Note: This used to be the DAS message "robot.msg_queue.recent_incoming_size" but was demoted to an INFO since it
  // was spamming DAS
  LOG_INFO("RobotConnectionManager.SendAndResetQueueStats.Stats",
           "num %d, min %.2f, mean %.2f, max %.2f",
           _queueSizeAccumulator.GetNum(),
           _queueSizeAccumulator.GetMin(),
           _queueSizeAccumulator.GetMean(),
           _queueSizeAccumulator.GetMax());

  // clear accumulator so we only send recent stats
  _queueSizeAccumulator.Clear();
}

bool RobotConnectionManager::IsConnected(RobotID_t robotID) const
{
  if (_robotID == robotID && _udpClient.IsConnected()) {
    return true;
  }
  return false;
}

Result RobotConnectionManager::Connect(RobotID_t robotID)
{
  // LOG_DEBUG("RobotConnectionManager.Connect", "Connect to robot %d", robotID);

  _currentConnectionData->Clear();

  if (_udpClient.IsConnected()) {
    _udpClient.Disconnect();
  }

  const std::string & client_path = ENGINE_ANIM_CLIENT_PATH + std::to_string(robotID);
  const std::string & server_path = ENGINE_ANIM_SERVER_PATH + std::to_string(robotID);

  const bool ok = _udpClient.Connect(client_path, server_path);
  if (!ok) {
    LOG_WARNING("RobotConnectionManager.Connect", "Unable to connect from %s to %s",
                client_path.c_str(), server_path.c_str());
    _currentConnectionData->SetState(RobotConnectionData::State::Disconnected);
    return RESULT_FAIL_IO;
  }

  _robotID = robotID;
  _currentConnectionData->SetState(RobotConnectionData::State::Connected);

  return RESULT_OK;
}

void RobotConnectionManager::DisconnectCurrent()
{
  LOG_DEBUG("RobotConnectionManager.DisconnectCurrent", "Disconnect");
  if (_udpClient.IsConnected()) {
    _udpClient.Disconnect();
    _robotID = -1;
  }

  _currentConnectionData->SetState(RobotConnectionData::State::Disconnected);

  // send connection stats data if there is any
  if( _queueSizeAccumulator.GetNum() > 0 ) {
    SendAndResetQueueStats();
  }
}

bool RobotConnectionManager::SendData(const uint8_t* buffer, unsigned int size)
{
  const bool validState = IsValidConnection();
  if (!validState)
  {
    LOG_DEBUG("RobotConnectionManager.SendData.NotValidState", "Not connected");
    return false;
  }

  const ssize_t sent = _udpClient.Send((const char *) buffer, size);
  if (sent != size) {
    LOG_ERROR("RobotConnectionManager.SendData.Error", "Sent %zd/%d bytes to robot", sent, size);
    DisconnectCurrent();
    return false;
  }

  return true;
}

void RobotConnectionManager::ProcessArrivedMessages()
{
  static const Util::TransportAddress addr;
  while (_udpClient.IsConnected()) {
    char buf[MAX_PACKET_BUFFER_SIZE];
    const ssize_t n = _udpClient.Recv(buf, sizeof(buf));
    if (n < 0) {
      LOG_ERROR("RobotConnectionManager.ProcessArrivedMessages", "Read error from robot");
      break;
    } else if (n == 0) {
      //LOG_DEBUG("RobotConnectionManager.ProcessArrivedMessages", "Nothing to read");
      break;
    } else {
      //LOG_DEBUG("RobotConnectionManager.ProcessArrivedMessages", "Read %zd/%lu from robot", n, sizeof(buf));
      _currentConnectionData->PushArrivedMessage((const uint8_t *) buf, (uint32_t) n, addr);
    }
  }

  while (_currentConnectionData->HasMessages())
  {
    RobotConnectionMessageData nextMessage = _currentConnectionData->PopNextMessage();

#if TRACK_INCOMING_PACKET_LATENCY
    const auto& timeReceived = nextMessage.GetTimeReceived();
    if (timeReceived != Util::kNetTimeStampZero)
    {
      const double timeQueued_ms = Util::GetCurrentNetTimeStamp() - timeReceived;
      _queuedTimes_ms.AddStat(timeQueued_ms);
    }
#endif

    _queueSizeAccumulator += _currentConnectionData->GetIncomingQueueSize();

    if (RobotConnectionMessageType::Data == nextMessage.GetType())
    {
      HandleDataMessage(nextMessage);
    }
    else if (RobotConnectionMessageType::ConnectionResponse == nextMessage.GetType())
    {
      HandleConnectionResponseMessage(nextMessage);
    }
    else if (RobotConnectionMessageType::Disconnect == nextMessage.GetType())
    {
      HandleDisconnectMessage(nextMessage);
    }
    else if (RobotConnectionMessageType::ConnectionRequest == nextMessage.GetType())
    {
      HandleConnectionRequestMessage(nextMessage);
    }
    else
    {
      LOG_ERROR("RobotConnectionManager.ProcessArrivedMessages.UnhandledMessageType",
                "Unhandled message type %d. Ignoring", nextMessage.GetType());
    }
  }
}

void RobotConnectionManager::HandleDataMessage(RobotConnectionMessageData& nextMessage)
{
  const bool isConnected = IsValidConnection();
  if (!isConnected)
  {
    LOG_INFO("RobotConnectionManager.HandleDataMessage.NotValidState", "Connection not yet valid, dropping message");
    return;
  }

  const bool correctAddress = _currentConnectionData->GetAddress() == nextMessage.GetAddress();
  if (!correctAddress)
  {
    LOG_ERROR("RobotConnectionManager.HandleDataMessage.IncorrectAddress",
              "Expected messages from %s but arrived from %s. Dropping message.",
              _currentConnectionData->GetAddress().ToString().c_str(),
              nextMessage.GetAddress().ToString().c_str());
    return;
  }

  _readyData.push_back(std::move(nextMessage.GetData()));
}

void RobotConnectionManager::HandleConnectionResponseMessage(RobotConnectionMessageData& nextMessage)
{
  LOG_DEBUG("RobotConnectionManager.HandleConnectionResponseMessage", "Handle connection response");

  const bool isWaitingState = _currentConnectionData->GetState() == RobotConnectionData::State::Waiting;
  if (!isWaitingState)
  {
    LOG_ERROR("RobotConnectionManager.HandleConnectionResponseMessage.NotWaitingForConnection",
              "Got connection response at unexpected time");
    return;
  }

  _currentConnectionData->SetState(RobotConnectionData::State::Connected);
}

void RobotConnectionManager::HandleDisconnectMessage(RobotConnectionMessageData& nextMessage)
{
  LOG_DEBUG("RobotConnectionManager.HandleDisconnectMessage", "Handle disconnect");

  const bool connectionWasInWaitingState = (RobotConnectionData::State::Waiting == _currentConnectionData->GetState());

  // This connection is no longer valid.
  // Note not calling DisconnectCurrent because this message means reliableTransport is already deleting this connection data
  _currentConnectionData->Clear();

  // This robot is gone.
  Robot* robot = _robotManager->GetRobot();
  if (nullptr != robot)
  {
    // If the connection is waiting when we handle this disconnect message, report it as a robot rejection
    _robotManager->RemoveRobot(connectionWasInWaitingState);
  }
}

void RobotConnectionManager::HandleConnectionRequestMessage(RobotConnectionMessageData& nextMessage)
{
  LOG_WARNING("RobotConnectionManager.HandleConnectionRequestMessage",
              "Received connection request from %s. Ignoring",
              nextMessage.GetAddress().ToString().c_str());
}

bool RobotConnectionManager::IsValidConnection() const
{
  return _currentConnectionData->GetState() == RobotConnectionData::State::Connected;
}

bool RobotConnectionManager::PopData(std::vector<uint8_t>& data_out)
{
  if (_readyData.empty()) {
    return false;
  }

  data_out = std::move(_readyData.front());
  _readyData.pop_front();
  return true;
}

void RobotConnectionManager::ClearData()
{
  _readyData.clear();
}

const Anki::Util::Stats::StatsAccumulator& RobotConnectionManager::GetQueuedTimes_ms() const
{
#if TRACK_INCOMING_PACKET_LATENCY
  return _queuedTimes_ms.GetPrimaryAccumulator();
#else
  static Anki::Util::Stats::StatsAccumulator sNullStats;
  return sNullStats;
#endif // TRACK_INCOMING_PACKET_LATENCY
}

#if ANKI_PROFILE_ENGINE_SOCKET_BUFFER_STATS

void RobotConnectionManager::InitSocketBufferStats()
{
  constexpr int lowest = 1;
  constexpr int highest = 256*1024;
  constexpr int significant_figures = 3;

  _incomingStats = std::make_unique<Histogram>(lowest, highest, significant_figures);
  _outgoingStats = std::make_unique<Histogram>(lowest, highest, significant_figures);

  // Postconditions
  DEV_ASSERT(_incomingStats, "RobotConnectionManager.InitSocketBufferStats.InvalidIncomingStats");
  DEV_ASSERT(_outgoingStats, "RobotConnectionManager.InitSocketBufferStats.InvalidOutgoingStats");
}

void RobotConnectionManager::UpdateSocketBufferStats()
{
  // Preconditions
  DEV_ASSERT(_incomingStats, "RobotConnectionManager.UpdateSocketBufferStats.InvalidIncomingStats");
  DEV_ASSERT(_outgoingStats, "RobotConnectionManager.UpdateSocketBufferStats.InvalidOutgoingStats");

  if (_udpClient.IsConnected()) {
    const auto incoming = _udpClient.GetIncomingSize();
    if (incoming >= 0) {
      _incomingStats->Record(incoming);
    }
    const auto outgoing = _udpClient.GetOutgoingSize();
    if (outgoing >= 0) {
      _outgoingStats->Record(outgoing);
    }
  }
}

void RobotConnectionManager::ReportSocketBufferStats(const std::string & name, const HistogramPtr & histogram)
{
  // Preconditions
  DEV_ASSERT(histogram, "RobotConnectionManager.ReportSocketBufferStats.InvalidHistogram");

  const int64_t min = histogram->GetMin();
  const int64_t mean = histogram->GetMean();
  const int64_t max = histogram->GetMax();

  LOG_INFO("RobotConnectionManager.ReportSocketBufferStats", "%s: %lld/%lld/%lld", name.c_str(), min, mean, max);
}

void RobotConnectionManager::ReportSocketBufferStats()
{
  // Preconditions
  DEV_ASSERT(_incomingStats, "RobotConnectionManager.ReportSocketBufferStats.InvalidIncomingStats");
  DEV_ASSERT(_outgoingStats, "RobotConnectionManager.ReportSocketBufferStats.InvalidOutgoingStats");

  ReportSocketBufferStats("incoming", _incomingStats);
  ReportSocketBufferStats("outgoing", _outgoingStats);
}

#endif // ANKI_PROFILE_ENGINE_SOCKET_BUFFER_STATS

} // end namespace Vector
} // end namespace Anki
