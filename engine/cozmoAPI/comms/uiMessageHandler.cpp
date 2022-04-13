/**
 * File: uiMessageHandler.cpp
 *
 * Author: Kevin Yoon
 * Date:   7/11/2014
 *
 * Description: Handles messages between UI and basestation just as
 *              RobotMessageHandler handles messages between basestation and robot.
 *
 * Copyright: Anki, Inc. 2014
 **/

#include "util/logging/logging.h"
#include "util/global/globalDefinitions.h"

#include "engine/cozmoContext.h"
#include "engine/debug/devLoggingSystem.h"
#include "engine/cozmoAPI/comms/protoCladInterpreter.h"
#include "engine/cozmoAPI/comms/localUdpSocketComms.h"
#include "engine/cozmoAPI/comms/udpSocketComms.h"
#include "engine/cozmoAPI/comms/uiMessageHandler.h"

#include "engine/viz/vizManager.h"
#include "engine/buildVersion.h"
#include "coretech/common/engine/utils/timer.h"

#include "anki/cozmo/shared/cozmoConfig.h"
#include "coretech/messaging/shared/socketConstants.h"

#include "coretech/messaging/engine/IComms.h"

#include "clad/externalInterface/messageGameToEngine_hash.h"
#include "clad/externalInterface/messageGameToEngineTag.h"
#include "clad/externalInterface/messageEngineToGame_hash.h"

#include "util/console/consoleInterface.h"
#include "util/cpuProfiler/cpuProfiler.h"
#include "util/enums/enumOperators.h"
#include "util/helpers/ankiDefines.h"
#include "util/time/universalTime.h"

#ifdef SIMULATOR
#include "osState/osState.h"
#endif

// The amount of time that the UI must have not been
// returning pings before we consider it disconnected
#ifdef SIMULATOR
// No timeout in sim
static const u32 kPingTimeoutForDisconnect_ms = 0;
#else
static const u32 kPingTimeoutForDisconnect_ms = 5000;
#endif

namespace Anki {
  namespace Vector {
    IMPLEMENT_ENUM_INCREMENT_OPERATORS(UiConnectionType);


    CONSOLE_VAR(bool, kAcceptMessagesFromUI,  "UiComms", true);
    CONSOLE_VAR(double, kPingSendFreq_ms, "UiComms", 1000.0); // 0 = never
    CONSOLE_VAR(uint32_t, kSdkStatusSendFreq, "UiComms", 1); // 0 = never


    bool IsExternalSdkConnection(UiConnectionType type)
    {
      switch(type)
      {
        case UiConnectionType::UI:          return false;
        case UiConnectionType::SdkOverUdp:  return true;
        case UiConnectionType::SdkOverTcp:  return true;
        case UiConnectionType::Switchboard: return false;
        case UiConnectionType::Gateway:     return false;
        default:
        {
          PRINT_NAMED_ERROR("IsExternalSdkConnection.BadType", "type = %d", (int)type);
          assert(0);
          return false;
        }
      }
    }


    ISocketComms* CreateSocketComms(UiConnectionType type,
                                    ISocketComms::DeviceId hostDeviceId)
    {
      // Note: Some SocketComms are deliberately null depending on the build platform, type etc.
#if FACTORY_TEST
      if(type != UiConnectionType::Switchboard)
      {
        return nullptr;
      }
#endif

      switch(type)
      {
        case UiConnectionType::UI:
        {
          return new UdpSocketComms(type);
        }
        case UiConnectionType::SdkOverUdp:
        {
          return nullptr;
        }
        case UiConnectionType::SdkOverTcp:
        {
          return nullptr;
        }
        case UiConnectionType::Switchboard:
        {
          ISocketComms* comms = new LocalUdpSocketComms(true, ENGINE_SWITCH_SERVER_PATH);
          return comms;
        }
        case UiConnectionType::Gateway:
        {
          ISocketComms* comms = new LocalUdpSocketComms(true, ENGINE_GATEWAY_SERVER_PATH);
          return comms;
        }
        default:
        {
          assert(0);
          return nullptr;
        }
      }
    }


    UiMessageHandler::UiMessageHandler(u32 hostUiDeviceID)
      : _sdkStatus()
      , _hostUiDeviceID(hostUiDeviceID)
      , _messageCountGameToEngine(0)
      , _messageCountEngineToGame(0)
    {

      // Currently not supporting UI connections for any sim robot other
      // than the default ID
      #ifdef SIMULATOR
      const auto robotID = OSState::getInstance()->GetRobotID();
      if (robotID != DEFAULT_ROBOT_ID) {
        PRINT_NAMED_WARNING("UiMessageHandler.Ctor.SkippingUIConnections",
                            "RobotID: %d - Only DEFAULT_ROBOT_ID may accept UI connections",
                            robotID);

        for (UiConnectionType i = UiConnectionType(0); i < UiConnectionType::Count; ++i) {
          _socketComms[(uint32_t)i] = 0;
        }
        return;
      }
      #endif

      for (UiConnectionType i = UiConnectionType(0); i < UiConnectionType::Count; ++i)
      {
        auto& socket = _socketComms[(uint32_t)i];
        socket = CreateSocketComms(i, GetHostUiDeviceID());

        // If UI disconnects due to timeout, disconnect Viz too
        if ((i == UiConnectionType::UI) && (socket != nullptr)) {
          ISocketComms::DisconnectCallback disconnectCallback = [this]() {
            _context->GetVizManager()->Disconnect();
          };
          socket->SetPingTimeoutForDisconnect(kPingTimeoutForDisconnect_ms, disconnectCallback);
        }
      }
    }


    UiMessageHandler::~UiMessageHandler()
    {
      for (uint32_t i = 0; i < (uint32_t)UiConnectionType::Count; ++i)
      {
        delete _socketComms[i];
        _socketComms[i] = nullptr;
      }
    }


    Result UiMessageHandler::Init(CozmoContext* context, const Json::Value& config)
    {
      for (UiConnectionType i = UiConnectionType(0); i < UiConnectionType::Count; ++i)
      {
        ISocketComms* socketComms = GetSocketComms(i);
        if (socketComms)
        {
          if (!socketComms->Init(i, config))
          {
            return RESULT_FAIL;
          }
        }
      }

      _isInitialized = true;

      _context = context;

      // We'll use this callback for simple events we care about
      auto commonCallback = std::bind(&UiMessageHandler::HandleEvents, this, std::placeholders::_1);

      // Subscribe to desired simple events
      _signalHandles.push_back(Subscribe(ExternalInterface::MessageGameToEngineTag::ConnectToUiDevice, commonCallback));
      _signalHandles.push_back(Subscribe(ExternalInterface::MessageGameToEngineTag::DisconnectFromUiDevice, commonCallback));
      _signalHandles.push_back(Subscribe(ExternalInterface::MessageGameToEngineTag::UiDeviceConnectionWrongVersion, commonCallback));
      _signalHandles.push_back(Subscribe(ExternalInterface::MessageGameToEngineTag::TransferFile, commonCallback));

      return RESULT_OK;
    }


    bool UiMessageHandler::ShouldHandleMessagesFromConnection(UiConnectionType type) const
    {
      switch(type)
      {
        case UiConnectionType::UI:          return kAcceptMessagesFromUI;
        case UiConnectionType::SdkOverUdp:  return false;
        case UiConnectionType::SdkOverTcp:  return false;
        case UiConnectionType::Switchboard: return true;
        case UiConnectionType::Gateway:     return true;
        default:
        {
          assert(0);
          return true;
        }
      }
    }

    bool UiMessageHandler::AreAnyConnectedDevicesOnAnySocket() const
    {
      for (UiConnectionType i = UiConnectionType(0); i < UiConnectionType::Count; ++i)
      {
        const ISocketComms* socketComms = GetSocketComms(i);
        if (socketComms)
        {
          if (socketComms->GetNumConnectedDevices() > 0)
            return true;
        }
      }
      return false;
    }


    void UiMessageHandler::DeliverToGame(const ExternalInterface::MessageEngineToGame& message, DestinationId destinationId)
    {
      // There is almost always a connected device, so better to just always pack the message even if it won't be sent
      // pterry 09/26/2018: Verified this is still true; I think because we use messages as engine-to-engine
      //if (AreAnyConnectedDevicesOnAnySocket())
      {
        ANKI_CPU_PROFILE("UiMH::DeliverToGame");

        ++_messageCountEngineToGame;

        Comms::MsgPacket p;
        message.Pack(p.data, Comms::MsgPacket::MAX_SIZE);

        (void) ProtoCladInterpreter::Redirect(message, _context);

        #if ANKI_DEV_CHEATS
        if (nullptr != DevLoggingSystem::GetInstance())
        {
          DevLoggingSystem::GetInstance()->LogMessage(message);
        }
        #endif

        p.dataLen = message.Size();
        p.destId = _hostUiDeviceID;

        const bool sendToEveryone = (destinationId == kDestinationIdEveryone);
        UiConnectionType connectionType = sendToEveryone ? UiConnectionType(0) : (UiConnectionType)destinationId;
        if (connectionType >= UiConnectionType::Count)
        {
          PRINT_NAMED_WARNING("UiMessageHandler.DeliverToGame.BadDestinationId", "Invalid destinationId %u = UiConnectionType '%s'", destinationId, EnumToString(connectionType));
        }

        while (connectionType < UiConnectionType::Count)
        {
          ISocketComms* socketComms = GetSocketComms(connectionType);
          if (socketComms)
          {
            socketComms->SendMessage(p);
          }

          if (sendToEveryone)
          {
            ++connectionType;
          }
          else
          {
            return;
          }
        }
      }
    }


    Result UiMessageHandler::ProcessMessageBytes(const uint8_t* const packetBytes, const size_t packetSize,
                                                 UiConnectionType connectionType, bool isSingleMessage, bool handleMessagesFromConnection)
    {
      ANKI_CPU_PROFILE("UiMH::ProcessMessageBytes");

      ExternalInterface::MessageGameToEngine message;
      uint16_t bytesRemaining = packetSize;
      const uint8_t* messagePtr = packetBytes;

      while (bytesRemaining > 0)
      {
        const size_t bytesUnpacked = message.Unpack(messagePtr, bytesRemaining);
        if (isSingleMessage && (bytesUnpacked != packetSize))
        {
          PRINT_STREAM_ERROR("UiMessageHandler.MessageBufferWrongSize",
                             "Buffer's size does not match expected size for this message ID. (Msg "
                              << ExternalInterface::MessageGameToEngineTagToString(message.GetTag())
                              << ", expected " << message.Size() << ", recvd " << packetSize << ")" );
          return RESULT_FAIL;
        }
        else if (!isSingleMessage && (bytesUnpacked > bytesRemaining || bytesUnpacked == 0))
        {
          PRINT_STREAM_ERROR("UiMessageHandler.MessageBufferWrongSize",
                            "Buffer overrun reading messages, last message: "
                             << ExternalInterface::MessageGameToEngineTagToString(message.GetTag()));

          return RESULT_FAIL;
        }
        bytesRemaining -= bytesUnpacked;
        messagePtr += bytesUnpacked;

        HandleProcessedMessage(message, connectionType, bytesUnpacked, handleMessagesFromConnection);
      }

      return RESULT_OK;
    }


    bool AlwaysHandleMessageTypeForConnection(ExternalInterface::MessageGameToEngine::Tag messageTag)
    {
      // Return true for small subset of message types that we handle even if we're not listening to that connection
      // We still want to accept certain message types (e.g. console vars to allow a connection to enable itself)

      using GameToEngineTag = ExternalInterface::MessageGameToEngineTag;
      switch (messageTag)
      {
        case GameToEngineTag::SetDebugConsoleVarMessage:    return true;
        case GameToEngineTag::GetDebugConsoleVarMessage:    return true;
        case GameToEngineTag::GetAllDebugConsoleVarMessage: return true;
        default:
          return false;
      }
    }


    void UiMessageHandler::HandleProcessedMessage(const ExternalInterface::MessageGameToEngine& message,
                                UiConnectionType connectionType, size_t messageSize, bool handleMessagesFromConnection)
    {
      ++_messageCountGameToEngine;

      const ExternalInterface::MessageGameToEngine::Tag messageTag = message.GetTag();
      if (!handleMessagesFromConnection)
      {
        // We still want to accept certain message types (e.g. console vars to allow a connection to enable itself)
        if (!AlwaysHandleMessageTypeForConnection(messageTag))
        {
          return;
        }
      }

      #if ANKI_DEV_CHEATS
      if (nullptr != DevLoggingSystem::GetInstance())
      {
        DevLoggingSystem::GetInstance()->LogMessage(message);
      }
      #endif

      // We must handle pings at this level because they are a connection type specific message
      // and must be dealt with at the transport level rather than at the app level
      if (messageTag == ExternalInterface::MessageGameToEngineTag::Ping)
      {
        ISocketComms* socketComms = GetSocketComms(connectionType);
        if (socketComms)
        {
          const ExternalInterface::Ping& pingMsg = message.Get_Ping();
          if (pingMsg.isResponse)
          {
            socketComms->HandlePingResponse(pingMsg);
          }
          else
          {
            ExternalInterface::Ping outPing(pingMsg.counter, pingMsg.timeSent_ms, true);
            ExternalInterface::MessageEngineToGame toSend;
            toSend.Set_Ping(outPing);
            DeliverToGame(std::move(toSend), (DestinationId)connectionType);
          }
        }
      }
      else
      {
        // Send out this message to anyone that's subscribed
        Broadcast(message);
      }
    }

    // Broadcasting MessageGameToEngine messages are only internal
    void UiMessageHandler::Broadcast(const ExternalInterface::MessageGameToEngine& message)
    {
      ANKI_CPU_PROFILE("UiMH::Broadcast_GToE"); // Some expensive and untracked - TODO: Capture message type for profile

      DEV_ASSERT(nullptr == _context || _context->IsEngineThread(),
                 "UiMessageHandler.GameToEngineRef.BroadcastOffEngineThread");

      _eventMgrToEngine.Broadcast(AnkiEvent<ExternalInterface::MessageGameToEngine>(
        BaseStationTimer::getInstance()->GetCurrentTimeInSeconds(), static_cast<u32>(message.GetTag()), message));
    } // Broadcast(MessageGameToEngine)


    void UiMessageHandler::Broadcast(ExternalInterface::MessageGameToEngine&& message)
    {
      ANKI_CPU_PROFILE("UiMH::BroadcastMove_GToE");

      DEV_ASSERT(nullptr == _context || _context->IsEngineThread(),
                 "UiMessageHandler.GameToEngineRval.BroadcastOffEngineThread");

      u32 type = static_cast<u32>(message.GetTag());
      _eventMgrToEngine.Broadcast(AnkiEvent<ExternalInterface::MessageGameToEngine>(
        BaseStationTimer::getInstance()->GetCurrentTimeInSeconds(), type, std::move(message)));
    } // Broadcast(MessageGameToEngine &&)


    // Called from any not main thread and dealt with during the update.
    void UiMessageHandler::BroadcastDeferred(const ExternalInterface::MessageGameToEngine& message)
    {
      ANKI_CPU_PROFILE("UiMH::BroadcastDeferred_GToE");

      std::lock_guard<std::mutex> lock(_mutex);
      _threadedMsgsToEngine.emplace_back(message);
    }


    void UiMessageHandler::BroadcastDeferred(ExternalInterface::MessageGameToEngine&& message)
    {
      ANKI_CPU_PROFILE("UiMH::BroadcastDeferredMove_GToE");

      std::lock_guard<std::mutex> lock(_mutex);
      _threadedMsgsToEngine.emplace_back(std::move(message));
    }


    // Broadcasting MessageEngineToGame messages also delivers them out of the engine
    void UiMessageHandler::Broadcast(const ExternalInterface::MessageEngineToGame& message)
    {
      ANKI_CPU_PROFILE("UiMH::Broadcast_EToG");

      DEV_ASSERT(nullptr == _context || _context->IsEngineThread(),
                 "UiMessageHandler.EngineToGameRef.BroadcastOffEngineThread");

      DeliverToGame(message);
      _eventMgrToGame.Broadcast(AnkiEvent<ExternalInterface::MessageEngineToGame>(
        BaseStationTimer::getInstance()->GetCurrentTimeInSeconds(), static_cast<u32>(message.GetTag()), message));
    } // Broadcast(MessageEngineToGame)


    void UiMessageHandler::Broadcast(ExternalInterface::MessageEngineToGame&& message)
    {
      ANKI_CPU_PROFILE("UiMH::BroadcastMove_EToG");

      DEV_ASSERT(nullptr == _context || _context->IsEngineThread(),
                 "UiMessageHandler.EngineToGameRval.BroadcastOffEngineThread");

      DeliverToGame(message);
      u32 type = static_cast<u32>(message.GetTag());
      _eventMgrToGame.Broadcast(AnkiEvent<ExternalInterface::MessageEngineToGame>(
        BaseStationTimer::getInstance()->GetCurrentTimeInSeconds(), type, std::move(message)));
    } // Broadcast(MessageEngineToGame &&)


    void UiMessageHandler::BroadcastDeferred(const ExternalInterface::MessageEngineToGame& message)
    {
      ANKI_CPU_PROFILE("UiMH::BroadcastDeferred_EToG");

      std::lock_guard<std::mutex> lock(_mutex);
      _threadedMsgsToGame.emplace_back(message);
    }


    void UiMessageHandler::BroadcastDeferred(ExternalInterface::MessageEngineToGame&& message)
    {
      ANKI_CPU_PROFILE("UiMH::BroadcastDeferredMove_EToG");

      std::lock_guard<std::mutex> lock(_mutex);
      _threadedMsgsToGame.emplace_back(std::move(message));
    }


    // Provides a way to subscribe to message types using the AnkiEventMgrs
    Signal::SmartHandle UiMessageHandler::Subscribe(const ExternalInterface::MessageEngineToGameTag& tagType,
                                                    std::function<void(const AnkiEvent<ExternalInterface::MessageEngineToGame>&)> messageHandler)
    {
      return _eventMgrToGame.Subscribe(static_cast<u32>(tagType), messageHandler);
    } // Subscribe(MessageEngineToGame)


    Signal::SmartHandle UiMessageHandler::Subscribe(const ExternalInterface::MessageGameToEngineTag& tagType,
                                                    std::function<void(const AnkiEvent<ExternalInterface::MessageGameToEngine>&)> messageHandler)
    {
      return _eventMgrToEngine.Subscribe(static_cast<u32>(tagType), messageHandler);
    } // Subscribe(MessageGameToEngine)


    Result UiMessageHandler::ProcessMessages()
    {
      ANKI_CPU_PROFILE("UiMH::ProcessMessages");

      Result retVal = RESULT_FAIL;

      if(_isInitialized)
      {
        retVal = RESULT_OK;

        for (UiConnectionType i = UiConnectionType(0); i < UiConnectionType::Count; ++i)
        {
          _connectionSource = i;
          ISocketComms* socketComms = GetSocketComms(i);
          if (socketComms)
          {
            bool keepReadingMessages = true;
            const bool isSingleMessage = !socketComms->AreMessagesGrouped();
            const bool handleMessagesFromConnection = ShouldHandleMessagesFromConnection(i);
            while(keepReadingMessages)
            {
              std::vector<uint8_t> buffer;
              keepReadingMessages = socketComms->RecvMessage(buffer);

              if (keepReadingMessages)
              {
                Result res = ProcessMessageBytes(buffer.data(), buffer.size(), i, isSingleMessage, handleMessagesFromConnection);
                if (res != RESULT_OK)
                {
                  retVal = RESULT_FAIL;
                }
              }
            }
          }
        }
        _connectionSource = UiConnectionType::Count;
      }

      return retVal;
    } // ProcessMessages()


    Result UiMessageHandler::Update()
    {
      ANKI_CPU_PROFILE("UiMH::Update");

      ++_updateCount;

      // Update all the comms

      const double currTime_ms = Util::Time::UniversalTime::GetCurrentTimeInMilliseconds();
      const bool sendPingThisTick = (kPingSendFreq_ms > 0.0) && (currTime_ms - _lastPingTime_ms > kPingSendFreq_ms);

      for (UiConnectionType i = UiConnectionType(0); i < UiConnectionType::Count; ++i)
      {
        ISocketComms* socketComms = GetSocketComms(i);
        if (socketComms)
        {
          socketComms->Update();

          if(sendPingThisTick && (socketComms->GetNumConnectedDevices() > 0))
          {
            // Ping the connection to let them know we're still here

            ANKI_CPU_PROFILE("UiMH::Update::SendPing");

            ExternalInterface::Ping outPing(socketComms->NextPingCounter(), currTime_ms, false);
            ExternalInterface::MessageEngineToGame message(std::move(outPing));
            DeliverToGame(message, (DestinationId)i);
            _lastPingTime_ms = currTime_ms;
          }
        }
      }

      // Read messages from all the comms

      Result lastResult = ProcessMessages();
      if (RESULT_OK != lastResult)
      {
        return lastResult;
      }

      // Send to all of the comms

      for (UiConnectionType i = UiConnectionType(0); i < UiConnectionType::Count; ++i)
      {
        ISocketComms* socketComms = GetSocketComms(i);
        if (socketComms)
        {
          std::vector<ISocketComms::DeviceId> advertisingUiDevices;
          socketComms->GetAdvertisingDeviceIDs(advertisingUiDevices);
          for (ISocketComms::DeviceId deviceId : advertisingUiDevices)
          {
            if (deviceId == GetHostUiDeviceID())
            {
              // Force connection to first UI device if not already connected
              if (ConnectToUiDevice(deviceId, i))
              {
                PRINT_CH_INFO("UiComms", "UiMessageHandler.Update.Connected",
                                 "Automatically connected to local %s device %d!", EnumToString(i), deviceId);
              }
              else
              {
                PRINT_NAMED_WARNING("UiMessageHandler.Update.FailedToConnect",
                                 "Failed to connected to local %s device %d!", EnumToString(i), deviceId);
              }
            }
            else
            {
              Broadcast(ExternalInterface::MessageEngineToGame(ExternalInterface::UiDeviceAvailable(i, deviceId)));
            }
          }
        }
      }

      {
        ANKI_CPU_PROFILE("UiMH::BroadcastThreadedMessagesToEngine");
        std::lock_guard<std::mutex> lock(_mutex);
        if( _threadedMsgsToEngine.size() > 0 )
        {
          for(auto& threaded_msg : _threadedMsgsToEngine) {
            Broadcast(std::move(threaded_msg));
          }
          _threadedMsgsToEngine.clear();
        }
      }

      {
        ANKI_CPU_PROFILE("UiMH::BroadcastThreadedMessagesToGame");
        std::lock_guard<std::mutex> lock(_mutex);
        if( _threadedMsgsToGame.size() > 0 )
        {
          for(auto& threaded_msg : _threadedMsgsToGame) {
            Broadcast(std::move(threaded_msg));
          }
          _threadedMsgsToGame.clear();
        }
      }

      UpdateSdk();

      return lastResult;
    } // Update()

    // TODO Revisit this. Use for SDK DAS messages?
    void UiMessageHandler::UpdateSdk()
    {
    }

    bool UiMessageHandler::ConnectToUiDevice(ISocketComms::DeviceId deviceId, UiConnectionType connectionType)
    {
      ISocketComms* socketComms = GetSocketComms(connectionType);

      const bool success = socketComms ? socketComms->ConnectToDeviceByID(deviceId) : false;

      std::array<uint8_t, 16> toGameCLADHash;
      std::copy(std::begin(messageEngineToGameHash), std::end(messageEngineToGameHash), std::begin(toGameCLADHash));

      std::array<uint8_t, 16> toEngineCLADHash;
      std::copy(std::begin(messageGameToEngineHash), std::end(messageGameToEngineHash), std::begin(toEngineCLADHash));

      // kReservedForTag is for future proofing - if we need to increase tag size to 16 bits, the
      const uint8_t kReservedForTag = 0;
      ExternalInterface::UiDeviceConnected deviceConnected(kReservedForTag,
                                                           connectionType,
                                                           deviceId,
                                                           success,
                                                           toGameCLADHash,
                                                           toEngineCLADHash,
                                                           kBuildVersion);

      Broadcast( ExternalInterface::MessageEngineToGame(std::move(deviceConnected)) );

      if (success)
      {
        // Ask Robot to send per-robot settings to Game/SDK
        Broadcast( ExternalInterface::MessageGameToEngine(ExternalInterface::RequestRobotSettings()) );
      }

      return success;
    }

    void UiMessageHandler::HandleEvents(const AnkiEvent<ExternalInterface::MessageGameToEngine>& event)
    {
      switch (event.GetData().GetTag())
      {
        case ExternalInterface::MessageGameToEngineTag::UiDeviceConnectionWrongVersion:
        {
          const ExternalInterface::UiDeviceConnectionWrongVersion& msg = event.GetData().Get_UiDeviceConnectionWrongVersion();
          if (IsExternalSdkConnection(msg.connectionType))
          {
            _sdkStatus.OnWrongVersion(msg);
            ISocketComms* socketComms = GetSocketComms(msg.connectionType);
            if (socketComms)
            {
              socketComms->DisconnectDeviceByID(msg.deviceID);
            }
          }
          break;
        }
        case ExternalInterface::MessageGameToEngineTag::ConnectToUiDevice:
        {
          const ExternalInterface::ConnectToUiDevice& msg = event.GetData().Get_ConnectToUiDevice();
          const ISocketComms::DeviceId deviceID = msg.deviceID;

          const bool success = ConnectToUiDevice(deviceID, msg.connectionType);
          if(success) {
            PRINT_CH_INFO("UiComms", "UiMessageHandler.HandleEvents", "Connected to %s device %d!",
                             EnumToString(msg.connectionType), deviceID);
          } else {
            PRINT_NAMED_ERROR("UiMessageHandler.HandleEvents", "Failed to connect to %s device %d!",
                              EnumToString(msg.connectionType), deviceID);
          }

          break;
        }
        case ExternalInterface::MessageGameToEngineTag::DisconnectFromUiDevice:
        {
          const ExternalInterface::DisconnectFromUiDevice& msg = event.GetData().Get_DisconnectFromUiDevice();

          ISocketComms* socketComms = GetSocketComms(msg.connectionType);
          const ISocketComms::DeviceId deviceId = msg.deviceID;

          if (socketComms && socketComms->DisconnectDeviceByID(deviceId))
          {
            PRINT_CH_INFO("UiComms", "UiMessageHandler.ProcessMessage", "Disconnected from %s device %d!",
                             EnumToString(msg.connectionType), deviceId);
          }

          break;
        }
        default:
        {
          PRINT_STREAM_ERROR("UiMessageHandler.HandleEvents",
                             "Subscribed to unhandled event of type "
                             << ExternalInterface::MessageGameToEngineTagToString(event.GetData().GetTag()) << "!");
        }
      }
    }


    bool UiMessageHandler::HasDesiredNumUiDevices() const
    {
      for (UiConnectionType i = UiConnectionType(0); i < UiConnectionType::Count; ++i)
      {
        // Ignore switchboard's numDesiredDevices
        if(i == UiConnectionType::Switchboard || i == UiConnectionType::Gateway)
        {
          continue;
        }

        const ISocketComms* socketComms = GetSocketComms(i);
        if (socketComms && socketComms->HasDesiredDevices())
        {
          return true;
        }
      }

      return false;
    }


    const Util::Stats::StatsAccumulator& UiMessageHandler::GetLatencyStats(UiConnectionType type) const
    {
      const ISocketComms* socketComms = GetSocketComms(type);
      if (socketComms)
      {
        return socketComms->GetLatencyStats();
      }
      else
      {
        static Util::Stats::StatsAccumulator sDummyStats;
        return sDummyStats;
      }
    }

  } // namespace Vector
} // namespace Anki
