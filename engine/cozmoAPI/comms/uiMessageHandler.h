/**
 * File: uiMessageHandler.h
 *
 * Author: Kevin Yoon
 * Date:   7/11/2014
 *
 * Description: Handles messages between UI and basestation just as 
 *              MessageHandler handles messages between basestation and robot.
 *
 * Copyright: Anki, Inc. 2014
 **/

#ifndef COZMO_UI_MESSAGEHANDLER_H
#define COZMO_UI_MESSAGEHANDLER_H

#include "coretech/common/shared/types.h"
#include "engine/events/ankiEventMgr.h"
#include "engine/externalInterface/externalInterface.h"
#include "engine/cozmoAPI/comms/iSocketComms.h"
#include "engine/cozmoAPI/comms/sdkStatus.h"
#include "clad/externalInterface/messageEngineToGame.h"
#include "clad/externalInterface/messageGameToEngine.h"
#include "clad/types/uiConnectionTypes.h"
#include "util/signals/simpleSignal_fwd.h"

#include <memory>

// Forward declarations
namespace Anki {

namespace Util {
  namespace Stats {
    class StatsAccumulator;
  }
}
}

namespace Anki {
  namespace Vector {
    

    class CozmoContext;
    class Robot;
    class RobotManager;
    
    class UiMessageHandler : public IExternalInterface
    {
    public:
      
      UiMessageHandler(u32 hostUiDeviceID); // Force construction with stuff in Init()?
      virtual ~UiMessageHandler();
      
      Result Init(CozmoContext* context, const Json::Value& config);
      
      Result Update();
      
      virtual void Broadcast(const ExternalInterface::MessageGameToEngine& message) override;
      virtual void Broadcast(ExternalInterface::MessageGameToEngine&& message) override;
      virtual void BroadcastDeferred(const ExternalInterface::MessageGameToEngine& message) override;
      virtual void BroadcastDeferred(ExternalInterface::MessageGameToEngine&& message) override;
      
      virtual void Broadcast(const ExternalInterface::MessageEngineToGame& message) override;
      virtual void Broadcast(ExternalInterface::MessageEngineToGame&& message) override;
      virtual void BroadcastDeferred(const ExternalInterface::MessageEngineToGame& message) override;
      virtual void BroadcastDeferred(ExternalInterface::MessageEngineToGame&& message) override;
      
      virtual Signal::SmartHandle Subscribe(const ExternalInterface::MessageEngineToGameTag& tagType, std::function<void(const AnkiEvent<ExternalInterface::MessageEngineToGame>&)> messageHandler) override;
      
      virtual Signal::SmartHandle Subscribe(const ExternalInterface::MessageGameToEngineTag& tagType, std::function<void(const AnkiEvent<ExternalInterface::MessageGameToEngine>&)> messageHandler) override;
      
      inline u32 GetHostUiDeviceID() const { return _hostUiDeviceID; }
      
      AnkiEventMgr<ExternalInterface::MessageEngineToGame>& GetEventMgrToGame() { return _eventMgrToGame; }
      AnkiEventMgr<ExternalInterface::MessageGameToEngine>& GetEventMgrToEngine() { return _eventMgrToEngine; }
      
      const Util::Stats::StatsAccumulator& GetLatencyStats(UiConnectionType type) const;
      
      bool HasDesiredNumUiDevices() const;
      
      virtual void SetSdkStatus(SdkStatusType statusType, std::string&& statusText) override
      {
        _sdkStatus.SetStatus(statusType, std::move(statusText));
      }

      virtual uint32_t GetMessageCountGtE() const override { return _messageCountGameToEngine; }
      virtual uint32_t GetMessageCountEtG() const override { return _messageCountEngineToGame; }
      virtual void     ResetMessageCounts() override { _messageCountGameToEngine = 0; _messageCountEngineToGame = 0; }

    private:
      
      // ============================== Private Member Functions ==============================
      
      const ISocketComms* GetSocketComms(UiConnectionType type) const
      {
        const uint32_t typeIndex = (uint32_t)type;
        const bool inRange = (typeIndex < uint32_t(UiConnectionType::Count));
        assert(inRange);
        return inRange ? _socketComms[typeIndex] : nullptr;
      }
      
      ISocketComms* GetSocketComms(UiConnectionType type)
      {
        return const_cast<ISocketComms*>( const_cast<const UiMessageHandler*>(this)->GetSocketComms(type) );
      }
      
      bool AreAnyConnectedDevicesOnAnySocket() const;
      
      bool ShouldHandleMessagesFromConnection(UiConnectionType type) const;
      
      void UpdateSdk();
      
      // As long as there are messages available from the comms object,
      // process them and pass them along to robots.
      Result ProcessMessages();
      
      // Process a raw byte buffer as a GameToEngine CLAD message and broadcast it
      Result ProcessMessageBytes(const uint8_t* packetBytes, size_t packetSize,
                                 UiConnectionType connectionType, bool isSingleMessage, bool handleMessagesFromConnection);
      void HandleProcessedMessage(const ExternalInterface::MessageGameToEngine& message, UiConnectionType connectionType,
                                  size_t messageSize, bool handleMessagesFromConnection);
      
      // Send a message to a specified ID
      virtual void DeliverToGame(const ExternalInterface::MessageEngineToGame& message, DestinationId = kDestinationIdEveryone) override;
      
      bool ConnectToUiDevice(ISocketComms::DeviceId deviceId, UiConnectionType connectionType);
      void HandleEvents(const AnkiEvent<ExternalInterface::MessageGameToEngine>& event);
      
      // ============================== Private Types ==============================
      
      using MessageEngineToGame = ExternalInterface::MessageEngineToGame;
      using MessageGameToEngine = ExternalInterface::MessageGameToEngine;
      
      // ============================== Private Member Vars ==============================

      ISocketComms* _socketComms[(size_t)UiConnectionType::Count];
      
      std::vector<Signal::SmartHandle>    _signalHandles;
      
      AnkiEventMgr<MessageEngineToGame>   _eventMgrToGame;
      AnkiEventMgr<MessageGameToEngine>   _eventMgrToEngine;
      
      std::vector<MessageGameToEngine>    _threadedMsgsToEngine;
      std::vector<MessageEngineToGame>    _threadedMsgsToGame;
      std::mutex                          _mutex;
      
      SdkStatus                           _sdkStatus;
      
      uint32_t                            _hostUiDeviceID = 0;
      
      uint32_t                            _updateCount = 0;
      
      double                              _lastPingTime_ms = 0.f;
      
      UiConnectionType                    _connectionSource = UiConnectionType::Count;  // Connection source currently processing messages
      
      bool                                _isInitialized = false;

      CozmoContext*                       _context = nullptr;

      uint32_t                            _messageCountGameToEngine = 0;
      uint32_t                            _messageCountEngineToGame = 0;
      
    }; // class MessageHandler
    
    
#undef MESSAGE_BASECLASS_NAME
    
  } // namespace Vector
} // namespace Anki


#endif // COZMO_MESSAGEHANDLER_H
