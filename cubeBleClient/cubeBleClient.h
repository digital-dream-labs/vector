/**
 * File: cubeBleClient.h
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

#ifndef __Victor_CubeBleClient_H__
#define __Victor_CubeBleClient_H__

#include "clad/types/cubeCommsTypes.h"

#include "coretech/common/shared/types.h"

#include <vector>
#include <functional>

// Forward declaration
namespace webots {
  class Supervisor;
}

namespace Anki {
namespace Vector {

namespace ExternalInterface {
  struct ObjectAvailable;
}
class MessageEngineToCube;
class MessageCubeToEngine;

// Alias for BLE factory ID (TODO: should be defined in CLAD)
using BleFactoryId = std::string;
  
class CubeBleClient
{
public:
  CubeBleClient();
  ~CubeBleClient();
  
  bool Init();
  
  bool Update();

#ifdef SIMULATOR
  // Assign Webots supervisor
  // Webots processes must do this before creating CubeBleClient for the first time.
  // Unit test processes must call SetSupervisor(nullptr) to run without a supervisor.
  static void SetSupervisor(webots::Supervisor *sup);
#endif

  using ObjectAvailableCallback   = std::function<void(const ExternalInterface::ObjectAvailable&)>;
  using CubeMessageCallback       = std::function<void(const BleFactoryId&, const MessageCubeToEngine&)>;
  using CubeConnectionCallback    = std::function<void(const BleFactoryId&, const bool connected)>;
  using ScanFinishedCallback      = std::function<void(void)>;
  using ConnectionFailedCallback  = std::function<void(const BleFactoryId&)>;
  
  void RegisterObjectAvailableCallback(const ObjectAvailableCallback& callback) {
    _objectAvailableCallbacks.push_back(callback);
  }
  void RegisterCubeMessageCallback(const CubeMessageCallback& callback) {
    _cubeMessageCallbacks.push_back(callback);
  }
  void RegisterCubeConnectionCallback(const CubeConnectionCallback& callback) {
    _cubeConnectionCallbacks.push_back(callback);
  }
  void RegisterScanFinishedCallback(const ScanFinishedCallback& callback) {
    _scanFinishedCallbacks.push_back(callback);
  }
  void RegisterConnectionFailedCallback(const ConnectionFailedCallback& callback) {
    _connectionFailedCallbacks.push_back(callback);
  }

  // Begin scanning for available cubes. Should not
  // be connected to a cube when starting a scan.
  void StartScanning();
  
  // Stop scanning for available cubes
  void StopScanning();
  
  // Send a message to the connected light cube. Returns true on success.
  bool SendMessageToLightCube(const MessageEngineToCube&);
  
  // Request to connect to an advertising cube. Returns true on success.
  bool RequestConnectToCube(const BleFactoryId&);
  
  // Request to disconnect from the connected cube. Returns true on success.
  bool RequestDisconnectFromCube();
  
  // Get the current cube connection state
  CubeConnectionState GetCubeConnectionState() const { return _cubeConnectionState; }
  
  BleFactoryId GetCurrentCube() const { return _currentCube; }
  
private:
  
#ifdef SIMULATOR
  // sim-only: Turn off cube lights on the connected cube
  void SendLightsOffToCube();
#endif
  
  // callbacks for advertisement messages:
  std::vector<ObjectAvailableCallback> _objectAvailableCallbacks;
  
  // callbacks for raw light cube messages:
  std::vector<CubeMessageCallback> _cubeMessageCallbacks;
  
  // callbacks for when a cube is connected/disconnected:
  std::vector<CubeConnectionCallback> _cubeConnectionCallbacks;
  
  // callbacks for when scanning for cubes has completed:
  std::vector<ScanFinishedCallback> _scanFinishedCallbacks;
  
  // callbacks for when a connection attempt times out or fails
  std::vector<ConnectionFailedCallback> _connectionFailedCallbacks;
  
  bool _inited = false;
  
  // Current state of cube connection
  CubeConnectionState _cubeConnectionState = CubeConnectionState::UnconnectedIdle;
  
  // This is the factory ID of the cube we are either currently
  // connected to, or pending connection or disconnection to.
  // It is an empty string if there is no current cube.
  BleFactoryId _currentCube;

////////////////////////////////////////////////////////////////////////////
// ---------- Implementation-specific (mac vs. vicos) methods. ---------- //
// These are defined in their respective *_mac.cpp and *_vicos.cpp files  //
////////////////////////////////////////////////////////////////////////////
  
public:
  
  void SetScanDuration(const float duration_sec);
  
  void SetCubeFirmwareFilepath(const std::string& path);
  
private:
  
  bool InitInternal();
  
  bool UpdateInternal();
  
  void StartScanInternal();
  
  void StopScanInternal();
  
  bool SendMessageInternal(const MessageEngineToCube&);
  
  bool RequestConnectInternal(const BleFactoryId&);
  
  bool RequestDisconnectInternal();
  
}; // class CubeBleClient
  
  
} // namespace Vector
} // namespace Anki

#endif // __Victor_CubeBleClient_H__
