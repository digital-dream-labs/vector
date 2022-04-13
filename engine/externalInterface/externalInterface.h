/**
* File: externalInterface
*
* Author: damjan stulic
* Created: 7/18/15
*
* Description: 
*
* Copyright: Anki, inc. 2015
* COZMO_PUBLIC_HEADER
*/

#ifndef __Anki_Cozmo_Basestation_ExternalInterface_ExternalInterface_H__
#define __Anki_Cozmo_Basestation_ExternalInterface_ExternalInterface_H__

#include "engine/events/ankiEvent.h"
#include "util/signals/simpleSignal.hpp"
#include <vector>
#include <utility>

namespace Anki {
namespace Vector {

//forward declarations
namespace ExternalInterface {
class MessageEngineToGame;
class MessageGameToEngine;
enum class MessageEngineToGameTag : uint8_t;
enum class MessageGameToEngineTag : uint8_t;
} // end namespace ExternalInterface

enum class SdkStatusType : uint8_t;

class IExternalInterface {
public:
  virtual ~IExternalInterface() {};
  
  virtual void Broadcast(const ExternalInterface::MessageGameToEngine& message) = 0;
  virtual void Broadcast(ExternalInterface::MessageGameToEngine&& message) = 0;
  virtual void BroadcastDeferred(const ExternalInterface::MessageGameToEngine& message) = 0;
  virtual void BroadcastDeferred(ExternalInterface::MessageGameToEngine&& message) = 0;
  
  virtual void Broadcast(const ExternalInterface::MessageEngineToGame& message) = 0;
  virtual void Broadcast(ExternalInterface::MessageEngineToGame&& message) = 0;
  virtual void BroadcastDeferred(const ExternalInterface::MessageEngineToGame& message) = 0;
  virtual void BroadcastDeferred(ExternalInterface::MessageEngineToGame&& message) = 0;
  
  template<typename T, typename ...Args>
  void BroadcastToGame(Args&& ...args)
  {
    Broadcast(ExternalInterface::MessageEngineToGame(T(std::forward<Args>(args)...)));
  }
  
  template<typename T, typename ...Args>
  void BroadcastToEngine(Args&& ...args)
  {
    Broadcast(ExternalInterface::MessageGameToEngine(T(std::forward<Args>(args)...)));
  }
  
  virtual Signal::SmartHandle Subscribe(const ExternalInterface::MessageEngineToGameTag& tagType, std::function<void(const AnkiEvent<ExternalInterface::MessageEngineToGame>&)> messageHandler) = 0;
  
  virtual Signal::SmartHandle Subscribe(const ExternalInterface::MessageGameToEngineTag& tagType, std::function<void(const AnkiEvent<ExternalInterface::MessageGameToEngine>&)> messageHandler) = 0;
  
  virtual void SetSdkStatus(SdkStatusType statusType, std::string&& statusText) = 0;
  
  virtual uint32_t GetMessageCountGtE() const = 0;
  virtual uint32_t GetMessageCountEtG() const = 0;
  virtual void     ResetMessageCounts() = 0;

  using DestinationId = uint32_t;
  static constexpr DestinationId kDestinationIdEveryone = 0xffffffff;
  
protected:
  virtual void DeliverToGame(const ExternalInterface::MessageEngineToGame& message, DestinationId desinationId) = 0;
  
  //virtual bool HasMoreMessagesForEngine() const;
  //virtual const MessageGameToEngine GetNextUndeliveredMessage();
  //virtual void GetNextUndeliveredMessages();
};


class SimpleExternalInterface : public IExternalInterface {
protected:
  void DeliverToGame(const ExternalInterface::MessageEngineToGame& message, DestinationId desinationId) override;

};

} // end namespace Vector
} // end namespace Anki

#endif //__Anki_Cozmo_Basestation_ExternalInterface_ExternalInterface_H__
