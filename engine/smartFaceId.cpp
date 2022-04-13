/**
 * File: smartFaceId.cpp
 *
 * Author: Brad Neuman
 * Created: 2017-04-13
 *
 * Description: Simple wrapper for faceID that automatically handles face deletion and id changes
 *
 * Copyright: Anki, Inc. 2017
 *
 **/

#include "engine/smartFaceId.h"

#include "engine/ankiEventUtil.h"
#include "engine/externalInterface/externalInterface.h"
#include "engine/robot.h"

#include "clad/externalInterface/messageEngineToGame.h"
#include "engine/cozmoContext.h"

namespace Anki {
namespace Vector {

using FaceID_t = Vision::FaceID_t;

class SmartFaceIDImpl {
public:

  SmartFaceIDImpl(IExternalInterface& externalInterface)
    : _id(Vision::UnknownFaceID)
    , _externalInterface(externalInterface)
  {

    // subscribe to events related to faces changed ids
    
    using namespace ExternalInterface;
    auto helper = MakeAnkiEventUtil(externalInterface, *this, _signalHandles);
    helper.SubscribeEngineToGame<MessageEngineToGameTag::RobotDeletedFace>();
    helper.SubscribeEngineToGame<MessageEngineToGameTag::RobotChangedObservedFaceID>();
  }

  SmartFaceIDImpl(const SmartFaceIDImpl& other)
    : SmartFaceIDImpl(other._externalInterface)
  {
    // this will create new subscriptions for the messages so we call the correct member functions
    _id = other._id;
  }
  
  SmartFaceIDImpl& operator=(const SmartFaceIDImpl& other)
  {
    // we are already subscribed to the messages, so no need to re-subscribe here
    _id = other._id;
    return *this;
  }

  template<typename T>
  void HandleMessage(const T& msg);

  FaceID_t _id;

  // we need to store this so that copy constructors can work (we need our own subscription to the events, so
  // that our member functions get called instead of functions on the (possibly deleted) _other_
  IExternalInterface& _externalInterface;

  std::vector<Signal::SmartHandle> _signalHandles;
  
};

template<>
void SmartFaceIDImpl::HandleMessage(const ExternalInterface::RobotChangedObservedFaceID& msg)
{
  if( _id == msg.oldID ) {
    _id = msg.newID;
  }
}

template<>
void SmartFaceIDImpl::HandleMessage(const ExternalInterface::RobotDeletedFace& msg)
{
  if( _id == msg.faceID ) {
    _id = Vision::UnknownFaceID;
  }
}


SmartFaceID::SmartFaceID()
{
  // impl is null, so will return unknown face id
}

SmartFaceID::~SmartFaceID()
{
}

SmartFaceID::SmartFaceID(const Robot& robot, Vision::FaceID_t faceID)
  : _impl( robot.HasExternalInterface() ? new SmartFaceIDImpl(*robot.GetContext()->GetExternalInterface()) : nullptr )
{
  if( _impl ) {
    _impl->_id = faceID;
  }
}

SmartFaceID::SmartFaceID(const SmartFaceID& other)
  : _impl(other._impl ? new SmartFaceIDImpl(*other._impl) : nullptr)
{
}

SmartFaceID::SmartFaceID(SmartFaceID&& other)
  : _impl(std::move(other._impl))
{
  // Move constructor transfers ownership of the impl, no need to do anything else
}

SmartFaceID& SmartFaceID::operator=(const SmartFaceID& other)
{
  if( other._impl ) {
    // point to a new copy of others impl
    _impl.reset(new SmartFaceIDImpl(*other._impl));
  }

  return *this;
}

void SmartFaceID::Reset(const Robot& robot, Vision::FaceID_t faceID)
{
  if( _impl ) {
    _impl->_id = faceID;
  }
  else if( robot.HasExternalInterface() ){
    _impl.reset(new SmartFaceIDImpl(*robot.GetContext()->GetExternalInterface()));
    _impl->_id = faceID;
  }
}

void SmartFaceID::Reset()
{
  if( _impl ) {
    _impl->_id = Vision::UnknownFaceID;
  }
}

bool SmartFaceID::IsValid() const
{
  const bool hasImplAndHasValidID = _impl && ( _impl->_id != Vision::UnknownFaceID );
  return hasImplAndHasValidID;
}

Vision::FaceID_t SmartFaceID::GetID() const
{
  const Vision::FaceID_t idOrInvalid = _impl ? _impl->_id : Vision::UnknownFaceID;
  return idOrInvalid;
}

bool SmartFaceID::MatchesFaceID(Vision::FaceID_t faceID) const
{
  const bool valid = IsValid();
  const bool bothUnknown = !valid && ( faceID == Vision::UnknownFaceID );
  return bothUnknown || ( valid && _impl->_id == faceID );
}

std::string SmartFaceID::GetDebugStr() const
{  
  return IsValid() ? std::to_string(_impl->_id) : "<unknown>";
}

bool SmartFaceID::operator==(const SmartFaceID& other) const
{
  const bool thisValid = IsValid();
  const bool otherValid = other.IsValid();

  if(thisValid && otherValid) {
    return _impl->_id == other._impl->_id;
  }
  else {
    // internal ids don't match, so only equal if both are invalid
    return !thisValid && !otherValid;
  }
}

}
}
