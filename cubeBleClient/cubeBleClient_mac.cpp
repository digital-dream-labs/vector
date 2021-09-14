/**
 * File: cubeBleClient_mac.cpp
 *
 * Author: Matt Michini
 * Created: 12/1/2017
 *
 * Description:
 *               Defines interface to simulated cubes (mac-specific implementations)
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

#include <webots/Emitter.hpp>
#include <webots/Receiver.hpp>
#include <webots/Supervisor.hpp>

#ifndef SIMULATOR
#error SIMULATOR should be defined by any target using cubeBleClient_mac.cpp
#endif

namespace Anki {
namespace Vector {

namespace { // "Private members"

  // Has SetSupervisor() been called yet?
  bool _engineSupervisorSet = false;

  // Current supervisor (if any)
  webots::Supervisor* _engineSupervisor = nullptr;
  
  // Receiver for discovery/advertisement
  webots::Receiver* _discoveryReceiver = nullptr;
  
  // Emitter used to talk to all cubes (by switching the channel as needed)
  webots::Emitter* _cubeEmitter = nullptr;
  
  // Webots comm channel used for the discovery emitter/receiver
  constexpr int kDiscoveryChannel = 0;
  
  // All of the Webots receivers found in the engine proto
  std::vector<webots::Receiver*> _receivers;
  
  // Maps factory ID to webots receiver
  std::map<BleFactoryId, webots::Receiver*> _cubeReceiverMap;
  
  float _scanDuration_sec = 3.f;
  float _scanUntil_sec = 0.f;
  
  // Last time we've heard from the connected cube. If you zap the
  // cube from the webots world to simulate an unsolicited disconnect,
  // use this value to automatically 'disconnect' it (or else it will
  // remain 'connected').
  double _connectedCubeLastHeardTime_sec = 0.f;
  
} // "private" namespace


int GetReceiverChannel(const BleFactoryId& factoryId)
{
  const int channel = (int) (std::hash<std::string>{}(factoryId) & 0x3FFFFFFF);
  return channel;
}
  
int GetEmitterChannel(const BleFactoryId& factoryId)
{
  return 1 + GetReceiverChannel(factoryId);
}


CubeBleClient::CubeBleClient()
{
  // Ensure that we have a webots supervisor
  DEV_ASSERT(_engineSupervisorSet, "CubeBleClient.NoWebotsSupervisor");
  
  if (nullptr != _engineSupervisor) {
    _discoveryReceiver = _engineSupervisor->getReceiver("discoveryReceiver");
    DEV_ASSERT(_discoveryReceiver != nullptr, "CubeBleClient.NullDiscoveryReceiver");
    _discoveryReceiver->setChannel(kDiscoveryChannel);
    _discoveryReceiver->enable(CUBE_TIME_STEP_MS);
    
    _cubeEmitter = _engineSupervisor->getEmitter("cubeCommsEmitter");
    DEV_ASSERT(_cubeEmitter != nullptr, "CubeBleClient.NullCubeEmitter");
    
    // Grab all the available Webots receivers
    const auto* selfNode = _engineSupervisor->getSelf();
    DEV_ASSERT(selfNode != nullptr, "CubeBleClient.NullRootNode");
    const auto* numReceiversField = selfNode->getField("numCubeReceivers");
    DEV_ASSERT(numReceiversField != nullptr, "CubeBleClient.NullNumReceiversField");
    const int numCubeReceivers = numReceiversField->getSFInt32();

    for (int i=0 ; i<numCubeReceivers ; i++) {
      auto* receiver = _engineSupervisor->getReceiver("cubeCommsReceiver" + std::to_string(i));
      DEV_ASSERT(receiver != nullptr, "CubeBleClient.NullReceiver");
      _receivers.push_back(receiver);
    }
    DEV_ASSERT(!_receivers.empty(), "CubeBleClient.NoReceiversFound");
  }
}


CubeBleClient::~CubeBleClient()
{

}


void CubeBleClient::SetSupervisor(webots::Supervisor *sup)
{
  _engineSupervisor = sup;
  _engineSupervisorSet = true;
}


void CubeBleClient::SetScanDuration(const float duration_sec)
{
  _scanDuration_sec = duration_sec;
}


void CubeBleClient::SetCubeFirmwareFilepath(const std::string& path)
{
  // not implemented for mac
}


void CubeBleClient::StartScanInternal()
{
  _cubeConnectionState = CubeConnectionState::ScanningForCubes;
  _scanUntil_sec = _scanDuration_sec + _engineSupervisor->getTime();
}


void CubeBleClient::StopScanInternal()
{
  _scanUntil_sec = _engineSupervisor->getTime();
}


bool CubeBleClient::SendMessageInternal(const MessageEngineToCube& msg)
{
  const int channel = GetEmitterChannel(_currentCube);
  _cubeEmitter->setChannel(channel);
  
  u8 buff[msg.Size()];
  msg.Pack(buff, msg.Size());
  int res = _cubeEmitter->send(buff, (int) msg.Size());
  
  // return value of 1 indicates that the message was successfully queued (see Webots documentation)
  return (res == 1);
}
  
  
bool CubeBleClient::RequestConnectInternal(const BleFactoryId& factoryId)
{
  _currentCube = factoryId;
  
  // Grab an available receiver for this cube:
  DEV_ASSERT(_cubeReceiverMap.find(factoryId) == _cubeReceiverMap.end(),
             "CubeBleClient.RequestConnectInternal.ReceiverAlreadyAssigned");
  for (auto* rec : _receivers) {
    // Is this receiver already in use by another cube?
    const auto it = std::find_if(_cubeReceiverMap.begin(), _cubeReceiverMap.end(),
                                 [rec](const std::pair<BleFactoryId, webots::Receiver*>& mapItem) {
                                   return rec == mapItem.second;
                                 });
    if (it == _cubeReceiverMap.end()) {
      // This receiver is free
      rec->setChannel(GetReceiverChannel(factoryId));
      rec->enable(CUBE_TIME_STEP_MS);
      _cubeReceiverMap[factoryId] = rec;
      break;
    }
  }
  
  DEV_ASSERT_MSG(_cubeReceiverMap.find(factoryId) != _cubeReceiverMap.end(),
                 "CubeBleClient.RequestConnectInternal.NoReceiverAssigned",
                 "Could not find a free receiver for cube with factory ID %s. Connected to too many cubes?",
                 factoryId.c_str());
  
  // Mark as connection pending
  _cubeConnectionState = CubeConnectionState::PendingConnect;
  return true;
}


bool CubeBleClient::RequestDisconnectInternal()
{
  // The simulated cubes do not know if they are 'connected' or not,
  // so we need to send a 'black' light animation to the cube so it
  // doesn't continue to play its current light animation.
  SendLightsOffToCube();
  
  // Disable and remove the receiver associated with this cube;
  const auto receiverIt = _cubeReceiverMap.find(_currentCube);
  if (receiverIt != _cubeReceiverMap.end()) {
    auto* receiver = receiverIt->second;
    // flush the receiver then disable it
    while (receiver->getQueueLength() > 0) {
      receiver->nextPacket();
    }
    receiver->disable();
    _cubeReceiverMap.erase(receiverIt);
  }
  
  // Mark as disconnection pending
  _cubeConnectionState = CubeConnectionState::PendingDisconnect;
  return true;
}

  
bool CubeBleClient::InitInternal()
{
  return true;
}

  
bool CubeBleClient::UpdateInternal()
{
  // Check for unwanted disconnects (cube removed from webots world)
  if (_cubeConnectionState == CubeConnectionState::Connected &&
      _engineSupervisor->getTime() > _connectedCubeLastHeardTime_sec + 3.0) {
    PRINT_NAMED_WARNING("CubeBleClient.Update.NotHearingFromCube",
                        "Disconnecting from cube since we have not heard from it recently.");
    RequestDisconnectInternal();
  }
  
  if (_cubeConnectionState == CubeConnectionState::PendingConnect) {
    _cubeConnectionState = CubeConnectionState::Connected;
    _connectedCubeLastHeardTime_sec = _engineSupervisor->getTime();
    for (const auto& callback : _cubeConnectionCallbacks) {
      callback(_currentCube, true);
    }
  } else if (_cubeConnectionState == CubeConnectionState::PendingDisconnect) {
    _cubeConnectionState = CubeConnectionState::UnconnectedIdle;
    for (const auto& callback : _cubeConnectionCallbacks) {
      callback(_currentCube, false);
    }
    _currentCube.clear();
  }
  
  // Look for discovery/advertising messages:
  while (_discoveryReceiver->getQueueLength() > 0) {
    // Shove the data into a MessageCubeToEngine and call callbacks.
    MessageCubeToEngine cubeMessage((uint8_t *) _discoveryReceiver->getData(),
                                    (size_t) _discoveryReceiver->getDataSize());
    const auto sigStrength = _discoveryReceiver->getSignalStrength();
    _discoveryReceiver->nextPacket();
    if (cubeMessage.GetTag() == MessageCubeToEngineTag::available) {
      // Received an advertisement message
      ExternalInterface::ObjectAvailable msg(cubeMessage.Get_available());
      
      // Webots signal strength is 1/r^2 with r = distance in meters.
      // Therefore typical webots signal strength values are in (0, ~150) or so.
      // Typical RSSI values for physical cubes range from -100 to -30 or so.
      // So map (0, 150) -> (-100, -30).
      const double rssiDbl = -100.0 + (sigStrength / 150.0) * 70.0;
      msg.rssi = Util::numeric_cast_clamped<decltype(msg.rssi)>(rssiDbl);
      
      // Call the appropriate callbacks with the modified message, but only if
      // we are actively scanning and are not connected to this cube
      const bool connectedToThisCube = (_currentCube == msg.factory_id) &&
                                       (_cubeConnectionState == CubeConnectionState::Connected);
      if (_cubeConnectionState == CubeConnectionState::ScanningForCubes &&
          !connectedToThisCube) {
        for (const auto& callback : _objectAvailableCallbacks) {
          callback(msg);
        }
      }
    } else {
      // Unexpected message type on the discovery channel
      PRINT_NAMED_WARNING("CubeBleClient.Update.UnexpectedMsg",
                          "Expected ObjectAvailable but received %s",
                          MessageCubeToEngineTagToString(cubeMessage.GetTag()));
    }
  }

  // Look for messages from the individual light cubes:
  for (const auto& mapEntry : _cubeReceiverMap) {
    const auto& factoryId = mapEntry.first;
    auto* receiver = mapEntry.second;
    while (receiver->getQueueLength() > 0) {
      MessageCubeToEngine cubeMessage((uint8_t *) receiver->getData(),
                                      (size_t) receiver->getDataSize());
      receiver->nextPacket();
      _connectedCubeLastHeardTime_sec = _engineSupervisor->getTime();
      // Received a light cube message. Call the registered callbacks.
      for (const auto& callback : _cubeMessageCallbacks) {
        callback(factoryId, cubeMessage);
      }
    }
  }
  
  // Check for the end of the scanning period
  if (_cubeConnectionState == CubeConnectionState::ScanningForCubes
      && _engineSupervisor->getTime() >= _scanUntil_sec) {
    _cubeConnectionState = CubeConnectionState::UnconnectedIdle;
    for (const auto& callback : _scanFinishedCallbacks) {
      callback();
    }
  }
  
  return true;
}

  
void CubeBleClient::SendLightsOffToCube()
{
  static const CubeLightKeyframe blackKeyframe({{0, 0, 0}}, 0, 0, 0);
  
  CubeLightKeyframeChunk keyframeChunk;
  keyframeChunk.startingIndex = 0;
  keyframeChunk.keyframes.fill(blackKeyframe);
  
  CubeLightSequence lightSequence(0, {{0, 0, 0, 0}});
  
  SendMessageToLightCube(MessageEngineToCube(std::move(keyframeChunk)));
  SendMessageToLightCube(MessageEngineToCube(std::move(lightSequence)));
}


} // namespace Vector
} // namespace Anki
