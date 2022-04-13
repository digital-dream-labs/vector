/*******************************************************************************************************************************
 *
 *  BaseStationEventDefinitions.h
 *  BaseStation
 *
 *  Created by Jarrod Hatfield on 1/17/14.
 *  Copyright (c) 2014 Anki. All rights reserved.
 *
 *  NOTE: This file should never need to be edited.
 *        All event creation is done in BaseStationEventDefinitions.def
 *
 *  Description:
 *  - Defines the structure of our in-game events.
 *  - I've macro-fied the creation of our events so that all you need to do in order to create a new event is to define it
 *    in BaseStationEventDefinitions.def.  My thinking is that if it's easy to create new events, people will be more likely
 *    to use them.
 *  - An event is basically just a container for event parameters, and has an associated type.  There should never be any need
 *    to debug these as they're just containers that get passed around ... this should alleviate any macro-usage concern.
 *
 *******************************************************************************************************************************/

#ifndef BASESTATION_BASESTATIONEVENTDEFINITIONS
#define BASESTATION_BASESTATIONEVENTDEFINITIONS


//==============================================================================================================================
// Allow our definition file to include header files
#define EVENT_INCLUDE_SECTION
#include "BaseStationEventDefinitions.def"
#undef EVENT_INCLUDE_SECTION


namespace Anki {
  namespace Vector {
    
class IBaseStationEventListener;


//==============================================================================================================================
// Allow our definition file to forward declare classes
#define EVENT_FORWARD_DECLARATION_SECTION
#include "BaseStationEventDefinitions.def"
#undef EVENT_FORWARD_DECLARATION_SECTION


//==============================================================================================================================
// Define our enum of event types
#define EVENT_BEGIN( name, args... ) BSETYPE_##name,
#define EVENT_ARG( ... )
#define EVENT_END( ... )

typedef enum
{
  #include "BaseStationEventDefinitions.def"
  
  BSETYPE_ALL,
} BaseStationEventType;

#undef EVENT_END
#undef EVENT_ARG
#undef EVENT_BEGIN


//==============================================================================================================================
class IBaseStationEventInterface
{
public:
  virtual ~IBaseStationEventInterface() { }
  virtual BaseStationEventType GetEventType() const = 0;
  
protected:
  IBaseStationEventInterface() { }
};


//==============================================================================================================================
// Crazy macro creation of events to make life easier for everybody.
// There should never be any need to debug these functions, and they're simple enough to figure out if anything goes wrong.
#define EVENT_BEGIN( name, args... ) \
  class BSE_##name : public IBaseStationEventInterface \
  { \
  public: \
    virtual BaseStationEventType GetEventType() const { return BSETYPE_##name; } \
    static void Register( IBaseStationEventListener* observer ); \
    static void Unregister( IBaseStationEventListener* observer ); \
    static void RaiseEvent( args );
  
#define EVENT_ARG( type, name ) \
  type name##_;
  
#define EVENT_END( name ) \
  };

#include "BaseStationEventDefinitions.def"

#undef EVENT_END
#undef EVENT_ARG
#undef EVENT_BEGIN

  } // namespace Vector
} // namespace Anki

#endif // BASESTATION_BASESTATIONEVENTDEFINITIONS