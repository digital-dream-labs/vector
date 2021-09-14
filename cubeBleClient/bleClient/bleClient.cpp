/**
 * File: bleClient
 *
 * Author: Matt Michini (adapted from paluri)
 * Created: 3/28/2018
 *
 * Description: ble Client for ankibluetoothd. This is essentially a wrapper around
 *              IPCClient. Contains a thread which runs the ev loop for communicating
 *              with the server - keep this in mind when writing callbacks. Callbacks
 *              will occur on the ev loop thread.
 *
 * Copyright: Anki, Inc. 2018
 *
 **/
  
#include "bleClient.h"
#include "anki-ble/common/anki_ble_uuids.h"
#include "anki-ble/common/gatt_constants.h"
#include "util/logging/DAS.h"
#include "util/logging/logging.h"
#include "util/fileUtils/fileUtils.h"
#include "util/string/stringUtils.h"

#include <cmath>

namespace Anki {
namespace Vector {

namespace {
  // If we're not connected, keep trying to connect at this rate
  const ev_tstamp kConnectionCheckTime_sec = 0.5f;
  
  // If we're already connected, check for disconnection at this rate
  const ev_tstamp kDisconnectionCheckTime_sec = 5.f;

  // bytes per packet when performing OTA flash of the cubes
  const size_t kMaxBytesPerPacket = 20;
  
  // Length of the cube firmware version string
  const size_t kFirmwareVersionStrLen = 16;
}


BleClient::BleClient(struct ev_loop* loop)
  : IPCClient(loop)
  , _loop(loop)
  , _asyncBreakSignal(loop)
  , _asyncStartScanSignal(loop)
  , _connectionCheckTimer(loop)
  , _scanningTimer(loop)
{
  // Set up watcher callbacks
  _asyncBreakSignal.set<BleClient, &BleClient::AsyncBreakCallback>(this);
  _asyncStartScanSignal.set<BleClient, &BleClient::AsyncStartScanCallback>(this);
  _connectionCheckTimer.set<BleClient, &BleClient::ServerConnectionCheckTimerCallback>(this);
  _scanningTimer.set<BleClient, &BleClient::ScanningTimerCallback>(this);
}

  
BleClient::~BleClient()
{
  Stop();
}
  

void BleClient::Start()
{
  // Read the on-disk firmware file to ensure it exists.
  // This will also retrieve its version string.
  {
    std::vector<uint8_t> unused;
    if (!GetCubeFirmwareFromDisk(unused)) {
      PRINT_NAMED_ERROR("BleClient.Start.FailedGettingFirmwareFromDisk",
                        "Unable to read cube firmware from disk - aborting.");
      return;
    }
  }
  
  // Create a thread which will run the ev_loop for server comms
  auto threadFunc = [this](){
    if (!Connect()) {
      PRINT_NAMED_WARNING("BleClient.LoopThread.ConnectFailed",
                          "Unable to connect to ble server - will retry");
    }
    
    // Start a connection check/retry timer to naively just always try to
    // reconnect if we become disconnected
    _connectionCheckTimer.start(kConnectionCheckTime_sec);
    
    // Start async watchers
    _asyncBreakSignal.start();
    _asyncStartScanSignal.start();
    
    // Start the loop (runs 'forever')
    ev_loop(_loop, 0);
  };
  
  // Kick off the thread
  _loopThread = std::thread(threadFunc);
  
}

void BleClient::Stop()
{
  DisconnectFromCube();
  
  // Signal the ev loop thread to break out of its loop and wait for the thread to complete
  if (_loopThread.joinable()) {
    _asyncBreakSignal.send();
    _loopThread.join();
  }
}
  
bool BleClient::Send(const std::vector<uint8_t>& msg)
{
  if (!IsConnectedToServer()) {
    PRINT_NAMED_WARNING("BleClient.Send.NotConnectedToServer",
                        "Cannot send - not connected to the server");
    return false;
  }
  
  if (_connectionId < 0) {
    PRINT_NAMED_WARNING("BleClient.Send.NotConnectedToCube",
                        "Cannot send - not connected to any cube");
    return false;
  }

  const bool reliable = true;
  const std::string& uuid = kCubeAppWrite_128_BIT_UUID;
  SendMessage(_connectionId,
              uuid,
              reliable,
              msg);
  return true;
}


void BleClient::ConnectToCube(const std::string& address)
{
  if (!IsConnectedToServer()) {
    PRINT_NAMED_WARNING("BleClient.ConnectToCube.NotConnectedToServer",
                        "Cannot connect to cube - not connected to the server");
    return;
  }
  
  _cubeAddress = address;
  ConnectToPeripheral(_cubeAddress);
}


void BleClient::DisconnectFromCube()
{
  if (!IsConnectedToServer()) {
    PRINT_NAMED_WARNING("BleClient.DisconnectFromCube.NotConnectedToServer",
                        "Cannot disconnect from cube - not connected to the server");
    return;
  }
  
  if (_connectionId >= 0) {
    Disconnect(_connectionId);
  } else if (!_cubeAddress.empty()) {
    // We don't have a connection ID with the server, but
    // we still have a cube address. Ask the server to
    // disconnect this cube by address.
    DisconnectByAddress(_cubeAddress);
    _cubeAddress.clear();
  }
}


void BleClient::StartScanForCubes()
{
  if (!IsConnectedToServer()) {
    PRINT_NAMED_WARNING("BleClient.StartScanForCubes.NotConnectedToServer",
                        "Cannot start a scan - not connected to the server");
    return;
  }
  
  _asyncStartScanSignal.send();
}


void BleClient::StopScanForCubes()
{
  _scanningTimer.stop();
  if (!IsConnectedToServer()) {
    PRINT_NAMED_WARNING("BleClient.StopScanForCubes.NotConnectedToServer",
                        "Cannot stop scanning - not connected to the server");
    return;
  }
  
  StopScan();
  if (_scanFinishedCallback) {
    _scanFinishedCallback();
  }
}


void BleClient::FlashCube()
{
  const bool canFlashCube = IsConnectedToServer() &&
                            (_connectionId >= 0) &&
                            _pendingFirmwareCheckOrUpdate;
  if (!canFlashCube) {
    PRINT_NAMED_WARNING("BleClient.FlashCube.CannotFlashCube",
                        "Cannot flash the cube - invalid BleClient state. ConnectedToServer %d, ConnectedToCube %d, PendingFirmwareCheckOrUpdate %d",
                        IsConnectedToServer(), _connectionId >= 0, (int) _pendingFirmwareCheckOrUpdate);
    return;
  }
  
  // grab firmware from file
  std::vector<uint8_t> firmware;
  if (!GetCubeFirmwareFromDisk(firmware)) {
    PRINT_NAMED_ERROR("BleClient.FlashCube.FailedGettingFirmware",
                      "Failed retrieving cube firmware from disk");
    return;
  }
  
  size_t offset = kFirmwareVersionStrLen; // skip the first few bytes, which is the firmware version
  size_t nBytesRemaining = firmware.size() - offset;
  
  while (nBytesRemaining > 0) {
    size_t chunkLength = std::min(kMaxBytesPerPacket, nBytesRemaining);
    std::vector<uint8_t> packet(firmware.begin() + offset,
                                firmware.begin() + offset + chunkLength);
    SendMessage(_connectionId, Anki::kCubeOTATarget_128_BIT_UUID, true, packet);
    offset += chunkLength;
    nBytesRemaining -= chunkLength;
  }
}


void BleClient::OnScanResults(int error, const std::vector<BluetoothDaemon::ScanResultRecord>& records)
{
  if (error != 0) {
    PRINT_NAMED_WARNING("BleClient.OnScanResults.Error",
                        "OnScanResults reporting error %d",
                        error);
    return;
  }
  
  if (_advertisementCallback) {
    for (const auto& r : records) {
      _advertisementCallback(r.address, r.rssi);
    }
  }
}

void BleClient::OnOutboundConnectionChange(const std::string& address,
                                           const int connected,
                                           const int connection_id,
                                           const std::vector<Anki::BluetoothDaemon::GattDbRecord>& records)
{
  PRINT_NAMED_INFO("BleClient.OnOutboundConnectionChange",
                   "addr %s, connected %d, connection_id %d, ",
                   address.c_str(), connected, connection_id);
  
  if (address != _cubeAddress) {
    PRINT_NAMED_WARNING("BleClient.OnOutboundConnectionChange.IgnoringUnexpected",
                        "Ignoring unexpected %s from address %s (connection_id %d). Expected address: %s",
                        connected ? "connection" : "disconnection",
                        address.c_str(),
                        connection_id,
                        _cubeAddress.c_str());
    return;
  }
  
  if (connected) {
    _connectionId = connection_id;
    // Immediately read the cube firmware version
    _pendingFirmwareCheckOrUpdate = true;
    ReadCharacteristic(_connectionId, Anki::kCubeAppVersion_128_BIT_UUID);
  } else if (_connectionId < 0) {
    // We were trying to connect, but received a disconnect notice instead.  Let's try again.
    PRINT_NAMED_INFO("BleClient.UnexpectedDisconnectWhileTryingToConnect",
                     "addr %s", address.c_str());
    DisconnectByAddress(address);
    ConnectToPeripheral(address);
  } else if (_connectionId == connection_id) {
    _connectionId = -1;
    _cubeAddress.clear();
    _pendingFirmwareCheckOrUpdate = false;
  }
}


void BleClient::OnCharacteristicReadResult(const int connection_id,
                                           const int error,
                                           const std::string& characteristic_uuid,
                                           const std::vector<uint8_t>& data) 
{
  if ((connection_id < 0) ||
      (connection_id != _connectionId)) {
    return;
  }

  if (error) {
    PRINT_NAMED_WARNING("BleClient.OnCharacteristicReadResult.Error",
                        "error %d", error);
    return;
  }

  const bool isAppVersion = Util::StringCaseInsensitiveEquals(characteristic_uuid,
                                                              Anki::kCubeAppVersion_128_BIT_UUID);

  if (isAppVersion && _pendingFirmwareCheckOrUpdate) {
    const auto& cubeFirmwareVersion = std::string(data.begin(), data.end());

    RequestConnectionParameterUpdate(_cubeAddress,
        kGattConnectionIntervalHighPriorityMinimum,
        kGattConnectionIntervalHighPriorityMaximum,
        kGattConnectionLatencyDefault,
        kGattConnectionTimeoutDefault);
    
    // Check the cube's firmware version against the on-disk version
    DEV_ASSERT(!_cubeFirmwareVersionOnDisk.empty(), "BleClient.OnCharacteristicReadResult.NoOnDiskFirmwareVersion");
    if (cubeFirmwareVersion == _cubeFirmwareVersionOnDisk) {
      // Firmware versions match! Yay.
      _pendingFirmwareCheckOrUpdate = false;
    } else {
      // Flash the cube with the firmware we have on disk
      PRINT_NAMED_INFO("BleClient.OnCharacteristicReadResult.FirmwareVersionMismatch",
                       "Flashing cube since its firmware version (%s) does not match that on disk (%s)",
                       cubeFirmwareVersion.c_str(), _cubeFirmwareVersionOnDisk.c_str());
      DASMSG(cube_firmware_mismatch, "cube.firmware_mismatch", "Flashing cube since its firmware version does not match that on disk");
      DASMSG_SET(s1, _cubeAddress, "Cube factory ID");
      DASMSG_SET(s2, cubeFirmwareVersion, "Cube firmware version");
      DASMSG_SET(s3, _cubeFirmwareVersionOnDisk, "On disk firmware version");
      DASMSG_SEND();
      FlashCube();
    }
  }
}
  
void BleClient::OnReceiveMessage(const int connection_id,
                                 const std::string& characteristic_uuid,
                                 const std::vector<uint8_t>& value)
{
  if (connection_id != _connectionId) {
    return;
  }
  
  if (Util::StringCaseInsensitiveEquals(characteristic_uuid, Anki::kCubeAppVersion_128_BIT_UUID)) {
    const auto& cubeFirmwareVersion = std::string(value.begin(), value.end());
    if (cubeFirmwareVersion == _cubeFirmwareVersionOnDisk) {
      PRINT_NAMED_INFO("BleClient.OnReceiveMessage.FlashingSuccess","%s",cubeFirmwareVersion.c_str());
      DASMSG(cube_firmware_flash_success, "cube.firmware_flash_success", "Flashing cube firmware succeeded");
      DASMSG_SET(s1, _cubeAddress, "Cube factory ID");
      DASMSG_SET(s2, cubeFirmwareVersion, "Cube firmware version");
      DASMSG_SEND();
    } else {
      PRINT_NAMED_WARNING("BleClient.OnReceiveMessage.FlashingFailure","got = %s exp = %s",cubeFirmwareVersion.c_str(), _cubeFirmwareVersionOnDisk.c_str());
      DASMSG(cube_firmware_flash_fail, "cube.firmware_flash_fail", "Flashing cube firmware failed");
      DASMSG_SET(s1, _cubeAddress, "Cube factory ID");
      DASMSG_SET(s2, cubeFirmwareVersion, "Cube firmware version");
      DASMSG_SET(s3, _cubeFirmwareVersionOnDisk, "On disk firmware version");
      DASMSG_SEND();
      
      // Disconnect from the cube, since there is no use in keeping the connection with incorrect cube firmware
      DisconnectFromCube();
    }
    
    // consider the firmware check complete now.
    _pendingFirmwareCheckOrUpdate = false;
  } else if (Util::StringCaseInsensitiveEquals(characteristic_uuid, Anki::kCubeAppRead_128_BIT_UUID)) {
    if (_receiveDataCallback) {
      _receiveDataCallback(_cubeAddress, value);
    }
  }
}


void BleClient::AsyncBreakCallback(ev::async& w, int revents)
{
  // Break out of the loop
  ev_unloop(_loop, EVUNLOOP_ALL);
}


void BleClient::AsyncStartScanCallback(ev::async& w, int revents)
{
  // Commence scanning and start the timer
  StartScan(Anki::kCubeService_128_BIT_UUID);
  _scanningTimer.start(_scanDuration_sec);
}


void BleClient::ServerConnectionCheckTimerCallback(ev::timer& timer, int revents)
{
  static bool wasConnected = false;
  const bool isConnected = IsConnectedToServer();
  
  if (!isConnected) {
    if (wasConnected) {
      PRINT_NAMED_WARNING("BleClient.ServerConnectionCheckTimerCallback.DisconnectedFromServer",
                          "We've become disconnected from the BLE server - attempting to reconnect");
      // Server will kill our cube connection once we've become disconnected,
      // so reset connectionId and cube address
      _connectionId = -1;
      _cubeAddress.clear();
      _pendingFirmwareCheckOrUpdate = false;
      // Stop scanning for cubes timer
      _scanningTimer.stop();
    }
    // Immediately attempt to re-connect
    if (!Connect()) {
      PRINT_NAMED_WARNING("BleClient.ServerConnectionCheckTimerCallback.ConnectFailed",
                          "Unable to connect to ble server - will retry");
    }
  } else if (!wasConnected && isConnected) {
    PRINT_NAMED_INFO("BleClient.ServerConnectionCheckTimerCallback.ConnectedToServer",
                     "Connected to the BLE server!");
  }
  
  // Fire up the timer again for the appropriate interval
  timer.start(isConnected ?
              kDisconnectionCheckTime_sec :
              kConnectionCheckTime_sec);
  
  wasConnected = isConnected;
}


void BleClient::ScanningTimerCallback(ev::timer& w, int revents)
{
  StopScanForCubes();
}


bool BleClient::GetCubeFirmwareFromDisk(std::vector<uint8_t>& firmware)
{
  if (!Util::FileUtils::FileExists(_cubeFirmwarePath)) {
    PRINT_NAMED_ERROR("BleClient.GetCubeFirmwareFromDisk.MissingCubeFirmwareFile",
                      "Cube firmware file does not exist (should be at %s)",
                      _cubeFirmwarePath.c_str());
    return false;
  }
  
  // Read from file
  firmware = Util::FileUtils::ReadFileAsBinary(_cubeFirmwarePath);
  
  if (firmware.size() < kFirmwareVersionStrLen) {
    PRINT_NAMED_ERROR("BleClient.GetCubeFirmwareFromDisk.CubeFirmwareFileTooSmall",
                      "Cube firmware file is %zu bytes long! Should be at least %zu.",
                      firmware.size(), kFirmwareVersionStrLen);
    firmware.clear();
    return false;
  }
  
  _cubeFirmwareVersionOnDisk = std::string(firmware.begin(),
                                           firmware.begin() + kFirmwareVersionStrLen);
  
  PRINT_NAMED_INFO("BleClient.GetCubeFirmwareFromDisk.ReadCubeFirmwareFileVersion",
                   "Read cube firmware file from disk. Version: %s",
                   _cubeFirmwareVersionOnDisk.c_str());
  
  return true;
}
  

} // Cozmo
} // Anki
