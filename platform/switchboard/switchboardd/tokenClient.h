/**
 * File: tokenClient.h
 *
 * Author: paluri
 * Created: 7/16/2018
 *
 * Description: Unix domain socket client connection to vic-cloud process.
 *
 * Copyright: Anki, Inc. 2018
 *
 **/

#pragma once

#include <functional>
#include <vector>
#include <queue>
#include "signals/simpleSignal.hpp"
#include "ev++.h"
#include "switchboardd/taskExecutor.h"
#include "engine/clad/cloud/token.h"
#include "coretech/messaging/shared/LocalUdpClient.h"
#include "coretech/messaging/shared/socketConstants.h"

namespace Anki {
namespace Switchboard {

class TokenResponseHandle {
public:
  TokenResponseHandle() :
  _valid(true) {}

  void Cancel() { _valid = false; }
  bool IsValid() { return _valid; }

private:
  bool _valid;
};
  
class TokenClient {
  typedef std::function<void(Anki::Vector::TokenError, std::string, std::string)> AuthRequestCallback;
  typedef std::function<void(Anki::Vector::TokenError, std::string)> JwtRequestCallback;
  using TokenMessageSignal = Signal::Signal<void (Anki::Vector::TokenResponse)>;

public:
  explicit TokenClient(struct ev_loop* loop, std::shared_ptr<TaskExecutor> taskExecutor);
  //~TokenClient();
  bool Init();
  bool Connect();
  std::shared_ptr<TokenResponseHandle> SendAuthRequest(std::string sessionToken, std::string clientName, std::string appId, AuthRequestCallback callback);
  std::shared_ptr<TokenResponseHandle> SendSecondaryAuthRequest(std::string sessionToken, std::string clientName, std::string appId, AuthRequestCallback callback);
  std::shared_ptr<TokenResponseHandle> SendReassociateAuthRequest(std::string sessionToken, std::string clientName, std::string appId, AuthRequestCallback callback);
  std::shared_ptr<TokenResponseHandle> SendJwtRequest(JwtRequestCallback callback);

private:
  const char* kDomainSocketServer = Vector::TOKEN_SERVER_PATH;
  const char* kDomainSocketClient = Vector::TOKEN_SWITCHBOARD_CLIENT_PATH;

  static uint8_t sMessageData[2048];
  const float kMessageFrequency_s = 0.1;

  void SendMessage(const Anki::Vector::TokenRequest& message);
  void HandleTokenResponse(Anki::Vector::TokenResponse response);
  static void sEvTokenMessageHandler(struct ev_loop* loop, struct ev_timer* w, int revents);

  TokenMessageSignal _tokenMessageSignal;

  struct ev_loop* _loop;
  struct ev_TokenMessageTimerStruct {
    ev_timer timer;
    LocalUdpClient* client;
    TokenMessageSignal* signal;
  } _handleTokenMessageTimer;

  Signal::SmartHandle _tokenResponseHandle;
  std::queue<AuthRequestCallback> _authCallbacks;
  std::queue<JwtRequestCallback> _jwtCallbacks;
  std::queue<std::shared_ptr<TokenResponseHandle>> _authHandles;
  std::queue<std::shared_ptr<TokenResponseHandle>> _jwtHandles;
  LocalUdpClient _client;
  std::shared_ptr<TaskExecutor> _taskExecutor; 
};

} // Anki
} // Switchboard
