/**
 * File: ankiEventUtil.h
 *
 * Author: Lee Crippen
 * Created: 10/30/15
 *
 * Description: Helper for ExternalInterface::MessageGameToEngine handling in your class. Add a public
 
 template<typename T>
 void HandleMessage(const T& msg);
 
 to your header. Then, in class, add specializations for each message type to handle like so:
 
 template<>
 void YourClass::HandleMessage(const MessageType& msg)
 {
 
 }
 
 *
 * Copyright: Anki, Inc. 2015
 *
 **/

#ifndef __Cozmo_Basestation_AnkiEventUtil_H__
#define __Cozmo_Basestation_AnkiEventUtil_H__


#include "engine/externalInterface/externalInterface.h"
#include "clad/externalInterface/messageGameToEngine.h"
#include "clad/externalInterface/messageEngineToGame.h"

namespace Anki {
namespace Vector {
  
// forward declarations
class IExternalInterface;
template<typename T> class AnkiEvent;
  
template <typename T, typename H>
class AnkiEventUtil
{
public:
  AnkiEventUtil(IExternalInterface& externalInterface, T& object, H& handlersIn)
  : _interface(externalInterface)
  , _object(object)
  , _eventHandlers(handlersIn)
  { }
  
  template <ExternalInterface::MessageGameToEngineTag Tag>
  void SubscribeGameToEngine()
  {
    T& temp = _object;
    _eventHandlers.push_back(_interface.Subscribe(Tag,
      [&temp] (const AnkiEvent<ExternalInterface::MessageGameToEngine>& event)
      {
        temp.HandleMessage(event.GetData().Get_<Tag>());
      }));
  }
  
  template <ExternalInterface::MessageEngineToGameTag Tag>
  void SubscribeEngineToGame()
  {
    T& temp = _object;
    _eventHandlers.push_back(_interface.Subscribe(Tag,
      [&temp] (const AnkiEvent<ExternalInterface::MessageEngineToGame>& event)
      {
        temp.HandleMessage(event.GetData().Get_<Tag>());
      }));
  }
  
private:
  IExternalInterface& _interface;
  T& _object;
  H& _eventHandlers;
  
}; // class AnkiEventUtil

template <typename TNew, typename HNew>
AnkiEventUtil<TNew, HNew> MakeAnkiEventUtil(IExternalInterface& externalInterface, TNew& object, HNew& handlersIn)
{
  return AnkiEventUtil<TNew, HNew>(externalInterface, object, handlersIn);
}
  
} // namespace Vector
} // namespace Anki

#endif //  __Cozmo_Basestation_AnkiEventUtil_H__
