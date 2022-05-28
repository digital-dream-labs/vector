/**
 * File: engineMessagingClient.cpp
 *
 * Author: shawnb
 * Created: 3/08/2018
 *
 * Description: Communication point for message coming from / 
 *              going to the engine process. Currently this is
 *              using a tcp connection where engine acts as the
 *              server, and this is the client.
 *
 * Copyright: Anki, Inc. 2018
 *
 **/

#include <stdio.h>
#include <chrono>
#include <thread>
#include <iomanip>
#include "anki-ble/common/log.h"
#include "anki-wifi/wifi.h"
#include "switchboardd/engineMessagingClient.h"
#include "clad/externalInterface/messageEngineToGame.h"
#include "clad/externalInterface/messageGameToEngine.h"
#include "switchboardd/savedSessionManager.h"
#include "switchboardd/log.h"

using GMessageTag = Anki::Vector::ExternalInterface::MessageGameToEngineTag;
using GMessage = Anki::Vector::ExternalInterface::MessageGameToEngine;
using EMessageTag = Anki::Vector::ExternalInterface::MessageEngineToGameTag;
using EMessage = Anki::Vector::ExternalInterface::MessageEngineToGame;
namespace Anki {
namespace Switchboard {

uint8_t EngineMessagingClient::sMessageData[2048];

EngineMessagingClient::EngineMessagingClient(struct ev_loop* evloop)
: loop_(evloop)
{}

bool EngineMessagingClient::Init() {
  ev_timer_init(&_handleEngineMessageTimer.timer,
                &EngineMessagingClient::sEvEngineMessageHandler,
                kEngineMessageFrequency_s, 
                kEngineMessageFrequency_s);
  _handleEngineMessageTimer.client = &_client;
  _handleEngineMessageTimer.signal = &_pairingStatusSignal;
  return true;
}

bool Anki::Switchboard::EngineMessagingClient::Connect() {
  bool connected = _client.Connect(Anki::Vector::ENGINE_SWITCH_CLIENT_PATH, Anki::Vector::ENGINE_SWITCH_SERVER_PATH);

  if(connected) {
    ev_timer_start(loop_, &_handleEngineMessageTimer.timer);
  }

  return connected;
}

bool EngineMessagingClient::Disconnect() {
  bool disconnected = true;
  ev_timer_stop(loop_, &_handleEngineMessageTimer.timer);
  if (_client.IsConnected()) {
    disconnected = _client.Disconnect();
  }
  return disconnected;
}

void EngineMessagingClient::sEvEngineMessageHandler(struct ev_loop* loop, struct ev_timer* w, int revents) {
  struct ev_EngineMessageTimerStruct *wData = (struct ev_EngineMessageTimerStruct*)w;

  int recvSize;
  
  while((recvSize = wData->client->Recv((char*)sMessageData, sizeof(sMessageData))) > kMessageHeaderLength) {
    // Get message tag, and adjust for header size
    const uint8_t* msgPayload = (const uint8_t*)&sMessageData[kMessageHeaderLength];

    const EMessageTag messageTag = (EMessageTag)*msgPayload;

    EMessage message;
    uint16_t msgSize = *(uint16_t*)sMessageData;
    size_t unpackedSize = message.Unpack(msgPayload, msgSize);

    if(unpackedSize != (size_t)msgSize) {
      Log::Error("Received message from engine but had mismatch size when unpacked.");
      continue;
    } 

    switch(messageTag) {
      case EMessageTag::EnterPairing:
      case EMessageTag::ExitPairing:
      case EMessageTag::WifiScanRequest:
      case EMessageTag::WifiConnectRequest:
      case EMessageTag::HasBleKeysRequest:
      {
        // Emit signal for message
        wData->signal->emit(message);
      }
        break;
      default:
        break;
    }
  }
}

void EngineMessagingClient::HandleWifiScanRequest() {
  std::vector<Wifi::WiFiScanResult> wifiResults;
  Wifi::WifiScanErrorCode code = Wifi::ScanForWiFiAccessPoints(wifiResults);

  const uint8_t statusCode = (uint8_t)code;

  Anki::Vector::SwitchboardInterface::WifiScanResponse rsp;
  rsp.status_code = statusCode;
  rsp.ssid_count = wifiResults.size();

  Log::Write("Sending wifi scan results.");
  SendMessage(GMessage::CreateWifiScanResponse(std::move(rsp)));
}

void EngineMessagingClient::HandleWifiConnectRequest(const std::string& ssid,
                                                     const std::string& pwd,
                                                     bool disconnectAfterConnection) {
  // Convert ssid to hex string
  std::stringstream ss;
  for(char c : ssid)
  {
    ss << std::hex << std::setfill('0') << std::setw(2) << (int)c;
  }
  const std::string ssidHex = ss.str();

  Anki::Vector::SwitchboardInterface::WifiConnectResponse rcp;
  rcp.status_code = 255;

  // Scan for access points
  std::vector<Wifi::WiFiScanResult> wifiResults;
  Wifi::WifiScanErrorCode code = Wifi::ScanForWiFiAccessPoints(wifiResults);
  
  if(code == Wifi::WifiScanErrorCode::SUCCESS)
  {
    // Scan was a success, look though results for an AP with matching ssid
    bool ssidInRange = false;
    for(const auto& result : wifiResults)
    {
      if(strcmp(ssidHex.c_str(), result.ssid.c_str()) == 0)
      {
        ssidInRange = true;
        
        Log::Write("HandleWifiConnectRequest: Found requested ssid from scan, attempting to connect");
        Wifi::ConnectWifiResult res = Wifi::ConnectWiFiBySsid(result.ssid,
                                                              pwd,
                                                              (uint8_t)result.auth,
                                                              result.hidden,
                                                              nullptr,
                                                              nullptr);

        if(res != 0)
        {
          Log::Write("HandleWifiConnectRequest: Failed to connect to ssid");
        }
        
        rcp.status_code = (uint8_t)res;
        break;
      }
    }
    
    if(!ssidInRange)
    {
      Log::Write("HandleWifiConnectRequest: Requested ssid not in range");
    }
  }
  else
  {
    Log::Write("HandleWifiConnectRequest: Wifi scan failed");
    rcp.status_code = (uint8_t)code;
  }

  if(disconnectAfterConnection)
  {
    // Immediately disconnect from ssid
    // Will do nothing if not connected to ssid
    (void)Wifi::RemoveWifiService(ssidHex);
  }
  
  SendMessage(GMessage::CreateWifiConnectResponse(std::move(rcp)));
}

void EngineMessagingClient::HandleHasBleKeysRequest() {
  Anki::Vector::SwitchboardInterface::HasBleKeysResponse rsp;

  // load keys
  RtsKeys keys = SavedSessionManager::LoadRtsKeys();
  rsp.hasBleKeys = keys.clients.size() > 0;

  SendMessage(GMessage::CreateHasBleKeysResponse(std::move(rsp)));
}

void EngineMessagingClient::SendMessage(const GMessage& message) {
  uint16_t message_size = message.Size();
  uint8_t buffer[message_size + kMessageHeaderLength];
  message.Pack(buffer + kMessageHeaderLength, message_size);
  memcpy(buffer, &message_size, kMessageHeaderLength);

  _client.Send((char*)buffer, sizeof(buffer));
}

void EngineMessagingClient::SetPairingPin(std::string pin) {
  Anki::Vector::SwitchboardInterface::SetBLEPin sbp;
  sbp.pin = std::stoul(pin);
  SendMessage(GMessage::CreateSetBLEPin(std::move(sbp)));
}

void EngineMessagingClient::SendBLEConnectionStatus(bool connected) {
  Anki::Vector::SwitchboardInterface::SendBLEConnectionStatus msg;
  msg.connected = connected;
  SendMessage(GMessage::CreateSendBLEConnectionStatus(std::move(msg)));
}

void EngineMessagingClient::ShowPairingStatus(Anki::Vector::SwitchboardInterface::ConnectionStatus status) {
  Anki::Vector::SwitchboardInterface::SetConnectionStatus scs;
  scs.status = status;

  SendMessage(GMessage::CreateSetConnectionStatus(std::move(scs)));
}

} // Switchboard
} // Anki
