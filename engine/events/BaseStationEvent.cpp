/*******************************************************************************************************************************
 *
 *  BaseStationEvents.cpp
 *  BaseStation
 *
 *  Created by Jarrod Hatfield on 1/17/14.
 *  Copyright (c) 2014 Anki. All rights reserved.
 *
 *******************************************************************************************************************************/

#include "engine/events/BaseStationEvent.h"
#include <algorithm>
#include <assert.h>

namespace Anki {
  namespace Vector {


BaseStationEventDispatcher* BaseStationEventDispatcher::instance_ = NULL;

//------------------------------------------------------------------------------------------------------------------------------
void IBaseStationEventListener::RegisterForAllEvents( void )
{
  BaseStationEventDispatcher::Instance().RegisterEventListener( BSETYPE_ALL, this);
}

//------------------------------------------------------------------------------------------------------------------------------
void IBaseStationEventListener::UnregisterForAllEvents( void )
{
  BaseStationEventDispatcher::Instance().UnregisterEventListener( BSETYPE_ALL, this);
}

//------------------------------------------------------------------------------------------------------------------------------
BaseStationEventDispatcher::BaseStationEventDispatcher( void )
{
  
}

//------------------------------------------------------------------------------------------------------------------------------
BaseStationEventDispatcher::~BaseStationEventDispatcher( void )
{
  
}

//------------------------------------------------------------------------------------------------------------------------------
void BaseStationEventDispatcher::Init( void )
{
  // We're beginning a new basestation here ...
}

//------------------------------------------------------------------------------------------------------------------------------
void BaseStationEventDispatcher::RemoveInstance( void )
{
  if ( instance_ != NULL )
  {
    delete instance_;
    instance_ = NULL;
  }
}

//------------------------------------------------------------------------------------------------------------------------------
void BaseStationEventDispatcher::RegisterEventListener( BaseStationEventType type, IBaseStationEventListener* observer )
{
  assert( ( type >= 0 ) && ( type <= BSETYPE_ALL ) );
  if ( type != BSETYPE_ALL )
  {
    ObserverList& list = observers_[type];
    ObserverList::iterator match = std::find( list.begin(), list.end(), observer );
    if ( match == list.end() )
    {
      list.push_back( observer );
    }
  }
  else
  {
    // Add the observer to each event list.
    for ( int i = 0; i < BSETYPE_ALL; ++i )
    {
      RegisterEventListener( (BaseStationEventType)i, observer );
    }
  }
}
  
//------------------------------------------------------------------------------------------------------------------------------
void BaseStationEventDispatcher::UnregisterEventListener( BaseStationEventType type, IBaseStationEventListener* observer )
{
  assert( ( type >= 0 ) && ( type <= BSETYPE_ALL ) );
  if ( type != BSETYPE_ALL )
  {
    ObserverList& list = observers_[type];
    ObserverList::iterator match = std::find( list.begin(), list.end(), observer );
    if ( match != list.end() )
    {
      list.erase( match );
    }
  }
  else
  {
    // Remove the observer from each event list.
    for ( int i = 0; i < BSETYPE_ALL; ++i )
    {
      UnregisterEventListener( (BaseStationEventType)i, observer );
    }
  }
}

//------------------------------------------------------------------------------------------------------------------------------
void BaseStationEventDispatcher::EventRaised( IBaseStationEventInterface* event, BaseStationEventDelivery delivery )
{
  NotifyEventListeners( event );
  delete event;
}
  
//------------------------------------------------------------------------------------------------------------------------------
void BaseStationEventDispatcher::NotifyEventListeners( IBaseStationEventInterface* event )
{
  const BaseStationEventType type = event->GetEventType();
  assert( ( type >= 0 ) && ( type < BSETYPE_ALL ) );
  
  ObserverList& list = observers_[type];
  ObserverList::const_iterator it = list.begin();
  ObserverList::const_iterator end = list.end();
  
  for( ; it != end; ++it )
  {
    (*it)->OnEventRaised( event );
  }
}

  } // namespace Vector
} // namespace Anki
