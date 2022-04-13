/**
 * File: protoCladInterpreter.h
 *
 * Author: Ron Barry
 * Date:   11/16/2018
 *
 * Description: Determine which proto messages need to be converted to
 *              Clad before being dispatched to their final destinations.
 *              (The gateway no longer does this work.)
 *
 * Copyright: Anki, Inc. 2018
 **/

#ifndef _ENGINE_COMMS_PROTOCLADINTERPRETER_H__
#define _ENGINE_COMMS_PROTOCLADINTERPRETER_H__

#include "engine/cozmoContext.h"
#include "engine/externalInterface/externalInterface.h"

#include "clad/externalInterface/messageGameToEngine.h"
#include "clad/gateway/messageRobotToExternalTag.h"
#include "proto/external_interface/shared.pb.h"

namespace Anki {
namespace Vector {


class ProtoCladInterpreter {
public:
    static bool Redirect(const external_interface::GatewayWrapper& message, CozmoContext* cozmo_context);
    static bool Redirect(const ExternalInterface::MessageGameToEngine& message, CozmoContext* cozmo_context);
    static bool Redirect(const ExternalInterface::MessageEngineToGame& message, CozmoContext* cozmo_context);

private:
  ProtoCladInterpreter() {}

  //
  // Proto-to-Clad interpreters
  //
  static void ProtoDriveWheelsRequestToClad(
      const external_interface::GatewayWrapper& proto_message,
      ExternalInterface::MessageGameToEngine& clad_message);

  static void ProtoPlayAnimationRequestToClad(
      const external_interface::GatewayWrapper& proto_message,
      ExternalInterface::MessageGameToEngine& clad_message);

  static void ProtoCancelActionByIdTagRequestToClad(
      const external_interface::GatewayWrapper& proto_message,
      ExternalInterface::MessageGameToEngine& clad_message);

  static void ProtoListAnimationsRequestToClad(
      const external_interface::GatewayWrapper& proto_message,
      ExternalInterface::MessageGameToEngine& clad_message);

  static void ProtoPlayAnimationTriggerRequestToClad(
      const external_interface::GatewayWrapper& proto_message,
      ExternalInterface::MessageGameToEngine& clad_message);

  static void ProtoStopAllMotorsRequestToClad(
      const external_interface::GatewayWrapper& proto_message,
      ExternalInterface::MessageGameToEngine& clad_message);

  static void ProtoSetFaceToEnrollRequestToClad(
      const external_interface::GatewayWrapper& proto_message,
      ExternalInterface::MessageGameToEngine& clad_message);

  static void ProtoCameraConfigRequestToClad(
      const external_interface::GatewayWrapper& proto_message,
      ExternalInterface::MessageGameToEngine& clad_message);

  //
  // Clad-to-Proto interpreters
  //
  static void CladDriveWheelsToProto(
      const ExternalInterface::MessageGameToEngine& clad_message,
      external_interface::GatewayWrapper& proto_message);

  static void CladPlayAnimationToProto(
      const ExternalInterface::MessageGameToEngine& clad_message,
      external_interface::GatewayWrapper& proto_message);

  static void CladCancelActionByIdTagToProto(
      const ExternalInterface::MessageGameToEngine& clad_message, 
      external_interface::GatewayWrapper& proto_message);

  static void CladAnimationAvailableToProto(
      const ExternalInterface::MessageEngineToGame& clad_message, 
      external_interface::GatewayWrapper& proto_message);

  static void CladStopAllMotorsToProto(
      const ExternalInterface::MessageGameToEngine& clad_message, 
      external_interface::GatewayWrapper& proto_message);

  static void CladSetFaceToEnrollToProto(
      const ExternalInterface::MessageGameToEngine& clad_message,
      external_interface::GatewayWrapper& proto_message);

  static void CladEndOfMessageToProto(
      const ExternalInterface::MessageEngineToGame& clad_message, 
      external_interface::GatewayWrapper& proto_message);

  static void CladPerRobotSettingsToProto(
      const ExternalInterface::MessageEngineToGame& clad_message, 
      external_interface::GatewayWrapper& proto_message);

  static void CladCurrentCameraParamsToProto(
      const ExternalInterface::MessageEngineToGame& clad_message, 
      external_interface::GatewayWrapper& proto_message);
};

} // namespace Vector
} // namespace Anki

#endif // _ENGINE_COMMS_PROTOCLADINTERPRETER_H__
