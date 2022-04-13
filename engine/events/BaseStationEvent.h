/*******************************************************************************************************************************
 *
 *  BaseStationEvents.h
 *  BaseStation
 *
 *  Created by Jarrod Hatfield on 1/17/14.
 *  Copyright (c) 2014 Anki. All rights reserved.
 *
 *  Description:
 *  - In-game event system for raising/handling events
 *
 *******************************************************************************************************************************/

#ifndef BASESTATION_BASESTATIONEVENTS
#define BASESTATION_BASESTATIONEVENTS

#include "engine/events/BaseStationEventDefinitions.h"
//#include <util/helpers/includeIostream.h>
#include <vector>

namespace Anki {
  namespace Vector {

//==============================================================================================================================
typedef enum
{
  BSED_IMMEDIATE,
} BaseStationEventDelivery;


//==============================================================================================================================
// Each class that wants to listen for events needs to implement this simple interface.
class IBaseStationEventListener
{
public:

  // called by event dispatcher when event is raised
  virtual void OnEventRaised( const IBaseStationEventInterface* event ) = 0;
  
  void RegisterForAllEvents( void );
  void UnregisterForAllEvents( void );
};


//==============================================================================================================================
// This class handles the collecting and dispatcing of events.
// It also handles the registration of event listeners.
class BaseStationEventDispatcher
{
  //----------------------------------------------------------------------------------------------------------------------------
  // Construct/Destruct
public:
  BaseStationEventDispatcher( void );
  ~BaseStationEventDispatcher( void );
  void Init( void );
  
  //----------------------------------------------------------------------------------------------------------------------------
  // Event Handling
public:
  void RegisterEventListener( BaseStationEventType type, IBaseStationEventListener* observer );
  void UnregisterEventListener( BaseStationEventType type, IBaseStationEventListener* observer );
  void EventRaised( IBaseStationEventInterface* event, BaseStationEventDelivery delivery );
  
protected:
  void NotifyEventListeners( IBaseStationEventInterface* event );
  
  //----------------------------------------------------------------------------------------------------------------------------
  // Singleton Access
public:
  static void RemoveInstance( void );
  static BaseStationEventDispatcher& Instance()
  {
    if ( instance_ == NULL )
    {
      instance_ = new BaseStationEventDispatcher();
    }
    
    return *instance_;
  }
  
  //----------------------------------------------------------------------------------------------------------------------------
  // Data Members
private:
  typedef std::vector<IBaseStationEventListener*> ObserverList;
  ObserverList observers_[BSETYPE_ALL];
  
  std::vector<IBaseStationEventInterface*> queuedEvents_;
  
  static BaseStationEventDispatcher* instance_;
};
  
  } // namespace Vector
} // namespace Anki
#endif // BASESTATION_BASESTATIONEVENTS
