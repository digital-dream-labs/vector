/**
* File: robotLogUploader.cpp
*
* Description: Robot Log Uploader
*
* Copyright: Anki, inc. 2018
*
*/

#include "robotLogUploader.h"
#include "robotLogDumper.h"

#include "clad/cloud/logcollector.h"
#include "coretech/messaging/shared/socketConstants.h"
#include "util/fileUtils/fileUtils.h"
#include "util/logging/logging.h"
#include "util/string/stringUtils.h"

#include <sys/select.h>
#include <unistd.h>

#define LOG_CHANNEL "RobotLogUploader"

namespace Anki {
namespace Vector {

Result RobotLogUploader::Connect()
{
  // Construct client path with PID so it will be unique on this host
  _clientPath = LOGCOLLECTOR_CLIENT_PATH + std::to_string(getpid());
  _serverPath = LOGCOLLECTOR_SERVER_PATH;

  const bool ok = _udpClient.Connect(_clientPath, _serverPath);
  if (!ok) {
    LOG_ERROR("RobotLogUploader.Connect", "Unable to connect from %s to %s", _clientPath.c_str(), _serverPath.c_str());
    Anki::Util::FileUtils::DeleteFile(_clientPath);
    return RESULT_FAIL;
  }

  // Set mode so non-privileged process (vic-cloud) can reply to this socket
  const auto mode = (S_IRWXU|S_IRWXG|S_IRWXO);
  chmod(_clientPath.c_str(), mode);

  return RESULT_OK;
}

Result RobotLogUploader::Send(const LogCollectorRequest & request)
{
  const size_t size = request.Size();
  uint8_t buffer[size];
  request.Pack(buffer, size);

  const ssize_t sent = _udpClient.Send((const char *) buffer, size);
  if (sent <= 0) {
    LOG_ERROR("RobotLogUploader.Send", "Failed to send log collector request (%zd/%zu)", sent, size);
    return RESULT_FAIL;
  }
  return RESULT_OK;
}

Result RobotLogUploader::Receive(LogCollectorResponse & response)
{
  uint8_t buffer[1024];
  const ssize_t received = _udpClient.Recv((char *) buffer, sizeof(buffer));
  if (received <= 0) {
    LOG_ERROR("RobotLogUploader.Receive", "Failed to receive log collector response (%zd/%zu)", received, sizeof(buffer));
    return RESULT_FAIL;
  }
  response.Unpack(buffer, (size_t) received);
  return RESULT_OK;
}

Result RobotLogUploader::Disconnect()
{
  const bool ok = _udpClient.Disconnect();
  if (!_clientPath.empty()) {
    Anki::Util::FileUtils::DeleteFile(_clientPath);
  }
  return (ok ? RESULT_OK : RESULT_FAIL);
}

Result RobotLogUploader::Upload(const std::string & path, std::string & url)
{
  Result result = Connect();
  if (result != RESULT_OK) {
    LOG_ERROR("RobotLogUploader.Upload", "Unable to connect to log collector service");
    return result;
  }

  LogCollector::UploadRequest request;
  request.logFileName = path;

  result  = Send(LogCollector::LogCollectorRequest(std::move(request)));
  if (result != RESULT_OK) {
    LOG_ERROR("RobotLogUploader.Upload", "Unable to send upload request (error %d)", result);
    Disconnect();
    return result;
  }

  // Block until response arrives
  const int fd = _udpClient.GetSocket();
  const int nfds = fd + 1;
  bool ready = false;

  while (!ready) {
    fd_set fdset;
    FD_ZERO(&fdset);
    FD_SET(fd, &fdset);
    int n = select(nfds, &fdset, nullptr, nullptr, nullptr);
    if (n <= 0 && errno == EINTR) {
      LOG_WARNING("RobotLogUploader.Upload", "select interrupted (errno %d)", errno);
      continue;
    }
    if (n <= 0) {
      LOG_WARNING("RobotLogUploader.Upload", "select error (errno %d)", errno);
      break;
    }
    if (!FD_ISSET(fd, &fdset)) {
      LOG_WARNING("RobotLogUploader.Upload", "socket not ready?");
      break;
    }
    ready = true;
  }

  LogCollectorResponse response;
  result = Receive(response);

  Disconnect();

  if (result != RESULT_OK) {
    LOG_ERROR("RobotLogUploader.Upload", "Unable to receive log collector response (error %d)", result);
    return result;
  }

  using LogCollectorResponseTag = LogCollector::LogCollectorResponseTag;
  const auto tag = response.GetTag();
  switch (tag)
  {
    case LogCollectorResponseTag::upload:
    {
      const auto & uploadResponse = response.Get_upload();
      url = uploadResponse.logUrl;
      LOG_INFO("RobotLogUploader.Upload", "Upload URL %s", url.c_str());
      result = RESULT_OK;
      break;
    }
    case LogCollectorResponseTag::err:
    {
      const auto & errorResponse = response.Get_err();
      const auto err = errorResponse.err;
      LOG_ERROR("RobotLogUploader.Upload", "Log collector upload error %s (%d)", EnumToString(err), (int) err);
      result = RESULT_FAIL;
      break;
    }
    default:
    {
      LOG_ERROR("RobotLogUploader.Upload", "Invalid response tag %s (%d)",
                LogCollector::LogCollectorResponseTagToString(tag), (int) tag);
      result = RESULT_FAIL;
      break;
    }
  }

  return result;
}

Result RobotLogUploader::UploadDebugLogs(std::string & status)
{
  using namespace Anki::Util;
  const std::string & logpath = "/tmp/" + GetUUIDString() + ".gz";
  std::string url;

  RobotLogDumper logDumper;
  Result result = logDumper.Dump(logpath);
  if (result != RESULT_OK) {
    FileUtils::DeleteFile(logpath);
    status = "Unable to dump logs";
    return result;
  }

  RobotLogUploader logUploader;
  result = logUploader.Upload(logpath, url);
  if (result != RESULT_OK) {
    FileUtils::DeleteFile(logpath);
    status = "Unable to upload logs";
    return result;
  }

  FileUtils::DeleteFile(logpath);
  status = url;
  return RESULT_OK;
}

} // end namespace Vector
} // end namespace Anki
