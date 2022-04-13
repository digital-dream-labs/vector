/**
 * File: gameMessageHandler.h
 *
 * Author: Andrew Stein
 * Date:   12/15/2014
 *
 * Description: Handles messages from basestation (eventually, game) to UI, 
 *              analogously to the way RobotMessageHandler handles messages from
 *              robot to basestation and UiMessageHandler handles messages
 *              from basestation to UI.
 *
 * Copyright: Anki, Inc. 2014
 **/

#ifndef ANKI_COZMO_GAME_MESSAGE_HANDLER_H
#define ANKI_COZMO_GAME_MESSAGE_HANDLER_H

#include "coretech/common/shared/types.h"

#include "coretech/messaging/engine/IComms.h"
#include "clad/externalInterface/messageEngineToGame.h"
#include "clad/externalInterface/messageGameToEngine.h"

namespace Anki {
  
  namespace Comms {
    class IComms;
  }
  
namespace Vector {
  
  
  // Define interface for a GameMessage handler
  class IGameMessageHandler
  {
  public:
    
    // TODO: Change these to interface references so they can be stubbed as well
    virtual Result Init(Comms::IComms* comms) = 0;
    
    virtual bool IsInitialized() const = 0;
    
    virtual Result ProcessMessages() = 0;
    
    virtual Result SendMessage(const UserDeviceID_t devID, const ExternalInterface::MessageGameToEngine& msg) = 0;
    
  }; // class IGameMessageHandler
  
  
  // The actual GameMessage handler implementation
  class GameMessageHandler : public IGameMessageHandler
  {
  public:
    GameMessageHandler(); // Force construction with stuff in Init()?
    
    // Set the message handler's communications manager
    virtual Result Init(Comms::IComms* comms) override;
    
    virtual bool IsInitialized() const override;
    
    // As long as there are messages available from the comms object,
    // process them and pass them along to robots.
    // Returns RESULT_FAIL if no handler callback was registered for one or more of the received messages.
    virtual Result ProcessMessages() override;
    
    // Send a message to a specified ID
    Result SendMessage(const UserDeviceID_t devID, const ExternalInterface::MessageGameToEngine& msg) override;
    
    inline void RegisterCallbackForMessage(const std::function<void(const ExternalInterface::MessageEngineToGame&)>& messageCallback)
    {
      this->messageCallback = messageCallback;
    }

  protected:
    
    Comms::IComms* comms_;
    
    bool isInitialized_;
    
    std::function<void(const ExternalInterface::MessageEngineToGame&)> messageCallback;

    // Process a raw byte buffer as a message and send it to the specified
    // robot.
    // Returns RESULT_FAIL if no handler callback was registered for this message.
    Result ProcessPacket(const std::vector<uint8_t>& buffer);
    
  }; // class GameMessageHandler
  

  // A stub for testing without a GameMessage handler
  class GameMessageHandlerStub : public IGameMessageHandler
  {
    GameMessageHandlerStub() { }
    
    virtual Result Init(Comms::IComms* comms) override
    {
      return RESULT_OK;
    }
    
    virtual bool IsInitialized() const override {
      return true;
    }
    
    // As long as there are messages available from the comms object,
    // process them and pass them along to robots.
    virtual Result ProcessMessages() override {
      return RESULT_OK;
    }
    
    // Send a message to a specified ID
    virtual Result SendMessage(const UserDeviceID_t devID, const ExternalInterface::MessageGameToEngine& msg) override {
      return RESULT_OK;
    }

  }; // class GameMessageHandlerStub

  
} // namespace Vector
} // namespace Anki


#endif // ANKI_COZMO_GAME_MESSAGE_HANDLER_H
