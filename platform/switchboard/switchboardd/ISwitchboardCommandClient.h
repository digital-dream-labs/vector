/**
* File: ISwitchboardCommandClient.h
*
* Author: chapados
* Date:   02/22/2019
*
* Description: Interface between the switchboard components and a controlling entity used to 
* receive command requests and pairing status updates, as well as display information on the
* robot's face.
*
* Copyright: Anki, Inc. 2019
**/

#pragma once

#include <signals/simpleSignal.hpp>
#include <string>

namespace Anki {

// Forward declarations
namespace Vector {
namespace ExternalInterface {
class MessageEngineToGame;
class MessageGameToEngine;
}

namespace SwitchboardInterface {
enum class ConnectionStatus : uint8_t;
}

} // namespace Vector

namespace Switchboard {

class ISwitchboardCommandClient {
  public:
  
  virtual bool Init() = 0;
  virtual bool Connect() = 0;
  virtual bool Disconnect() = 0;
  virtual void SendMessage(const Anki::Vector::ExternalInterface::MessageGameToEngine& message) = 0;
  virtual void SetPairingPin(std::string pin) = 0 ;
  virtual void SendBLEConnectionStatus(bool connected) = 0;
  virtual void ShowPairingStatus(Anki::Vector::SwitchboardInterface::ConnectionStatus status) = 0;
  virtual void HandleWifiScanRequest() = 0;
  virtual void HandleWifiConnectRequest(const std::string& ssid,
                                        const std::string& pwd,
                                        bool disconnectAfterConnection) = 0;
  virtual void HandleHasBleKeysRequest() = 0;
  
  // All subclasses must implement signal accessors
  using EngineMessageSignal = Signal::Signal<void (Anki::Vector::ExternalInterface::MessageEngineToGame)>;
  virtual EngineMessageSignal& OnReceivePairingStatus() = 0;
  virtual EngineMessageSignal& OnReceiveEngineMessage() = 0;
};

} // namespace Switchboard
} // namespace Anki