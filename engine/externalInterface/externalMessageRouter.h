/**
* File: externalMessageRouter.h
*
* Author: ross
* Created: may 31 2018
*
* Description: Templates to automatically wrap messages included in the MessageEngineToGame union
*              and the GatewayWrapper protobuf oneof (union) based on external requirements and
*              event organization (the hierarchy in clad and proto files)
 *             TODO: remove clad portions once messages are converted
*
* Copyright: Anki, Inc. 2018
*/

#ifndef __Engine_ExternalInterface_ExternalMessageRouter_H__
#define __Engine_ExternalInterface_ExternalMessageRouter_H__

#include "clad/externalInterface/messageEngineToGame.h"
#include "proto/external_interface/shared.pb.h"
#include "util/helpers/templateHelpers.h"
#include <type_traits>

namespace Anki {
namespace Vector {

class ExternalMessageRouter
{
public:

  // helper that provides type Ret if B is true
  template <bool B, class Ret>
  using ReturnIf = typename std::enable_if<B, Ret>::type;

  template <typename T, typename From>
  using CanConstruct = Anki::Util::is_explicitly_constructible<T, From*>;

  // -------------------------------------------------------------------------------------------------
  // Outbound Proto Messages:
  // If your message is a response to a request, call WrapResponse() on your allocated message pointer.
  // If your message is NOT a response to a request, call Wrap() on your allocated message pointer
  // Eventually, gateway changes should render this distinction obsolete
  // -------------------------------------------------------------------------------------------------

  using GatewayWrapper = external_interface::GatewayWrapper;
  // which contains
  using Event = external_interface::Event;
  // which contains all of the following:
  using Status = external_interface::Status;
  using Onboarding = external_interface::Onboarding;
  using WakeWord = external_interface::WakeWord;
  using AttentionTransfer = external_interface::AttentionTransfer;

  //optional connId allows for identifying source of response
  template<typename T>
  inline static external_interface::GatewayWrapper WrapResponse( T* message, uint64_t connId = 0)
  {
    GatewayWrapper wrapper{ message };
    wrapper.set_connection_id(connId);
    return wrapper;
  }

  template <typename T>
  inline static ReturnIf<CanConstruct<Event,T>::value, GatewayWrapper>
  /* GatewayWrapper */ Wrap( T* message )
  {
    Event* event = new Event{ message };
    return WrapResponse( event );
  }

  // custom Wrap() function for timestamped status
  template <typename T>
  inline static ReturnIf<CanConstruct<Status,T>::value, GatewayWrapper>
  Wrap( T* message )
  {
    auto* statusMsg = new Status{ message };
    auto* timeStampedStatusMsg = new external_interface::TimeStampedStatus;
    timeStampedStatusMsg->set_allocated_status( statusMsg );
    timeStampedStatusMsg->set_timestamp_utc( GetTimestampUTC() );
    return Wrap( timeStampedStatusMsg );
  }

  // macro to generate Wrap() methods based on Event types listed in the proto file
  #define MAKE_MESSAGE_WRAPPER(Type) \
    template <typename T> \
    inline static ReturnIf<CanConstruct<Type,T>::value, GatewayWrapper> \
    /* GatewayWrapper */ Wrap( T* message ) \
    { \
      auto* msgPtr = new Type{ message }; \
      return Wrap( msgPtr ); \
    }
  MAKE_MESSAGE_WRAPPER(Onboarding)
  MAKE_MESSAGE_WRAPPER(WakeWord)
  MAKE_MESSAGE_WRAPPER(AttentionTransfer)

  // -------------------------------------------------------------------------------------------------
  // Outbound CLAD Messages
  // -------------------------------------------------------------------------------------------------

  // the hierarchy, based on messageEngineToGame.clad:
  using MessageEngineToGame = ExternalInterface::MessageEngineToGame;
  // which contains
  using CLADEvent = ExternalInterface::Event;

  // Is an Event (not part of a request/response pair)
  template <typename T>
  using CanBeCLADEvent = Anki::Util::is_explicitly_constructible<CLADEvent, T>;

  template <typename T>
  using CanBeEngineToGame = Anki::Util::is_explicitly_constructible<MessageEngineToGame, T>;


  // in case this is used with a MessageEngineToGame
  inline static MessageEngineToGame Wrap(MessageEngineToGame&& message)
  {
    return std::move(message);
  }

  template <typename T>
  inline static ReturnIf<!CanBeCLADEvent<T>::value && CanBeEngineToGame<T>::value, MessageEngineToGame>
  /* MessageEngineToGame */ Wrap(T&& message)
  {
    MessageEngineToGame engineToGame{ std::forward<T>(message) };
    return engineToGame;
  }

  template <typename T>
  inline static ReturnIf<CanBeCLADEvent<T>::value, MessageEngineToGame>
  /* MessageEngineToGame */ Wrap(T&& message)
  {
    CLADEvent event{ std::forward<T>(message) };
    return Wrap( std::move(event) );
  }

private:

  static uint32_t GetTimestampUTC();

};

} // end namespace Vector
} // end namespace Anki

#endif //__Engine_ExternalInterface_ExternalMessageRouter_H__
