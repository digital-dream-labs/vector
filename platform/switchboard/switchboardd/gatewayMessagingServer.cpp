/**
 * File: GatewayMessagingServer.cpp
 *
 * Author: shawnb
 * Created: 7/24/2018
 *
 * Description: Communication point for message coming from / 
 *              going to the gateway process. Currently this is
 *              using a tcp connection where gateway acts as the
 *              client, and this is the server.
 *
 * Copyright: Anki, Inc. 2018
 *
 **/

#include <stdio.h>
#include <chrono>
#include <thread>
#include "switchboardd/gatewayMessagingServer.h"
#include "clad/externalInterface/messageExternalComms.h"
#include "switchboardd/log.h"

namespace Anki {
namespace Switchboard {

using namespace Anki::Vector::ExternalComms;

uint8_t GatewayMessagingServer::sMessageData[2048];

GatewayMessagingServer::GatewayMessagingServer(struct ev_loop* evloop, std::shared_ptr<TaskExecutor> taskExecutor, std::shared_ptr<TokenClient> tokenClient, std::shared_ptr<ConnectionIdManager> connectionIdManager)
: _tokenClient(tokenClient)
, _connectionIdManager(connectionIdManager)
, _taskExecutor(taskExecutor)
, loop_(evloop)
{}

GatewayMessagingServer::~GatewayMessagingServer() {
  (void)Disconnect();
}

bool GatewayMessagingServer::Init() {
  if(_server.HasClient()) {
    _server.Disconnect();
  }

  _server.StopListening();
  _server.StartListening(Anki::Vector::SWITCH_GATEWAY_SERVER_PATH);

  ev_timer_init(&_handleGatewayMessageTimer.timer,
                &GatewayMessagingServer::sEvGatewayMessageHandler,
                kGatewayMessageFrequency_s, 
                kGatewayMessageFrequency_s);
  _handleGatewayMessageTimer.server = &_server;
  _handleGatewayMessageTimer.messagingServer = this;

  ev_timer_start(loop_, &_handleGatewayMessageTimer.timer);

  return true;
}

bool GatewayMessagingServer::Disconnect() {
  if(_server.HasClient()) {
    _server.Disconnect();
  }
  ev_timer_stop(loop_, &_handleGatewayMessageTimer.timer);
  return true;
}

std::shared_ptr<SafeHandle> GatewayMessagingServer::SendConnectionIdRequest(ConnectionIdRequestCallback callback) {
  std::shared_ptr<SafeHandle> sharedHandle = std::make_shared<SafeHandle>();

  _connectionIdRequestCallbackQueue.push(callback);
  _connectionIdRequestHandlesQueue.push(std::weak_ptr<SafeHandle>(sharedHandle));

  SendMessage(SwitchboardResponse(ExternalConnectionRequest()));

  return sharedHandle;
}

std::shared_ptr<SafeHandle> GatewayMessagingServer::SendClientGuidRefreshRequest(ClientGuidRefreshRequestCallback callback) {
  std::shared_ptr<SafeHandle> sharedHandle = std::make_shared<SafeHandle>();

  _refreshClientGuidRequestCallbackQueue.push(callback);
  _refreshClientGuidRequestHandlesQueue.push(std::weak_ptr<SafeHandle>(sharedHandle));

  SendMessage(SwitchboardResponse(ClientGuidRefreshRequest()));

  return sharedHandle;
}

std::shared_ptr<SafeHandle> GatewayMessagingServer::SendSdkProxyRequest(std::string clientGuid, std::string id, std::string path, std::string json, SdkProxyRequestCallback callback) {
  std::shared_ptr<SafeHandle> sharedHandle = std::make_shared<SafeHandle>();

  _sdkProxyRequestCallbackQueue.insert({ id, callback });
  _sdkProxyRequestHandlesQueue.insert({ id, std::weak_ptr<SafeHandle>(sharedHandle) });

  SendMessage(SwitchboardResponse(SdkProxyRequest(clientGuid, id, path, json)));

  return sharedHandle;
}

void GatewayMessagingServer::HandleAuthRequest(const SwitchboardRequest& message) {
  if(_tokenClient.expired()) {
    return;
  }

  std::shared_ptr<TokenClient> tokenClient = _tokenClient.lock();
  std::string sessionToken = message.Get_AuthRequest().sessionToken;
  std::string clientName = message.Get_AuthRequest().clientName;
  std::string appId = message.Get_AuthRequest().appId;

  tokenClient->SendJwtRequest(
    [this, sessionToken, clientName, appId, tokenClient](Anki::Vector::TokenError error, std::string jwtToken) {
      bool isPrimary = false;
      Log::Write("CloudRequest JWT Response Handler");

      switch(error) {
        case Anki::Vector::TokenError::NullToken: {
          // Primary association
          isPrimary = true;
          tokenClient->SendAuthRequest(sessionToken, clientName, appId,
            [this, isPrimary](Anki::Vector::TokenError authError, std::string appToken, std::string authJwtToken) {
            ProcessCloudAuthResponse(isPrimary, authError, appToken, authJwtToken);
          });
        }
        break;
        case Anki::Vector::TokenError::NoError: {
          // Secondary association
          isPrimary = false;
          tokenClient->SendSecondaryAuthRequest(sessionToken, clientName, appId,
            [this, isPrimary](Anki::Vector::TokenError authError, std::string appToken, std::string authJwtToken) {
            Log::Write("CloudRequest Auth Response Handler");
            ProcessCloudAuthResponse(isPrimary, authError, appToken, authJwtToken);
          });
        }
        break;
        case Anki::Vector::TokenError::InvalidToken: {
          // We received an invalid token
          Log::Error("Received invalid token for JwtRequest, try reassociation");
          isPrimary = false;
          tokenClient->SendReassociateAuthRequest(sessionToken, clientName, appId,
            [this, isPrimary](Anki::Vector::TokenError authError, std::string appToken, std::string authJwtToken) {
            Log::Write("CloudRequest Auth Response Handler");
            ProcessCloudAuthResponse(isPrimary, authError, appToken, authJwtToken);
          });
        }
        break;
        case Anki::Vector::TokenError::Connection:
        default: {
          // Could not connect/authorize to server
          Log::Error("Received connection error msg for JwtRequest");
          SendMessage(SwitchboardResponse(Anki::Vector::AuthResponse("", "", error)));
        }
        break;
      }
  });
}

void GatewayMessagingServer::HandleConnectionIdRequest(const SwitchboardRequest& message) {
  _taskExecutor->Wake([this, message]() {
    // Give Gateway our connection id and status
    std::string connectionId = _connectionIdManager->GetConnectionId();
    bool isConnected = connectionId != "";
    SwitchboardResponse res = SwitchboardResponse(ExternalConnectionResponse(isConnected, connectionId));
    SendMessage(res);
  });
}

void GatewayMessagingServer::HandleConnectionIdResponse(const SwitchboardRequest& message) {
  _taskExecutor->Wake([this, message]() {
    // Receive connection id and status from Gateway
    ExternalConnectionResponse response = message.Get_ExternalConnectionResponse();
    
    ConnectionIdRequestCallback cb = _connectionIdRequestCallbackQueue.front();
    _connectionIdRequestCallbackQueue.pop();
    std::weak_ptr<SafeHandle> handle = _connectionIdRequestHandlesQueue.front();
    _connectionIdRequestHandlesQueue.pop();
    if(!handle.expired()) {
      cb(response.isConnected, response.connectionId);
    }
  });
}

void GatewayMessagingServer::HandleClientGuidRefreshResponse(const SwitchboardRequest& message) {
  _taskExecutor->Wake([this, message]() {
    // Receive client guid refresh from Gateway
    ClientGuidRefreshRequestCallback cb = _refreshClientGuidRequestCallbackQueue.front();
    _refreshClientGuidRequestCallbackQueue.pop();
    std::weak_ptr<SafeHandle> handle = _refreshClientGuidRequestHandlesQueue.front();
    _refreshClientGuidRequestHandlesQueue.pop();
    if(!handle.expired()) {
      cb(true);
    }
  });
}

void GatewayMessagingServer::HandleSdkProxyResponse(const SwitchboardRequest& message) {
  _taskExecutor->Wake([this, message]() {
    // Handle SdkProxyResponse
    SdkProxyResponse response = message.Get_SdkProxyResponse();

    auto callbackIter = _sdkProxyRequestCallbackQueue.find(response.messageId);
    auto handleIter = _sdkProxyRequestHandlesQueue.find(response.messageId);

    if(callbackIter != _sdkProxyRequestCallbackQueue.end() &&
      handleIter != _sdkProxyRequestHandlesQueue.end()) {
      
      if(!handleIter->second.expired()) {
        callbackIter->second(response.messageId, response.statusCode, response.contentType, response.content);
      }

      _sdkProxyRequestCallbackQueue.erase(callbackIter);
      _sdkProxyRequestHandlesQueue.erase(handleIter);
    } else {
      // handle case where somehow message id doesn't exist
      Log::Write("GatewayMessageServer received Sdk Proxy Response from gateway with unknown id.");
    }
  });
}

void GatewayMessagingServer::ProcessCloudAuthResponse(bool isPrimary, Anki::Vector::TokenError authError, std::string appToken, std::string authJwtToken) {
  // Send SwitchboardResponse
  SwitchboardResponse res = SwitchboardResponse(Anki::Vector::AuthResponse(appToken, "", authError));
  SendMessage(res);
}

void GatewayMessagingServer::sEvGatewayMessageHandler(struct ev_loop* loop, struct ev_timer* w, int revents) {
  struct ev_GatewayMessageTimerStruct *wData = (struct ev_GatewayMessageTimerStruct*)w;

  GatewayMessagingServer* self = wData->messagingServer;
  int recvSize;

  while((recvSize = wData->server->Recv((char*)sMessageData, sizeof(sMessageData))) > kMessageHeaderLength) {
    // Get message tag, and adjust for header size
    const uint8_t* msgPayload = (const uint8_t*)&sMessageData[kMessageHeaderLength];

    const SwitchboardRequestTag messageTag = (SwitchboardRequestTag)*msgPayload;

    SwitchboardRequest message;
    uint16_t msgSize = *(uint16_t*)sMessageData;

    if(msgSize > (sizeof(sMessageData) - sizeof(msgSize))) {
      // if the size is greater than the buffer we support, than don't try to read
      Log::Write("GatewayMessagingServer received message from vic-gateway that didn't fit into our buffer.");
      continue;
    }

    size_t unpackedSize = message.Unpack(msgPayload, msgSize);

    if(unpackedSize != (size_t)msgSize) {
      continue;
    }

    switch(messageTag) {
      case SwitchboardRequestTag::AuthRequest:
      {
        self->HandleAuthRequest(message);
        break;
      }
      case SwitchboardRequestTag::ExternalConnectionRequest:
      {
        self->HandleConnectionIdRequest(message);
        break;
      }
      case SwitchboardRequestTag::ExternalConnectionResponse:
      {
        self->HandleConnectionIdResponse(message);
        break;
      }
      case SwitchboardRequestTag::ClientGuidRefreshResponse:
      {
        self->HandleClientGuidRefreshResponse(message);
        break;
      }
      case SwitchboardRequestTag::SdkProxyResponse:
      {
        self->HandleSdkProxyResponse(message);
        break;
      }
      default:
        break;
    }
  }
}

bool GatewayMessagingServer::SendMessage(const SwitchboardResponse& message) {
  if (_server.HasClient()) {
    uint16_t message_size = message.Size();
    uint8_t buffer[message_size + kMessageHeaderLength];
    message.Pack(buffer + kMessageHeaderLength, message_size);
    memcpy(buffer, &message_size, kMessageHeaderLength);

    const ssize_t res = _server.Send((const char*)buffer, sizeof(buffer));
    if (res < 0) {
      _server.Disconnect();
      return false;
    }
    return true;
  }
  return false;
}

} // Switchboard
} // Anki
