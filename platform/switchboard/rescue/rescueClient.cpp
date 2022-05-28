/**
* File: rescueClient.cpp
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

#include "rescue/rescueClient.h"
#include "rescue/miniFaceDisplay.h"

#include "ev++.h"

#include "clad/externalInterface/messageGameToEngine.h"
#include "clad/externalInterface/messageEngineToGame.h"

#include "cutils/properties.h"

#include "switchboardd/savedSessionManager.h"


namespace Anki {
namespace Switchboard {

bool RescueClient::Init()
{
  // Get Robot Name
  _robotName = SavedSessionManager::GetRobotName();
  
  if(_robotName.empty())
  {
    return false;
  }

  return true;
}

bool RescueClient::Connect()
{
  _isConnected = true;
  return true;
}

bool RescueClient::Disconnect()
{
  return true;
}

void RescueClient::StartPairing()
{
  // Send Enter pairing message
  Anki::Vector::SwitchboardInterface::EnterPairing enterPairing;
  using EMessage = Anki::Vector::ExternalInterface::MessageEngineToGame;
  EMessage msg;
  msg.Set_EnterPairing(std::move(enterPairing));
  _pairingStatusSignal.emit(msg);
}


void RescueClient::SendMessage(const Anki::Vector::ExternalInterface::MessageGameToEngine& message)
{
  /* noop */
}

void RescueClient::SetPairingPin(std::string pin)
{
  _pin = pin;
}

void RescueClient::SetFaultCode(int faultCode) {
  _faultCode = faultCode;
}

void RescueClient::SendBLEConnectionStatus(bool connected)
{
  // This interface function is for engineMessagingClient to 
  // inform vic-engine of BLE connection status, so it is 
  // unneeded for rescueClient.
}

void RescueClient::ShowPairingStatus(Anki::Vector::SwitchboardInterface::ConnectionStatus status)
{
  using namespace Anki::Vector::SwitchboardInterface;
  switch(status) {
    case ConnectionStatus::NONE:
      break;
    case ConnectionStatus::START_PAIRING:
      break;
    case ConnectionStatus::SHOW_PRE_PIN:
      Anki::Vector::DrawShowPinScreen(_robotName, "######");
      break;
    case ConnectionStatus::SHOW_PIN:
      Anki::Vector::DrawShowPinScreen(_robotName, _pin);
      break;
    case ConnectionStatus::SETTING_WIFI:
    case ConnectionStatus::UPDATING_OS:
    case ConnectionStatus::UPDATING_OS_ERROR:
    case ConnectionStatus::WAITING_FOR_APP:
      Anki::Vector::DrawShowPinScreen(_robotName, "RESCUE");
      break;
    case ConnectionStatus::END_PAIRING:
      Anki::Vector::DrawFaultCode(_faultCode, _faultCodeRestart);
      break;
    default:
      break;
  }
}

//
// Ignore requests that would normally come from vic-engine
//

void RescueClient::HandleWifiScanRequest() { /* noop */ }

void RescueClient::HandleWifiConnectRequest(const std::string& ssid,
                                            const std::string& pwd,
                                            bool disconnectAfterConnection) { /* noop */ }

void RescueClient::HandleHasBleKeysRequest() { /* noop */ }

} // namespace Switchboard
} // namespace Anki