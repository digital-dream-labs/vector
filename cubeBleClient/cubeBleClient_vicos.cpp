/**
 * File: cubeBleClient_vicos.cpp
 *
 * Author: Matt Michini
 * Created: 12/1/2017
 *
 * Description:
 *               Defines interface to BLE central process which communicates with cubes (vic-os specific implementations)
 *
 * Copyright: Anki, Inc. 2017
 *
 **/

#include "cubeBleClient.h"

#include "anki-ble/common/anki_ble_uuids.h"
#include "bleClient/bleClient.h"

#include "clad/externalInterface/messageCubeToEngine.h"
#include "clad/externalInterface/messageEngineToCube.h"

#include "util/logging/DAS.h"
#include "util/logging/logging.h"
#include "util/math/numericCast.h"
#include "util/string/stringUtils.h"
#include "util/fileUtils/fileUtils.h"
#include "util/time/universalTime.h"

#include <queue>
#include <thread>

#ifdef SIMULATOR
#error SIMULATOR should NOT be defined by any target using cubeBleClient_vicos.cpp
#endif

namespace Anki {
namespace Vector {

namespace {
  
  struct ev_loop* _loop = nullptr;
  
  std::unique_ptr<BleClient> _bleClient = nullptr;
  
  // For detecting connection state changes
  bool _wasConnectedToCube = false;
  
  // shared queue for buffering cube messages received on the client thread
  using CubeMsgRecvBuffer = std::queue<std::vector<uint8_t>>;
  CubeMsgRecvBuffer _cubeMsgRecvBuffer;
  std::mutex _cubeMsgRecvBuffer_mutex;
  
  // shared queue for buffering advertisement messages received on the client thread
  struct CubeAdvertisementInfo {
    CubeAdvertisementInfo(const std::string& addr, const int rssi) : addr(addr), rssi(rssi) { }
    std::string addr;
    int rssi;
  };
  using CubeAdvertisementBuffer = std::queue<CubeAdvertisementInfo>;
  CubeAdvertisementBuffer _cubeAdvertisementBuffer;
  std::mutex _cubeAdvertisementBuffer_mutex;

  // Flag indicating when scanning for cubes has completed
  std::atomic<bool> _scanningFinished{false};
  
  // Time after which we consider a connection attempt to have failed.
  // Always less than 0 if there is no pending connection attempt.
  float _connectionAttemptFailTime_sec = -1.f;
  
  // Time after which we consider the firmware check/update step to have failed.
  // Always less than 0 if there is no firmware check/update in progress.
  float _firmwareUpdateFailTime_sec = -1.f;
  
  // Max time a connection attempt is allowed to take before timing out
  const float kConnectionAttemptTimeout_sec = 20.f;
  
  // Max time the firmware check/update step is allowed to take before timing out
  const float kFirmwareUpdateTimeout_sec = 15.f;
}

  
CubeBleClient::CubeBleClient()
{
  _loop = ev_default_loop(EVBACKEND_SELECT);
  _bleClient = std::make_unique<BleClient>(_loop);
  
  _bleClient->RegisterAdvertisementCallback([](const std::string& addr, const int rssi) {
    std::lock_guard<std::mutex> lock(_cubeAdvertisementBuffer_mutex);
    _cubeAdvertisementBuffer.emplace(addr, rssi);
  });
  
  _bleClient->RegisterReceiveDataCallback([](const std::string& addr, const std::vector<uint8_t>& data){
    std::lock_guard<std::mutex> lock(_cubeMsgRecvBuffer_mutex);
    _cubeMsgRecvBuffer.push(data);
  });
  
  _bleClient->RegisterScanFinishedCallback([](){
    _scanningFinished = true;
  });

}


CubeBleClient::~CubeBleClient()
{
  _bleClient.reset();
  ev_loop_destroy(_loop);
  _loop = nullptr;
}

bool CubeBleClient::InitInternal()
{
  DEV_ASSERT(!_inited, "CubeBleClient.Init.AlreadyInitialized");

  _bleClient->Start();
  return true;
}

bool CubeBleClient::UpdateInternal()
{
  // Check bleClient's connection to the bluetooth daemon
  if (!_bleClient->IsConnectedToServer() &&
      _cubeConnectionState != CubeConnectionState::UnconnectedIdle) {
    const auto prevConnnectionState = _cubeConnectionState;
    _cubeConnectionState = CubeConnectionState::UnconnectedIdle;
    if (prevConnnectionState == CubeConnectionState::Connected) {
      // inform callbacks that we've been disconnected
      for (const auto& callback : _cubeConnectionCallbacks) {
        callback(_currentCube, false);
      }
    }
    _currentCube.clear();
    PRINT_NAMED_WARNING("CubeBleClient.UpdateInternal.NotConnectedToDaemon",
                        "We are not connected to the bluetooth daemon - setting connection state to %s. "
                        "Previous connection state: %s.",
                        CubeConnectionStateToString(_cubeConnectionState),
                        CubeConnectionStateToString(prevConnnectionState));
  }
  
  
  // Check for connection attempt timeout or firmware check/update timeout
  if (_cubeConnectionState == CubeConnectionState::PendingConnect) {
    const float now_sec = static_cast<float>(Util::Time::UniversalTime::GetCurrentTimeInSeconds());
    // Check if we have just started firmware checking/updating
    if (_firmwareUpdateFailTime_sec < 0.f &&
        _bleClient->IsPendingFirmwareCheckOrUpdate()) {
      PRINT_NAMED_INFO("CubeBleClient.UpdateInternal.FirmwareCheckStart",
                       "Firmware check/update started for cube %s",
                       _currentCube.c_str());
      _firmwareUpdateFailTime_sec = now_sec + kFirmwareUpdateTimeout_sec;
      _connectionAttemptFailTime_sec = -1.f;
    }
    
    const bool connectionAttemptTimedOut = (_connectionAttemptFailTime_sec > 0.f) && (now_sec > _connectionAttemptFailTime_sec);
    const bool firmwareUpdateTimedOut = (_firmwareUpdateFailTime_sec > 0.f) && (now_sec > _firmwareUpdateFailTime_sec);
    
    if (connectionAttemptTimedOut || firmwareUpdateTimedOut) {
      PRINT_NAMED_WARNING("CubeBleClient.UpdateInternal.ConnectionTimeout",
                          "%s has taken more than %.2f seconds - aborting.",
                          connectionAttemptTimedOut ? "Connection attempt" : "Firmware check or update",
                          connectionAttemptTimedOut ? kConnectionAttemptTimeout_sec : kFirmwareUpdateTimeout_sec);
      DASMSG(cube_connection_failed, "cube.connection_failed", "Connection attempt timed out");
      DASMSG_SET(s1, _currentCube, "Cube factory ID");
      DASMSG_SEND();
      // Inform callbacks that the connection attempt has failed
      for (const auto& callback : _connectionFailedCallbacks) {
        callback(_currentCube);
      }
      // Tell BleClient to disconnect from cube. This will cancel the
      // connection attempt.
      RequestDisconnectInternal();
    }
  } else {
    _connectionAttemptFailTime_sec = -1.f;
    _firmwareUpdateFailTime_sec = -1.f;
  }
  
  
  // Check for connection state changes
  auto onConnect = [this](){
    PRINT_NAMED_INFO("CubeBleClient.UpdateInternal.ConnectedToCube",
                     "Connected to cube %s",
                     _currentCube.c_str());
    DASMSG(cube_connected, "cube.connected", "We have connected to a cube");
    DASMSG_SET(s1, _currentCube, "Cube factory ID");
    DASMSG_SEND();
    _cubeConnectionState = CubeConnectionState::Connected;
    for (const auto& callback : _cubeConnectionCallbacks) {
      callback(_currentCube, true);
    }
  };
  
  auto onDisconnect = [this](){
    PRINT_NAMED_INFO("CubeBleClient.UpdateInternal.DisconnectedFromCube",
                     "Disconnected from cube %s",
                     _currentCube.c_str());
    DASMSG(cube_disconnected, "cube.disconnected", "We have disconnected from a cube");
    DASMSG_SET(s1, _currentCube, "Cube factory ID");
    DASMSG_SEND();
    _cubeConnectionState = CubeConnectionState::UnconnectedIdle;
    for (const auto& callback : _cubeConnectionCallbacks) {
      callback(_currentCube, false);
    }
    _currentCube.clear();
  };
  
  // Check for connection state changes (both expected and unexpected)
  const bool connectedToCube = _bleClient->IsConnectedToCube();
  if (connectedToCube && _cubeConnectionState == CubeConnectionState::PendingConnect) {
    // Successfully connected
    onConnect();
  } else if (!connectedToCube && _cubeConnectionState == CubeConnectionState::PendingDisconnect) {
    // Successfully disconnected
    onDisconnect();
  } else if (connectedToCube != _wasConnectedToCube) {
    // Unexpected cube connection/disconnection!
    PRINT_NAMED_WARNING("CubeBleClient.UpdateInternal.UnexpectedConnectOrDisconnect",
                        "Received unexpected %s. Previous connection state: %s",
                        connectedToCube ? "connection" : "disconnection",
                        CubeConnectionStateToString(_cubeConnectionState));
    DASMSG(cube_unexpected_connect_disconnect, "cube.unexpected_connect_disconnect", "Unexpectedly connected or disconnected from a cube");
    DASMSG_SET(i1, connectedToCube, "1 if we have connected to a cube, 0 or null if we have disconnected");
    DASMSG_SET(s1, _currentCube, "Cube factory ID");
    DASMSG_SET(s2, CubeConnectionStateToString(_cubeConnectionState), "Previous connection state");
    DASMSG_SEND();
    if (connectedToCube) {
      onConnect();
    } else {
      onDisconnect();
    }
  }
  _wasConnectedToCube = connectedToCube;
  
  // Pull advertisement messages from queue into a temp queue,
  // to avoid holding onto the mutex for too long.
  CubeAdvertisementBuffer swapCubeAdvertisementBuffer;
  {
    std::lock_guard<std::mutex> lock(_cubeAdvertisementBuffer_mutex);
    swapCubeAdvertisementBuffer.swap(_cubeAdvertisementBuffer);
  }
  
  while (!swapCubeAdvertisementBuffer.empty()) {
    const auto& data = swapCubeAdvertisementBuffer.front();
    ExternalInterface::ObjectAvailable msg;
    msg.factory_id = data.addr;
    msg.objectType = ObjectType::Block_LIGHTCUBE1; // TODO - update this with the Victor cube type once it's defined
    msg.rssi = Util::numeric_cast_clamped<decltype(msg.rssi)>(data.rssi);
    if (_cubeConnectionState == CubeConnectionState::ScanningForCubes) {
      for (const auto& callback : _objectAvailableCallbacks) {
        callback(msg);
      }
    } else {
      PRINT_NAMED_WARNING("CubeBleClient.UpdateInternal.IgnoringAdvertisement",
                          "Ignoring cube advertisement message from %s since we are not scanning for cubes. "
                          "Current connection state: %s",
                          msg.factory_id.c_str(),
                          CubeConnectionStateToString(_cubeConnectionState));
    }
    swapCubeAdvertisementBuffer.pop();
  }
  
  // Pull cube messages from queue into a temp queue,
  // to avoid holding onto the mutex for too long.
  CubeMsgRecvBuffer swapCubeMsgRecvBuffer;
  {
    std::lock_guard<std::mutex> lock(_cubeMsgRecvBuffer_mutex);
    swapCubeMsgRecvBuffer.swap(_cubeMsgRecvBuffer);
  }
  
  while (!swapCubeMsgRecvBuffer.empty()) {
    const auto& data = swapCubeMsgRecvBuffer.front();
    if (_cubeConnectionState == CubeConnectionState::Connected) {
      MessageCubeToEngine cubeMessage(data.data(), data.size());
      for (const auto& callback : _cubeMessageCallbacks) {
        callback(_currentCube, cubeMessage);
      }
    } else {
      PRINT_NAMED_WARNING("CubeBleClient.UpdateInternal.IgnoringCubeMsg",
                          "Ignoring cube messages since we are not connected to a cube. "
                          "Current connection state: %s",
                          CubeConnectionStateToString(_cubeConnectionState));
    }
    swapCubeMsgRecvBuffer.pop();
  }

  // Check to see if scanning for cubes has finished
  if (_scanningFinished) {
    _cubeConnectionState = CubeConnectionState::UnconnectedIdle;
    for (const auto& callback : _scanFinishedCallbacks) {
      callback();
    }
    _scanningFinished = false;
  }
  
  return true;
}


void CubeBleClient::SetScanDuration(const float duration_sec)
{
  _bleClient->SetScanDuration(duration_sec);
}


void CubeBleClient::SetCubeFirmwareFilepath(const std::string& path)
{
  _bleClient->SetCubeFirmwareFilepath(path);
}


void CubeBleClient::StartScanInternal()
{
  PRINT_NAMED_INFO("CubeBleClient.StartScanInternal",
                   "Starting to scan for available cubes");
  
  // Sending from this thread for now. May need to queue this and
  // send it on the client thread if ipc client is not thread safe.
  _bleClient->StartScanForCubes();
  _cubeConnectionState = CubeConnectionState::ScanningForCubes;
}


void CubeBleClient::StopScanInternal()
{
  PRINT_NAMED_INFO("CubeBleClient.StopScanInternal",
                   "Stopping scan for available cubes");
  
  // Sending from this thread for now. May need to queue this and
  // send it on the client thread if ipc client is not thread safe.
  _bleClient->StopScanForCubes();
  _cubeConnectionState = CubeConnectionState::UnconnectedIdle;
}


bool CubeBleClient::SendMessageInternal(const MessageEngineToCube& msg)
{
  u8 buff[msg.Size()];
  msg.Pack(buff, msg.Size());
  const auto& msgVec = std::vector<u8>(buff, buff + msg.Size());

  // Sending from this thread for now. May need to queue this and
  // send it on the client thread if ipc client is not thread safe.
  return _bleClient->Send(msgVec);
}


bool CubeBleClient::RequestConnectInternal(const BleFactoryId& factoryId)
{
  if (_bleClient->IsConnectedToCube()) {
    PRINT_NAMED_WARNING("CubeBleClient.RequestConnectInternal.AlreadyConnected",
                        "We are already connected to a cube (address %s)!",
                        _currentCube.c_str());
    return false;
  }
  
  DEV_ASSERT(_currentCube.empty(), "CubeBleClient.RequestConnectInternal.CubeAddressNotEmpty");
  
  _currentCube = factoryId;
  _cubeConnectionState = CubeConnectionState::PendingConnect;
  
  PRINT_NAMED_INFO("CubeBleClient.RequestConnectInternal.AttemptingToConnect",
                   "Attempting to connect to cube %s",
                   _currentCube.c_str());
  
  DEV_ASSERT(_connectionAttemptFailTime_sec < 0.f, "CubeBleClient.RequestConnectInternal.UnexpectedConnectionAttemptFailTime");
  const float now_sec = static_cast<float>(Util::Time::UniversalTime::GetCurrentTimeInSeconds());
  _connectionAttemptFailTime_sec = now_sec + kConnectionAttemptTimeout_sec;
  
  // Sending from this thread for now. May need to queue this and
  // send it on the client thread if ipc client is not thread safe.
  _bleClient->ConnectToCube(_currentCube);
  return true;
}


bool CubeBleClient::RequestDisconnectInternal()
{
  if (!_bleClient->IsConnectedToCube()) {
    PRINT_NAMED_WARNING("CubeBleClient.RequestDisconnectInternal.NotConnected",
                        "We are not connected to any cubes! Telling BleClient to disconnect anyway to be safe. "
                        "Current connection state: %s. Setting connection state to Unconnected.",
                        CubeConnectionStateToString(_cubeConnectionState));
    _cubeConnectionState = CubeConnectionState::UnconnectedIdle;
    _currentCube.clear();
    _bleClient->DisconnectFromCube();
    return false;
  }
  
  _cubeConnectionState = CubeConnectionState::PendingDisconnect;
  
  // Sending from this thread for now. May need to queue this and
  // send it on the client thread if ipc client is not thread safe.
  _bleClient->DisconnectFromCube();
  return true;
}

} // namespace Vector
} // namespace Anki
