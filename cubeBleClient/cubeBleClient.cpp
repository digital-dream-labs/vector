/**
 * File: cubeBleClient.cpp
 *
 * Author: Matt Michini
 * Created: 12/1/2017
 *
 * Description:
 *               Defines interface to BLE-connected real or simulated cubes
 *
 * Copyright: Anki, Inc. 2017
 *
 **/

#include "cubeBleClient.h"

#include "anki/cozmo/shared/cozmoConfig.h"

#include "clad/externalInterface/messageCubeToEngine.h"
#include "clad/externalInterface/messageEngineToCube.h"

#include "util/logging/logging.h"
#include "util/helpers/templateHelpers.h"


namespace Anki {
namespace Vector {


bool CubeBleClient::Init()
{
  DEV_ASSERT(!_inited, "CubeBleClient.Init.AlreadyInitialized");
  if (InitInternal()) {
    _inited = true;
  } else {
    PRINT_NAMED_ERROR("CubeBleClient.Init.FailedInit",
                      "Failed to initialize CubeBleClient");
  }
  
  return _inited;
}


bool CubeBleClient::Update()
{
  if (!_inited) {
    DEV_ASSERT(false, "CubeBleClient.Update.NotInited");
    return false;
  }
  
  // sanity checks:
  if (_currentCube.empty()) {
    // No current cube - we can only either be scanning or unconnected
    DEV_ASSERT((_cubeConnectionState == CubeConnectionState::UnconnectedIdle) ||
               (_cubeConnectionState == CubeConnectionState::ScanningForCubes),
               "CubeBleClient.Update.InvalidNotConnectedState");
  } else {
    // We have a current cube - should be only one of the following states
    DEV_ASSERT((_cubeConnectionState == CubeConnectionState::Connected) ||
               (_cubeConnectionState == CubeConnectionState::PendingConnect) ||
               (_cubeConnectionState == CubeConnectionState::PendingDisconnect),
               "CubeBleClient.Update.InvalidConnectionState");
  }
  
  const bool result = UpdateInternal();
  return result;
}


void CubeBleClient::StartScanning()
{
  if (ANKI_VERIFY(_cubeConnectionState == CubeConnectionState::UnconnectedIdle,
                  "CubeBleClient.StartScanning.NotUnconnected",
                  "Should not be connected or have pending connections/disconnections "
                  "when initiating a scan for cubes. Current connection state %s. "
                  "Current cube %s.",
                  CubeConnectionStateToString(_cubeConnectionState),
                  _currentCube.c_str())) {
    StartScanInternal();
  }
}


void CubeBleClient::StopScanning()
{
  StopScanInternal();
}


bool CubeBleClient::SendMessageToLightCube(const MessageEngineToCube& msg)
{
  bool result = false;
  if (ANKI_VERIFY(_cubeConnectionState == CubeConnectionState::Connected && !_currentCube.empty(),
                  "CubeBleClient.SendMessageToLightCube.CubeNotConnected",
                  "Current connection state %s, current cube '%s'",
                  CubeConnectionStateToString(_cubeConnectionState),
                  _currentCube.c_str())) {
    result = SendMessageInternal(msg);
  }
  return result;
}
  
  
bool CubeBleClient::RequestConnectToCube(const BleFactoryId& factoryId)
{
  bool result = false;
  if (ANKI_VERIFY(_cubeConnectionState == CubeConnectionState::UnconnectedIdle && _currentCube.empty(),
                  "CubeBleClient.RequestConnectToCube.NotUnconnected",
                  "Current connection state %s, current cube '%s'",
                  CubeConnectionStateToString(_cubeConnectionState),
                  _currentCube.c_str())) {
    result = RequestConnectInternal(factoryId);
  }
  return result;
}


bool CubeBleClient::RequestDisconnectFromCube()
{
  const bool connectedOrPending = ((_cubeConnectionState == CubeConnectionState::Connected) ||
                                   (_cubeConnectionState == CubeConnectionState::PendingConnect)) &&
                                  !_currentCube.empty();
  bool result = false;
  if (ANKI_VERIFY(connectedOrPending,
                  "CubeBleClient.RequestDisconnectFromCube.NotConnectedOrPendingConnect",
                  "Current connection state %s, current cube '%s'",
                  CubeConnectionStateToString(_cubeConnectionState),
                  _currentCube.c_str())) {
    result = RequestDisconnectInternal();
  }
  return result;
}


} // namespace Vector
} // namespace Anki
