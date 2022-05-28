/**
 * File: tokenClient.cpp
 *
 * Author: paluri
 * Created: 7/16/2018
 *
 * Description: Unix domain socket client connection to vic-cloud process.
 *
 * Copyright: Anki, Inc. 2018
 *
 **/

#include "log.h"
#include "tokenClient.h"

namespace Anki {
namespace Switchboard {

uint8_t TokenClient::sMessageData[2048];

TokenClient::TokenClient(struct ev_loop* evloop, std::shared_ptr<TaskExecutor> taskExecutor) : 
_loop(evloop),
_taskExecutor(taskExecutor)
{}

//TokenClient::~TokenClient() {}

bool TokenClient::Init() {
  ev_timer_init(&_handleTokenMessageTimer.timer, 
                &TokenClient::sEvTokenMessageHandler, 
                kMessageFrequency_s, 
                kMessageFrequency_s);
  _handleTokenMessageTimer.client = &_client;
  _handleTokenMessageTimer.signal = &_tokenMessageSignal;

  _tokenResponseHandle = _tokenMessageSignal.ScopedSubscribe(
    std::bind(&TokenClient::HandleTokenResponse, this, std::placeholders::_1));

  return true;
}

bool TokenClient::Connect() {
  bool connected = _client.Connect(kDomainSocketClient, kDomainSocketServer);

  if(connected) {
    ev_timer_start(_loop, &_handleTokenMessageTimer.timer);
  }

  return connected;
}

std::shared_ptr<TokenResponseHandle> TokenClient::SendAuthRequest(std::string sessionToken, std::string clientName, std::string appId, AuthRequestCallback callback) {
  std::shared_ptr<TokenResponseHandle> handle = std::make_shared<TokenResponseHandle>();

  _taskExecutor->Wake([this, handle, callback, sessionToken, clientName, appId]() {
    // add callback to queue
    _authCallbacks.push(callback);
    _authHandles.push(handle);

    Anki::Vector::TokenRequest tokenRequest =
      Anki::Vector::TokenRequest(Anki::Vector::AuthRequest(sessionToken, clientName, appId));

    SendMessage(tokenRequest);
  });

  return handle;
}

std::shared_ptr<TokenResponseHandle> TokenClient::SendSecondaryAuthRequest(std::string sessionToken, std::string clientName, std::string appId, AuthRequestCallback callback) {
  std::shared_ptr<TokenResponseHandle> handle = std::make_shared<TokenResponseHandle>();

  _taskExecutor->Wake([this, handle, callback, sessionToken, clientName, appId]() {
    // add callback to queue
    _authCallbacks.push(callback);
    _authHandles.push(handle);

    Anki::Vector::TokenRequest tokenRequest = 
      Anki::Vector::TokenRequest(Anki::Vector::SecondaryAuthRequest(sessionToken, clientName, appId));

    SendMessage(tokenRequest);
  });

  return handle;
}

std::shared_ptr<TokenResponseHandle> TokenClient::SendReassociateAuthRequest(std::string sessionToken, std::string clientName, std::string appId, AuthRequestCallback callback) {
  std::shared_ptr<TokenResponseHandle> handle = std::make_shared<TokenResponseHandle>();

  _taskExecutor->Wake([this, handle, callback, sessionToken, clientName, appId]() {
    // add callback to queue
    _authCallbacks.push(callback);
    _authHandles.push(handle);

    Anki::Vector::TokenRequest tokenRequest = 
      Anki::Vector::TokenRequest(Anki::Vector::ReassociateRequest(sessionToken, clientName, appId));

    SendMessage(tokenRequest);
  });

  return handle;
}

std::shared_ptr<TokenResponseHandle> TokenClient::SendJwtRequest(JwtRequestCallback callback) {
  std::shared_ptr<TokenResponseHandle> handle = std::make_shared<TokenResponseHandle>();

  _taskExecutor->Wake([this, handle, callback]() {
    // add callback to queue
    _jwtCallbacks.push(callback);
    _jwtHandles.push(handle);

    Anki::Vector::TokenRequest tokenRequest = 
      Anki::Vector::TokenRequest(Anki::Vector::JwtRequest());

    SendMessage(tokenRequest);
  });

  return handle;
}

void TokenClient::SendMessage(const Anki::Vector::TokenRequest& message) {
  uint16_t message_size = message.Size();
  uint8_t buffer[message_size];
  message.Pack(buffer, message_size);

  _client.Send((char*)buffer, sizeof(buffer));
}

void TokenClient::HandleTokenResponse(Anki::Vector::TokenResponse response) {
  _taskExecutor->Wake([this, response]() {
    switch(response.GetTag()) {
      case Anki::Vector::TokenResponseTag::auth: {
        Anki::Vector::AuthResponse msg = response.Get_auth();
        AuthRequestCallback cb = _authCallbacks.front();
        _authCallbacks.pop();
        std::shared_ptr<TokenResponseHandle> handle = _authHandles.front();
        _authHandles.pop();
        if(handle->IsValid()) {
          cb(msg.error, msg.appToken, msg.jwtToken);
        }
        handle = nullptr;
      }
      break;
      case Anki::Vector::TokenResponseTag::jwt: {
        Anki::Vector::JwtResponse msg = response.Get_jwt();
        JwtRequestCallback cb = _jwtCallbacks.front();
        _jwtCallbacks.pop();
        std::shared_ptr<TokenResponseHandle> handle = _jwtHandles.front();
        _jwtHandles.pop();
        if(handle->IsValid()) {
          cb(msg.error, msg.jwtToken);
        }
        handle = nullptr;
      }
      break;
      default:
        Log::Error("Received unknown message type from TokenServer");
        break;
    }
  });
}

void TokenClient::sEvTokenMessageHandler(struct ev_loop* loop, struct ev_timer* w, int revents) {
  struct ev_TokenMessageTimerStruct *wData = (struct ev_TokenMessageTimerStruct*)w;

  int recvSize;
  
  while((recvSize = wData->client->Recv((char*)sMessageData, sizeof(sMessageData))) > 0) {
    Log::Write("Received message from token_server: %d", recvSize);

    // Get message tag, and adjust for header size
    const uint8_t* msgPayload = (const uint8_t*)&sMessageData;

    Anki::Vector::TokenResponse message;
    size_t unpackedSize = message.Unpack(msgPayload, recvSize);

    if(unpackedSize != (size_t)recvSize) {
      Log::Error("Received message from token server but had mismatch size when unpacked.");
      continue;
    }

    // emit signal
    wData->signal->emit(message);
  }
}

} // Anki
} // Switchboard
