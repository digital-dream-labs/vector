/**
 * File: cozmoAnim/animComms.cpp
 *
 * Author: Kevin Yoon
 * Created: 7/30/2017
 *
 * Description: Create sockets and manages low-level data transfer to engine and robot processes
 *
 * Copyright: Anki, Inc. 2017
 **/

#include "cozmoAnim/animComms.h"
#include "osState/osState.h"

#include "anki/cozmo/shared/cozmoConfig.h"

#include "coretech/messaging/shared/LocalUdpClient.h"
#include "coretech/messaging/shared/LocalUdpServer.h"
#include "coretech/messaging/shared/SocketUtils.h"
#include "coretech/messaging/shared/socketConstants.h"
#include "util/histogram/histogram.h"
#include "util/logging/logging.h"

#include <thread>
#include <stdio.h>

// Log options
#define LOG_CHANNEL "AnimComms"

// Trace options
// #define LOG_TRACE(name, format, ...)   LOG_DEBUG(name, format, ##__VA_ARGS__)
#define LOG_TRACE(name, format, ...)   {}

#if ANKI_PROFILE_ANIMCOMMS_SOCKET_BUFFER_STATS
namespace
{
  using Histogram = Anki::Util::Histogram;
  using HistogramPtr = std::unique_ptr<Histogram>;

  typedef struct SocketBufferStats {
    HistogramPtr _incoming;
    HistogramPtr _outgoing;
  } SocketBufferStats;

  SocketBufferStats _robotStats;
  SocketBufferStats _engineStats;

}
#endif

namespace Anki {
namespace Vector {
namespace AnimComms {

namespace { // "Private members"

  LocalUdpServer _engineComms;

  // For comms with robot
  LocalUdpClient _robotComms;
}


#if ANKI_PROFILE_ANIMCOMMS_SOCKET_BUFFER_STATS

void InitSocketBufferStats(SocketBufferStats & stats)
{
  constexpr int64_t lowest = 1;
  constexpr int64_t highest = 256*1024;
  constexpr int significant_figures = 3;
  stats._incoming = std::make_unique<Histogram>(lowest, highest, significant_figures);
  stats._outgoing = std::make_unique<Histogram>(lowest, highest, significant_figures);
}

void InitSocketBufferStats()
{
  InitSocketBufferStats(_robotStats);
  InitSocketBufferStats(_engineStats);
}

void UpdateSocketBufferStats(SocketBufferStats & stats, int socket)
{
  if (socket >= 0) {
    const auto incoming = Anki::Messaging::GetIncomingSize(socket);
    DEV_ASSERT(incoming >= 0, "AnimComms.UpdateSocketBufferStats.InvalidIncoming");
    if (incoming >= 0) {
      stats._incoming->Record(incoming);
    }
    const auto outgoing = Anki::Messaging::GetOutgoingSize(socket);
    DEV_ASSERT(outgoing >= 0, "AnimComms.UpdateSocketBufferStats.InvalidOutgoing");
    if (outgoing >= 0) {
      stats._outgoing->Record(outgoing);
    }
  }
}

void UpdateSocketBufferStats()
{
  UpdateSocketBufferStats(_robotStats, _robotComms.GetSocket());
  UpdateSocketBufferStats(_engineStats, _engineComms.GetSocket());
}


void ReportSocketBufferStats(const std::string & name, const HistogramPtr & histogram)
{
  const int64_t min = histogram->GetMin();
  const int64_t mean = histogram->GetMean();
  const int64_t max = histogram->GetMax();

  LOG_INFO("AnimComms.ReportSocketBufferStats", "%s = %lld/%lld/%lld", name.c_str(), min, mean, max);
}

void ReportSocketBufferStats(const std::string & name, const SocketBufferStats & stats)
{
  ReportSocketBufferStats(name + ".incoming", stats._incoming);
  ReportSocketBufferStats(name + ".outgoing", stats._outgoing);
}

void ReportSocketBufferStats()
{
  ReportSocketBufferStats("robot", _robotStats);
  ReportSocketBufferStats("engine", _engineStats);
}

#endif

Result InitRobotComms()
{
  const RobotID_t robotID = OSState::getInstance()->GetRobotID();
  const std::string & client_path = std::string(ANIM_ROBOT_CLIENT_PATH) + std::to_string(robotID);
  const std::string & server_path = std::string(ANIM_ROBOT_SERVER_PATH) + std::to_string(robotID);

  LOG_INFO("AnimComms.InitRobotComms", "Connect from %s to %s", client_path.c_str(), server_path.c_str());

  bool ok = _robotComms.Connect(client_path.c_str(), server_path.c_str());
  if (!ok) {
    LOG_ERROR("AnimComms.InitRobotComms", "Unable to connect from %s to %s",
              client_path.c_str(), server_path.c_str());
    return RESULT_FAIL_IO;
  }

  return RESULT_OK;
}

Result InitEngineComms()
{
  const RobotID_t robotID = OSState::getInstance()->GetRobotID();
  const std::string & server_path = std::string(ENGINE_ANIM_SERVER_PATH) + std::to_string(robotID);

  LOG_INFO("AnimComms.InitEngineComms", "Start listening at %s", server_path.c_str());

  if (!_engineComms.StartListening(server_path)) {
    LOG_ERROR("AnimComms.InitEngineComms", "Unable to listen at %s", server_path.c_str());
    return RESULT_FAIL_IO;
  }

  return RESULT_OK;
}

Result InitComms()
{
  Result result = InitRobotComms();
  if (RESULT_OK != result) {
    LOG_ERROR("AnimComms.InitComms", "Unable to init robot comms (result %d)", result);
    return result;
  }

  result = InitEngineComms();
  if (RESULT_OK != result) {
    LOG_ERROR("AnimComms.InitComms", "Unable to init engine comms (result %d)", result);
    return result;
  }

  return RESULT_OK;
}

bool IsConnectedToRobot(void)
{
  return _robotComms.IsConnected();
}

bool IsConnectedToEngine(void)
{
  return _engineComms.HasClient();
}

void DisconnectRobot()
{
  LOG_DEBUG("AnimComms.DisconnectRobot", "Disconnect robot");
  _robotComms.Disconnect();
}

void DisconnectEngine(void)
{
  LOG_DEBUG("AnimComms.DisconnectEngine", "Disconnect engine");
  _engineComms.Disconnect();
}

bool SendPacketToEngine(const void *buffer, const u32 length)
{
  if (!_engineComms.HasClient()) {
    LOG_TRACE("AnimComms.SendPacketToEngine", "No engine client");
    return false;
  }

  const ssize_t bytesSent = _engineComms.Send((char*)buffer, length);
  if (bytesSent < (ssize_t) length) {
    LOG_ERROR("AnimComms.SendPacketToEngine.FailedSend",
              "Failed to send msg contents (%zd of %d bytes sent)",
              bytesSent, length);
    DisconnectEngine();
    return false;
  }

  return true;
}


u32 GetNextPacketFromEngine(u8* buffer, u32 max_length)
{
  // Read available datagram
  const ssize_t dataLen = _engineComms.Recv((char*)buffer, max_length);
  if (dataLen < 0) {
    // Something went wrong
    LOG_ERROR("GetNextPacketFromEngine.FailedRecv", "Failed to receive from engine");
    DisconnectEngine();
    return 0;
  }
  return (u32) dataLen;
}


bool SendPacketToRobot(const void *buffer, const u32 length)
{
  if (!_robotComms.IsConnected()) {
    LOG_TRACE("SendPacketToRobot", "Robot is not connected");
    return false;
  }

  const ssize_t bytesSent = _robotComms.Send((const char*)buffer, length);
  if (bytesSent < (ssize_t) length) {
    LOG_ERROR("SendPacketToRobot.FailedSend", "Failed to send msg contents (%zd bytes sent)", bytesSent);
    DisconnectRobot();
    return false;
  }

  return true;
}

u32 GetNextPacketFromRobot(u8* buffer, u32 max_length)
{
  // Read available datagram
  const ssize_t dataLen = _robotComms.Recv((char*)buffer, max_length);
  if (dataLen < 0) {
    // Something went wrong
    LOG_ERROR("GetNextPacketFromRobot.FailedRecv", "Failed to receive from robot");
    DisconnectRobot();
    return 0;
  }

  return (u32) dataLen;
}

} // namespace AnimComms
} // namespace Vector
} // namespace Anki
