/**
* File: robotComponents_impl.cpp
*
* Author: Kevin M. Karol
* Created: 2/12/18
*
* Description: Template specializations mapping classes to enums
*
* Copyright: Anki, Inc. 2018
*
**/

#include "engine/robotComponents_fwd.h"
#include "util/entityComponent/componentTypeEnumMap.h"

namespace Anki {
namespace Vector {

// Forward declarations
namespace Audio {
class EngineRobotAudioClient;
}

class AppCubeConnectionSubscriber;
class ContextWrapper;
class BlockWorld;
class FaceWorld;
class LocaleComponent;
class PetWorld;
class PublicStateBroadcaster;
class PathComponent;
class DrivingAnimationHandler;
class ActionList;
class MovementComponent;
class VisionComponent;
class VisionScheduleMediator;
class MapComponent;
class NVStorageComponent;
class AIComponent;
class CubeLightComponent;
class BackpackLightComponent;
class CubeAccelComponent;
class CubeBatteryComponent;
class CubeCommsComponent;
class CubeConnectionCoordinator;
class CubeInteractionTracker;
class RobotGyroDriftDetector;
class DockingComponent;
class CarryingComponent;
class CliffSensorComponent;
class ProxSensorComponent;
class ImuComponent;
class RangeSensorComponent;
class TouchSensorComponent;
class AnimationComponent;
class RobotStateHistory;
class MoodManager;
class StimulationFaceDisplay;
class BlockTapFilterComponent;
class RobotToEngineImplMessaging;
class MicComponent;
class BatteryComponent;
class FullRobotPose;
class DataAccessorComponent;
class BeatDetectorComponent;
class HabitatDetectorComponent;
class TextToSpeechCoordinator;
class SDKComponent;
class PhotographyManager;
class RobotHealthReporter;
class RobotStatsTracker;
class SettingsCommManager;
class SettingsManager;
class VariableSnapshotComponent;
class PowerStateManager;
class JdocsManager;
class LocaleComponent;
class RobotExternalRequestComponent;
class AccountSettingsManager;
class UserEntitlementsManager;
class RangeSensorComponent;
} // namespace Vector

// Template specializations mapping enums from the _fwd.h file to the class forward declarations above
LINK_COMPONENT_TYPE_TO_ENUM(AppCubeConnectionSubscriber,   RobotComponentID, AppCubeConnectionSubscriber)
LINK_COMPONENT_TYPE_TO_ENUM(ContextWrapper,                RobotComponentID, CozmoContextWrapper)
LINK_COMPONENT_TYPE_TO_ENUM(BlockWorld,                    RobotComponentID, BlockWorld)
LINK_COMPONENT_TYPE_TO_ENUM(FaceWorld,                     RobotComponentID, FaceWorld)
LINK_COMPONENT_TYPE_TO_ENUM(PetWorld,                      RobotComponentID, PetWorld)
LINK_COMPONENT_TYPE_TO_ENUM(PublicStateBroadcaster,        RobotComponentID, PublicStateBroadcaster)
LINK_COMPONENT_TYPE_TO_ENUM(Audio::EngineRobotAudioClient, RobotComponentID, EngineAudioClient)
LINK_COMPONENT_TYPE_TO_ENUM(PathComponent,                 RobotComponentID, PathPlanning)
LINK_COMPONENT_TYPE_TO_ENUM(DrivingAnimationHandler,       RobotComponentID, DrivingAnimationHandler)
LINK_COMPONENT_TYPE_TO_ENUM(ActionList,                    RobotComponentID, ActionList)
LINK_COMPONENT_TYPE_TO_ENUM(MovementComponent,             RobotComponentID, Movement)
LINK_COMPONENT_TYPE_TO_ENUM(VisionComponent,               RobotComponentID, Vision)
LINK_COMPONENT_TYPE_TO_ENUM(VisionScheduleMediator,        RobotComponentID, VisionScheduleMediator)
LINK_COMPONENT_TYPE_TO_ENUM(MapComponent,                  RobotComponentID, Map)
LINK_COMPONENT_TYPE_TO_ENUM(NVStorageComponent,            RobotComponentID, NVStorage)
LINK_COMPONENT_TYPE_TO_ENUM(AIComponent,                   RobotComponentID, AIComponent)
LINK_COMPONENT_TYPE_TO_ENUM(CubeLightComponent,            RobotComponentID, CubeLights)
LINK_COMPONENT_TYPE_TO_ENUM(BackpackLightComponent,        RobotComponentID, BackpackLights)
LINK_COMPONENT_TYPE_TO_ENUM(CubeAccelComponent,            RobotComponentID, CubeAccel)
LINK_COMPONENT_TYPE_TO_ENUM(CubeBatteryComponent,          RobotComponentID, CubeBattery)
LINK_COMPONENT_TYPE_TO_ENUM(CubeCommsComponent,            RobotComponentID, CubeComms)
LINK_COMPONENT_TYPE_TO_ENUM(CubeConnectionCoordinator,     RobotComponentID, CubeConnectionCoordinator)
LINK_COMPONENT_TYPE_TO_ENUM(CubeInteractionTracker,        RobotComponentID, CubeInteractionTracker)
LINK_COMPONENT_TYPE_TO_ENUM(RobotGyroDriftDetector,        RobotComponentID, GyroDriftDetector)
LINK_COMPONENT_TYPE_TO_ENUM(HabitatDetectorComponent,      RobotComponentID, HabitatDetector)
LINK_COMPONENT_TYPE_TO_ENUM(DockingComponent,              RobotComponentID, Docking)
LINK_COMPONENT_TYPE_TO_ENUM(CarryingComponent,             RobotComponentID, Carrying)
LINK_COMPONENT_TYPE_TO_ENUM(CliffSensorComponent,          RobotComponentID, CliffSensor)
LINK_COMPONENT_TYPE_TO_ENUM(ProxSensorComponent,           RobotComponentID, ProxSensor)
LINK_COMPONENT_TYPE_TO_ENUM(ImuComponent,                  RobotComponentID, ImuSensor)
LINK_COMPONENT_TYPE_TO_ENUM(TouchSensorComponent,          RobotComponentID, TouchSensor)
LINK_COMPONENT_TYPE_TO_ENUM(AnimationComponent,            RobotComponentID, Animation)
LINK_COMPONENT_TYPE_TO_ENUM(RobotStateHistory,             RobotComponentID, StateHistory)
LINK_COMPONENT_TYPE_TO_ENUM(MoodManager,                   RobotComponentID, MoodManager)
LINK_COMPONENT_TYPE_TO_ENUM(StimulationFaceDisplay,        RobotComponentID, StimulationFaceDisplay)
LINK_COMPONENT_TYPE_TO_ENUM(BlockTapFilterComponent,       RobotComponentID, BlockTapFilter)
LINK_COMPONENT_TYPE_TO_ENUM(RobotToEngineImplMessaging,    RobotComponentID, RobotToEngineImplMessaging)
LINK_COMPONENT_TYPE_TO_ENUM(MicComponent,                  RobotComponentID, MicComponent)
LINK_COMPONENT_TYPE_TO_ENUM(BatteryComponent,              RobotComponentID, Battery)
LINK_COMPONENT_TYPE_TO_ENUM(FullRobotPose,                 RobotComponentID, FullRobotPose)
LINK_COMPONENT_TYPE_TO_ENUM(DataAccessorComponent,         RobotComponentID, DataAccessor)
LINK_COMPONENT_TYPE_TO_ENUM(BeatDetectorComponent,         RobotComponentID, BeatDetector)
LINK_COMPONENT_TYPE_TO_ENUM(TextToSpeechCoordinator,       RobotComponentID, TextToSpeechCoordinator)
LINK_COMPONENT_TYPE_TO_ENUM(SDKComponent,                  RobotComponentID, SDK)
LINK_COMPONENT_TYPE_TO_ENUM(PhotographyManager,            RobotComponentID, PhotographyManager)
LINK_COMPONENT_TYPE_TO_ENUM(SettingsCommManager,           RobotComponentID, SettingsCommManager)
LINK_COMPONENT_TYPE_TO_ENUM(SettingsManager,               RobotComponentID, SettingsManager)
LINK_COMPONENT_TYPE_TO_ENUM(RobotHealthReporter,           RobotComponentID, RobotHealthReporter)
LINK_COMPONENT_TYPE_TO_ENUM(RobotStatsTracker,             RobotComponentID, RobotStatsTracker)
LINK_COMPONENT_TYPE_TO_ENUM(VariableSnapshotComponent,     RobotComponentID, VariableSnapshotComponent)
LINK_COMPONENT_TYPE_TO_ENUM(PowerStateManager,             RobotComponentID, PowerStateManager)
LINK_COMPONENT_TYPE_TO_ENUM(JdocsManager,                  RobotComponentID, JdocsManager)
LINK_COMPONENT_TYPE_TO_ENUM(LocaleComponent,               RobotComponentID, LocaleComponent)
LINK_COMPONENT_TYPE_TO_ENUM(RobotExternalRequestComponent, RobotComponentID, RobotExternalRequestComponent)
LINK_COMPONENT_TYPE_TO_ENUM(AccountSettingsManager,        RobotComponentID, AccountSettingsManager)
LINK_COMPONENT_TYPE_TO_ENUM(UserEntitlementsManager,       RobotComponentID, UserEntitlementsManager)
LINK_COMPONENT_TYPE_TO_ENUM(RangeSensorComponent,          RobotComponentID, RangeSensor)


// Translate entity into string
template<>
std::string GetEntityNameForEnumType<Vector::RobotComponentID>(){ return "RobotComponents"; }

template<>
std::string GetComponentStringForID<Vector::RobotComponentID>(Vector::RobotComponentID enumID)
{
  // Handy macro tricks: Use #param to stringify macro parameters
  #define CASE(id) case Vector::RobotComponentID::id: { return #id ; }

  // Note that list must stay in alphabetical order!
  switch (enumID) {
    CASE(AccountSettingsManager)
    CASE(ActionList)
    CASE(AIComponent)
    CASE(Animation)
    CASE(AppCubeConnectionSubscriber)
    CASE(BackpackLights)
    CASE(Battery)
    CASE(BeatDetector)
    CASE(BlockTapFilter)
    CASE(BlockWorld)
    CASE(Carrying)
    CASE(CliffSensor)
    CASE(CozmoContextWrapper)
    CASE(CubeAccel)
    CASE(CubeBattery)
    CASE(CubeComms)
    CASE(CubeConnectionCoordinator)
    CASE(CubeInteractionTracker)
    CASE(CubeLights)
    CASE(DataAccessor)
    CASE(Docking)
    CASE(DrivingAnimationHandler)
    CASE(EngineAudioClient)
    CASE(FaceWorld)
    CASE(FullRobotPose)
    CASE(GyroDriftDetector)
    CASE(HabitatDetector)
    CASE(ImuSensor)
    CASE(JdocsManager)
    CASE(LocaleComponent)
    CASE(Map)
    CASE(MicComponent)
    CASE(MoodManager)
    CASE(Movement)
    CASE(NVStorage)
    CASE(PathPlanning)
    CASE(PetWorld)
    CASE(PhotographyManager)
    CASE(PowerStateManager)
    CASE(ProxSensor)
    CASE(PublicStateBroadcaster)
    CASE(RangeSensor)
    CASE(RobotExternalRequestComponent)
    CASE(RobotHealthReporter)
    CASE(RobotStatsTracker)
    CASE(RobotToEngineImplMessaging)
    CASE(SDK)
    CASE(SettingsCommManager)
    CASE(SettingsManager)
    CASE(StateHistory)
    CASE(StimulationFaceDisplay)
    CASE(TextToSpeechCoordinator)
    CASE(TouchSensor)
    CASE(UserEntitlementsManager)
    CASE(VariableSnapshotComponent)
    CASE(Vision)
    CASE(VisionScheduleMediator)
    CASE(Count)
  }
  #undef CASE
}

} // namespace Anki
