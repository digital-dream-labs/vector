/**
* File: rescueClient.h
*
* Author: chapados
* Date:   02/22/2019
*
* Description: ISwitchboardCommandClient implementation for vic-rescue program.
* This implmementation relies on externally provided state and does not connect
* to vic-engine. It provides enough logic to connect components to the logic the
* subscribers of ht e Status/EngineMessage signals.
*
* Copyright: Anki, Inc. 2019
**/

#pragma once

#include "switchboardd/ISwitchboardCommandClient.h"

namespace Anki {
namespace Switchboard {

class RescueClient : public ISwitchboardCommandClient {
public:
  bool Init();
  bool Connect();
  bool Disconnect();
  void SendMessage(const Anki::Vector::ExternalInterface::MessageGameToEngine& message);
  void SetPairingPin(std::string pin);
  void SetFaultCode(int faultCode);
  void SendBLEConnectionStatus(bool connected);
  void ShowPairingStatus(Anki::Vector::SwitchboardInterface::ConnectionStatus status);
  void HandleWifiScanRequest();
  void HandleWifiConnectRequest(const std::string& ssid,
                                const std::string& pwd,
                                bool disconnectAfterConnection);
  void HandleHasBleKeysRequest();
  EngineMessageSignal& OnReceivePairingStatus() {
    return _pairingStatusSignal;
  }
  EngineMessageSignal& OnReceiveEngineMessage() {
    return _engineMessageSignal;
  }

  void StartPairing();
  bool IsConnected() const { return _isConnected; }
private:
  EngineMessageSignal _pairingStatusSignal;
  EngineMessageSignal _engineMessageSignal;

  bool _isConnected;

  std::string _robotName;
  std::string _pin;
  uint16_t _faultCode;
  bool _faultCodeRestart;
};

} // namespace Switchboard
} // namespace Anki
