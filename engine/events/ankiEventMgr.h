/**
 * File: ankiEventMgr.h
 *
 * Author: Lee Crippen
 * Created: 07/30/15
 *
 * Description: Manager for events to be used across the engine; responsible for keeping track of events that can
 *              trigger, registered listeners, and dispatching events when they occur.
 *
 * Copyright: Anki, Inc. 2015
 *
 * COZMO_PUBLIC_HEADER
 **/

#ifndef __Anki_Cozmo_Basestation_Events_AnkiEventMgr_H__
#define __Anki_Cozmo_Basestation_Events_AnkiEventMgr_H__

#pragma mark

#include "engine/events/ankiEvent.h"
#include "util/signals/simpleSignal.hpp"
#include "util/helpers/noncopyable.h"
#include <stdint.h>
#include <unordered_map>
#include <functional>

namespace Anki {
namespace Vector {


// Base class for the AnkiEventMgr specialized variations
template<typename DataType, typename SignalStruct>
class AnkiEventMgrBase : private Util::noncopyable
{
public:
  virtual ~AnkiEventMgrBase() { }
  
  virtual void UnsubscribeAll()
  {
    _eventHandlerMap.clear();
  }
  
protected:
  std::unordered_map<uint32_t, SignalStruct>  _eventHandlerMap;
};

// Shorthand for the Signal type that we use to store our handler references
template<typename DataType>
using EventHandlerSignal = Signal::Signal<void(const AnkiEvent<DataType>&)>;

// Shorthand for the function signature we accept as event handlers
template<typename DataType>
using SubscriberFunction = std::function<void(const AnkiEvent<DataType>&)>;
  

// Original templated AnkiEventMgr type (with a default type for the signal structure)
template<typename DataType, typename SignalStruct = EventHandlerSignal<DataType> >
class AnkiEventMgr : public AnkiEventMgrBase<DataType, SignalStruct>
{
public:
  using EventDataType = AnkiEvent<DataType>;

  // Broadcasts a given event to everyone that has subscribed to that event type
  void Broadcast(const EventDataType& event)
  {
    auto iter = this->_eventHandlerMap.find(event.GetType());
    if (iter != this->_eventHandlerMap.end())
    {
      iter->second.emit(event);
    }
  }

  // Allows subscribing to events by type with the passed in function
  Signal::SmartHandle Subscribe(const uint32_t type, SubscriberFunction<DataType> function)
  {
    return this->_eventHandlerMap[type].ScopedSubscribe(function);
  }
  
  void SubscribeForever(const uint32_t type, SubscriberFunction<DataType> function)
  {
    this->_eventHandlerMap[type].SubscribeForever(function);
  }
}; // class AnkiEventMgr



/**
 *      Specialization takes in another param: mailbox id. Mailbox allows us to listen for events coming from specific
 *      device, or going to specific device. For example, if you want to subscribe to "battery_status" message
 *      coming only from device "3".
 *
*/

// For mailboxes, our signal struct is going to be a map of mailbox ID to signal
template <typename DataType>
using MailboxSignalMap = std::unordered_map<uint32_t, EventHandlerSignal<DataType> >;
  
// Here's the partially specialized type's declaration/definition
template<typename DataType>
class AnkiEventMgr<DataType, MailboxSignalMap<DataType> > : public AnkiEventMgrBase<DataType, MailboxSignalMap<DataType> >
{
public:
  const uint32_t AnyMailboxId{65999};
  using EventDataType = AnkiEvent<DataType>;

  // Broadcasts a given event to everyone that has subscribed to that event type
  void Broadcast(const uint32_t mailbox, const EventDataType& event)
  {
    auto iter = this->_eventHandlerMap.find(event.GetType());
    if (iter != this->_eventHandlerMap.end())
    {
      if (mailbox == AnyMailboxId)
      {
        // deliver to all mailboxes
        for (auto& mapPair : iter->second) {
          mapPair.second.emit(event);
        }
      } else {
        // iter->second is a map: std::map<uint32_t, EventHandlerSignal >
        // search the inner map for correct mailbox
        auto innerIter = iter->second.find(mailbox);
        if (innerIter != iter->second.end()) {
          innerIter->second.emit(event);
        }
        // also search for AnyMailbox
        innerIter = iter->second.find(AnyMailboxId);
        if (innerIter != iter->second.end()) {
          innerIter->second.emit(event);
        }
      }
    }
  }

  // Allows subscribing to events by type with the passed in function
  Signal::SmartHandle Subscribe(const uint32_t mailbox, const uint32_t type, SubscriberFunction<DataType> function)
  {
    return this->_eventHandlerMap[type][mailbox].ScopedSubscribe(function);
  }
  
  void SubscribeForever(const uint32_t mailbox, const uint32_t type, SubscriberFunction<DataType> function)
  {
    this->_eventHandlerMap[type][mailbox].SubscribeForever(function);
  }
}; // class AnkiEventMgr (Mailbox specialization)

} // namespace Vector
} // namespace Anki

#endif //  __Anki_Cozmo_Basestation_Events_AnkiEventMgr_H__
