/**
* File: cladProtoTypeTranslator.h
*
* Author: ross
* Created: jun 18 2018
*
* Description: Guards and helpers make sure translation between clad and proto enum types is safe,
*              in case the underlying values or field numbers change.
*
* Copyright: Anki, Inc. 2018
*/

#ifndef __Engine_ExternalInterface_CladProtoTypeTranslator_H__
#define __Engine_ExternalInterface_CladProtoTypeTranslator_H__


#include "proto/external_interface/messages.pb.h"
#include "clad/externalInterface/messageEngineToGame.h"
#include "clad/externalInterface/messageGameToEngine.h"
#include "clad/types/alexaTypes.h"
#include "clad/types/behaviorComponent/attentionTransferTypes.h"
#include "clad/types/unexpectedMovementTypes.h"
#include "clad/types/onboardingPhase.h"
#include "clad/types/onboardingPhaseState.h"


namespace Anki {
namespace Vector {

namespace CladProtoTypeTranslator {

  constexpr external_interface::OnboardingPhase ToProtoEnum( OnboardingPhase value ){
    return static_cast<external_interface::OnboardingPhase>( static_cast<std::underlying_type_t<OnboardingPhase>>(value) );
  }

  constexpr OnboardingPhase ToCladEnum( external_interface::OnboardingPhase value ){
    return static_cast<OnboardingPhase>( static_cast<std::underlying_type_t<external_interface::OnboardingPhase>>(value) );
  } 

  constexpr external_interface::OnboardingPhaseState ToProtoEnum( OnboardingPhaseState value ){
    return static_cast<external_interface::OnboardingPhaseState>( static_cast<std::underlying_type_t<OnboardingPhaseState>>(value) );
  }

  constexpr OnboardingPhaseState ToCladEnum( external_interface::OnboardingPhaseState value ){
    return static_cast<OnboardingPhaseState>( static_cast<std::underlying_type_t<external_interface::OnboardingPhaseState>>(value) );
  } 

  constexpr external_interface::OnboardingStages ToProtoEnum( OnboardingStages value ){
    return static_cast<external_interface::OnboardingStages>( static_cast<std::underlying_type_t<OnboardingStages>>(value) );
  }

  constexpr external_interface::AttentionTransferReason ToProtoEnum( AttentionTransferReason value ){
    return static_cast<external_interface::AttentionTransferReason>( static_cast<std::underlying_type_t<AttentionTransferReason>>(value) );
  }
  
  constexpr external_interface::FaceEnrollmentResult ToProtoEnum( FaceEnrollmentResult value ){
    return static_cast<external_interface::FaceEnrollmentResult>( static_cast<std::underlying_type_t<FaceEnrollmentResult>>(value) );
  }
  
  constexpr external_interface::AlexaAuthState ToProtoEnum( AlexaAuthState value ){
    return static_cast<external_interface::AlexaAuthState>( static_cast<std::underlying_type_t<AlexaAuthState>>(value) );
  }

  constexpr external_interface::UnexpectedMovementSide ToProtoEnum( UnexpectedMovementSide value ){
    return static_cast<external_interface::UnexpectedMovementSide>( static_cast<std::underlying_type_t<UnexpectedMovementSide>>(value) );
  }

  constexpr external_interface::UnexpectedMovementType ToProtoEnum( UnexpectedMovementType value ){
    return static_cast<external_interface::UnexpectedMovementType>( static_cast<std::underlying_type_t<UnexpectedMovementType>>(value) );
  }

  #define CLAD_PROTO_COMPARE_ASSERT(T,V) static_assert(ToProtoEnum(T::V) == external_interface::T::V, "Invalid cast " #T "::" #V )
  #define CLAD_PROTO_COMPARE_ASSERT2(T,V,U) static_assert(ToProtoEnum(T::V) == external_interface::T::U, "Invalid cast " #T "::" #V " to " #T "::" #U )
  
  CLAD_PROTO_COMPARE_ASSERT(OnboardingStages, NotStarted);
  CLAD_PROTO_COMPARE_ASSERT(OnboardingStages, TimedOut);
  CLAD_PROTO_COMPARE_ASSERT(OnboardingStages, Complete);
  CLAD_PROTO_COMPARE_ASSERT(OnboardingStages, DevDoNothing);

  CLAD_PROTO_COMPARE_ASSERT(OnboardingPhase, Default);
  CLAD_PROTO_COMPARE_ASSERT(OnboardingPhase, LookAtPhone);
  CLAD_PROTO_COMPARE_ASSERT(OnboardingPhase, WakeUp);
  CLAD_PROTO_COMPARE_ASSERT(OnboardingPhase, LookAtUser);
  CLAD_PROTO_COMPARE_ASSERT(OnboardingPhase, TeachWakeWord);
  CLAD_PROTO_COMPARE_ASSERT(OnboardingPhase, TeachComeHere);
  CLAD_PROTO_COMPARE_ASSERT(OnboardingPhase, TeachMeetVictor);

  CLAD_PROTO_COMPARE_ASSERT(OnboardingPhaseState, PhaseInvalid);
  CLAD_PROTO_COMPARE_ASSERT(OnboardingPhaseState, PhasePending);
  CLAD_PROTO_COMPARE_ASSERT(OnboardingPhaseState, PhaseInProgress);
  CLAD_PROTO_COMPARE_ASSERT(OnboardingPhaseState, PhaseComplete);

  CLAD_PROTO_COMPARE_ASSERT(AttentionTransferReason, Invalid);
  CLAD_PROTO_COMPARE_ASSERT(AttentionTransferReason, NoCloudConnection);
  CLAD_PROTO_COMPARE_ASSERT(AttentionTransferReason, NoWifi);
  CLAD_PROTO_COMPARE_ASSERT(AttentionTransferReason, UnmatchedIntent);
  
  CLAD_PROTO_COMPARE_ASSERT2(FaceEnrollmentResult, Success, SUCCESS);
  CLAD_PROTO_COMPARE_ASSERT2(FaceEnrollmentResult, SawWrongFace, SAW_WRONG_FACE);
  CLAD_PROTO_COMPARE_ASSERT2(FaceEnrollmentResult, SawMultipleFaces, SAW_MULTIPLE_FACES);
  CLAD_PROTO_COMPARE_ASSERT2(FaceEnrollmentResult, TimedOut, TIMED_OUT);
  CLAD_PROTO_COMPARE_ASSERT2(FaceEnrollmentResult, SaveFailed, SAVE_FAILED);
  CLAD_PROTO_COMPARE_ASSERT2(FaceEnrollmentResult, Incomplete, INCOMPLETE);
  CLAD_PROTO_COMPARE_ASSERT2(FaceEnrollmentResult, Cancelled, CANCELLED);
  CLAD_PROTO_COMPARE_ASSERT2(FaceEnrollmentResult, NameInUse, NAME_IN_USE);
  CLAD_PROTO_COMPARE_ASSERT2(FaceEnrollmentResult, NamedStorageFull, NAMED_STORAGE_FULL);
  CLAD_PROTO_COMPARE_ASSERT2(FaceEnrollmentResult, UnknownFailure, UNKNOWN_FAILURE);
  
  CLAD_PROTO_COMPARE_ASSERT2(AlexaAuthState, Invalid, ALEXA_AUTH_INVALID);
  CLAD_PROTO_COMPARE_ASSERT2(AlexaAuthState, Uninitialized, ALEXA_AUTH_UNINITIALIZED);
  CLAD_PROTO_COMPARE_ASSERT2(AlexaAuthState, RequestingAuth, ALEXA_AUTH_REQUESTING_AUTH);
  CLAD_PROTO_COMPARE_ASSERT2(AlexaAuthState, WaitingForCode, ALEXA_AUTH_WAITING_FOR_CODE);
  CLAD_PROTO_COMPARE_ASSERT2(AlexaAuthState, Authorized, ALEXA_AUTH_AUTHORIZED);

  CLAD_PROTO_COMPARE_ASSERT(UnexpectedMovementSide, UNKNOWN);
  CLAD_PROTO_COMPARE_ASSERT(UnexpectedMovementSide, FRONT);
  CLAD_PROTO_COMPARE_ASSERT(UnexpectedMovementSide, BACK);
  CLAD_PROTO_COMPARE_ASSERT(UnexpectedMovementSide, LEFT);
  CLAD_PROTO_COMPARE_ASSERT(UnexpectedMovementSide, RIGHT);
  
  CLAD_PROTO_COMPARE_ASSERT(UnexpectedMovementType, TURNED_BUT_STOPPED);
  CLAD_PROTO_COMPARE_ASSERT(UnexpectedMovementType, TURNED_IN_SAME_DIRECTION);
  CLAD_PROTO_COMPARE_ASSERT(UnexpectedMovementType, TURNED_IN_OPPOSITE_DIRECTION);
  CLAD_PROTO_COMPARE_ASSERT(UnexpectedMovementType, ROTATING_WITHOUT_MOTORS);

}


} // end namespace Vector
} // end namespace Anki

#endif //__Engine_ExternalInterface_CladProtoTypeTranslator_H__
