/**
 * File: GatewayMessagingServer.h
 *
 * Author: shawnb
 * Created: 7/24/2018
 *
 * Description: Communication point for message coming from / 
 *              going to the gateway process. Currently this is
 *              using a udp connection where gateway acts as the
 *              client, and this is the server.
 *
 * Copyright: Anki, Inc. 2018
 *
 **/

#ifndef PLATFORM_SWITCHBOARD_SWITCHBOARDD_GatewayMessagingServer_H_
#define PLATFORM_SWITCHBOARD_SWITCHBOARDD_GatewayMessagingServer_H_

#include <string>
#include <queue>
#include <unordered_map>
#include <signals/simpleSignal.hpp>
#include "ev++.h"
#include "coretech/messaging/shared/socketConstants.h"
#include "coretech/messaging/shared/LocalUdpServer.h"
#include "switchboardd/connectionIdManager.h"
#include "switchboardd/taskExecutor.h"
#include "switchboardd/safeHandle.h"
#include "tokenClient.h"
#include "engine/clad/gateway/switchboard.h"

namespace Anki {
namespace Switchboard {

class GatewayMessagingServer {
  typedef std::function<void(bool, std::string)> ConnectionIdRequestCallback;
  typedef std::function<void(bool)> ClientGuidRefreshRequestCallback;
  typedef std::function<void(std::string, uint16_t, std::string, std::string)> SdkProxyRequestCallback;

public:
  using GatewayMessageSignal = Signal::Signal<void (SwitchboardRequest)>;
  explicit GatewayMessagingServer(struct ev_loop* loop, std::shared_ptr<TaskExecutor> taskExecutor, std::shared_ptr<TokenClient> tokenClient, std::shared_ptr<ConnectionIdManager> connectionIdManager);
  ~GatewayMessagingServer();
  bool Init();
  bool Disconnect();

  std::shared_ptr<SafeHandle> SendConnectionIdRequest(ConnectionIdRequestCallback callback);
  std::shared_ptr<SafeHandle> SendClientGuidRefreshRequest(ClientGuidRefreshRequestCallback callback);
  std::shared_ptr<SafeHandle> SendSdkProxyRequest(std::string clientGuid, std::string id, std::string path, std::string json, SdkProxyRequestCallback callback);


  void HandleAuthRequest(const SwitchboardRequest& message);
  void HandleConnectionIdRequest(const SwitchboardRequest& message);
  void HandleConnectionIdResponse(const SwitchboardRequest& message);
  void HandleClientGuidRefreshResponse(const SwitchboardRequest& message);
  void HandleSdkProxyResponse(const SwitchboardRequest& message);
  void ProcessCloudAuthResponse(bool isPrimary, Anki::Vector::TokenError authError, std::string appToken, std::string authJwtToken);
  static void sEvGatewayMessageHandler(struct ev_loop* loop, struct ev_timer* w, int revents);

  std::weak_ptr<TokenClient> _tokenClient;
  std::shared_ptr<ConnectionIdManager> _connectionIdManager;

private:
  LocalUdpServer _server;
  std::shared_ptr<TaskExecutor> _taskExecutor;
  std::queue<ConnectionIdRequestCallback> _connectionIdRequestCallbackQueue;
  std::queue<std::weak_ptr<SafeHandle>> _connectionIdRequestHandlesQueue;

  std::queue<ClientGuidRefreshRequestCallback> _refreshClientGuidRequestCallbackQueue;
  std::queue<std::weak_ptr<SafeHandle>> _refreshClientGuidRequestHandlesQueue;

  // SdkProxyRequestCallback requires matching id, so we will 
  // keep map instead of queue  
  std::unordered_map<std::string, SdkProxyRequestCallback> _sdkProxyRequestCallbackQueue;
  std::unordered_map<std::string, std::weak_ptr<SafeHandle>> _sdkProxyRequestHandlesQueue;

  bool SendMessage(const SwitchboardResponse& message);

  struct ev_loop* loop_;

  struct ev_GatewayMessageTimerStruct {
    ev_timer timer;
    LocalUdpServer* server;
    GatewayMessagingServer* messagingServer;
  } _handleGatewayMessageTimer;

  static uint8_t sMessageData[2048];
  static const unsigned int kMessageHeaderLength = 2;
  const float kGatewayMessageFrequency_s = 0.1;
};
} // Switchboard
} // Anki

#endif // PLATFORM_SWITCHBOARD_SWITCHBOARDD_GatewayMessagingServer_H_
