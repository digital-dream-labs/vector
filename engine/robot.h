/**
 * File: robot.h
 *
 * Author: Andrew Stein feat. Various Artists
 * Date:   8/23/13
 *
 * Description: Defines a Robot representation on the Basestation, which is
 *              in charge of communicating with (and mirroring the state of)
 *              a physical (hardware) robot.
 *
 *              Convention: Set*() methods do not actually command the physical
 *              robot to do anything; they simply update some aspect of the
 *              state or internal representation of the Basestation Robot.
 *              To command the robot to "do" something, methods beginning with
 *              other action words, or add IAction objects to its ActionList.
 *
 * Copyright: Anki, Inc. 2013
 **/

#ifndef ANKI_VECTOR_BASESTATION_ROBOT_H
#define ANKI_VECTOR_BASESTATION_ROBOT_H

#include "engine/actionableObject.h"
#include "engine/contextWrapper.h"
#include "engine/cpuStats.h"
#include "engine/encodedImage.h"
#include "engine/engineTimeStamp.h"
#include "engine/events/ankiEvent.h"
#include "engine/fullRobotPose.h"
#include "engine/robotComponents_fwd.h"

#include "util/entityComponent/dependencyManagedEntity.h"
#include "util/entityComponent/entity.h"
#include "util/helpers/noncopyable.h"


namespace Anki {

// Forward declaration:

class PoseOriginList;

namespace Util {
class RandomGenerator;
namespace Data {
class DataPlatform;
}
}

namespace Vector {

// Forward declarations:
class AppCubeConnectionSubscriber;
class AIComponent;
class ActionList;
class BehaviorFactory;
class BehaviorManager;
class BehaviorSystemManager;
class BlockTapFilterComponent;
class BlockWorld;
class CozmoContext;
class CubeAccelComponent;
class CubeBatteryComponent;
class CubeCommsComponent;
class CubeConnectionCoordinator;
class CubeInteractionTracker;
class DrivingAnimationHandler;
class DataAccessorComponent;
enum class EngineErrorCode : uint8_t;
class FaceWorld;
class IExternalInterface;
class IGatewayInterface;
class LocaleComponent;
class MoodManager;
class MovementComponent;
class NVStorageComponent;
enum class OffTreadsState : int8_t;
class PetWorld;
class PhotographyManager;
class PowerStateManager;
class RobotEventHandler;
class RobotGyroDriftDetector;
class RobotHealthReporter;
class RobotStateHistory;
class HistRobotState;
class IExternalInterface;
struct RobotState;
class CubeLightComponent;
class BackpackLightComponent;
class RobotToEngineImplMessaging;
class PublicStateBroadcaster;
class VariableSnapshotComponent;
class VisionComponent;
class VisionScheduleMediator;
class PathComponent;
class DockingComponent;
class CarryingComponent;
class CliffSensorComponent;
class ProxSensorComponent;
class RangeSensorComponent;
class TouchSensorComponent;
class ImuComponent;
class AnimationComponent;
class MapComponent;
class MicComponent;
class BatteryComponent;
class BeatDetectorComponent;
class HabitatDetectorComponent;
class TextToSpeechCoordinator;
class SDKComponent;
enum class ShutdownReason : uint8_t;

namespace Audio {
  class EngineRobotAudioClient;
}

namespace RobotInterface {
class MessageHandler;
class EngineToRobot;
class RobotToEngine;
enum class EngineToRobotTag : uint8_t;
enum class RobotToEngineTag : uint8_t;
} // end namespace RobotInterface

namespace ExternalInterface {
class MessageEngineToGame;
struct RobotState;
}

namespace external_interface {
class RobotState;
}


class Robot : private Util::noncopyable
{
public:

  Robot(const RobotID_t robotID, CozmoContext* context);
  ~Robot();

  // =========== Robot properties ===========

  const RobotID_t GetID() const;

  bool IsPhysical() const {
#ifdef SIMULATOR
    return false;
#else
    return true;
#endif
  }

  // Whether or not to ignore all incoming external messages that create/queue actions
  // Use with care: Make sure a call to ignore is eventually followed by a call to unignore
  void SetIgnoreExternalActions(bool ignore) { _ignoreExternalActions = ignore; }
  bool GetIgnoreExternalActions() { return _ignoreExternalActions;}

  // =========== Robot Update ===========

  Result Update();

  Result UpdateFullRobotState(const RobotState& msg);

  bool HasReceivedRobotState() const;

  const bool GetSyncRobotAcked() const {return _syncRobotAcked;}
  void       SetSyncRobotAcked()       {_syncRobotAcked = true; _syncRobotSentTime_sec = 0.0f; }

  Result SyncRobot();  // TODO:(bn) only for robot event handler, move out of this header...

  RobotTimeStamp_t GetLastMsgTimestamp() const { return _lastMsgTimestamp; }

  // This is just for unit tests to fake a syncRobotAck message from the robot
  // and force the head into calibrated state.
  void FakeSyncRobotAck() { _syncRobotAcked = true; _isHeadCalibrated = true; _isLiftCalibrated = true; }

  // =========== Components ===========

  template<typename T>
  bool HasComponent() const {
    return (_components != nullptr) &&
           _components->HasComponent<T>() &&
           _components->GetComponent<T>().IsComponentValid();
  }

  template<typename T>
  T& GetComponent() const {return _components->GetComponent<T>();}

  template<typename T>
  T& GetComponent() {return _components->GetComponent<T>();}


  template<typename T>
  T* GetComponentPtr() const {return _components->GetComponentPtr<T>();}

  template<typename T>
  T* GetComponentPtr() {return _components->GetComponentPtr<T>();}

  //
  // Most components declare both const and non-const accessors.
  // If your component does not fit this pattern, add custom code below.
  //
  // Handy macro tricks: Use ## to splice macro parameters into a symbol
  //
  #define INLINE_GETTERS(T) \
    inline T & Get##T() { return GetComponent<T>(); } \
    inline const T & Get##T() const { return GetComponent<T>(); }

  INLINE_GETTERS(AIComponent)
  INLINE_GETTERS(AnimationComponent)
  INLINE_GETTERS(AppCubeConnectionSubscriber)
  INLINE_GETTERS(BackpackLightComponent)
  INLINE_GETTERS(BatteryComponent)
  INLINE_GETTERS(BeatDetectorComponent)
  INLINE_GETTERS(BlockWorld)
  INLINE_GETTERS(CarryingComponent)
  INLINE_GETTERS(CliffSensorComponent)
  INLINE_GETTERS(CubeAccelComponent)
  INLINE_GETTERS(CubeBatteryComponent)
  INLINE_GETTERS(CubeCommsComponent)
  INLINE_GETTERS(CubeConnectionCoordinator)
  INLINE_GETTERS(CubeInteractionTracker)
  INLINE_GETTERS(CubeLightComponent)
  INLINE_GETTERS(DataAccessorComponent)
  INLINE_GETTERS(DockingComponent)
  INLINE_GETTERS(DrivingAnimationHandler)
  INLINE_GETTERS(FaceWorld)
  INLINE_GETTERS(HabitatDetectorComponent)
  INLINE_GETTERS(LocaleComponent)
  INLINE_GETTERS(MapComponent)
  INLINE_GETTERS(MicComponent)
  INLINE_GETTERS(MoodManager)
  INLINE_GETTERS(NVStorageComponent)
  INLINE_GETTERS(PathComponent)
  INLINE_GETTERS(PetWorld)
  INLINE_GETTERS(PhotographyManager)
  INLINE_GETTERS(PowerStateManager)
  INLINE_GETTERS(ProxSensorComponent)
  INLINE_GETTERS(ImuComponent)
  INLINE_GETTERS(PublicStateBroadcaster)
  INLINE_GETTERS(RobotHealthReporter)
  INLINE_GETTERS(RobotToEngineImplMessaging)
  INLINE_GETTERS(SDKComponent)
  INLINE_GETTERS(TextToSpeechCoordinator)
  INLINE_GETTERS(TouchSensorComponent)
  INLINE_GETTERS(VariableSnapshotComponent)
  INLINE_GETTERS(VisionComponent)
  INLINE_GETTERS(VisionScheduleMediator)

  #undef INLINE_GETTERS

  const PoseOriginList& GetPoseOriginList() const { return *_poseOrigins.get(); }

  inline RangeSensorComponent& GetRangeSensorComponent() {return GetComponent<RangeSensorComponent>(); }
  inline const RangeSensorComponent& GetRangeSensorComponent() const {return GetComponent<RangeSensorComponent>(); }

  inline BlockTapFilterComponent& GetBlockTapFilter() {return GetComponent<BlockTapFilterComponent>();}
  inline const BlockTapFilterComponent& GetBlockTapFilter() const {return GetComponent<BlockTapFilterComponent>();}

  inline MovementComponent& GetMoveComponent() {return GetComponent<MovementComponent>();}
  inline const MovementComponent& GetMoveComponent() const {return GetComponent<MovementComponent>();}

  ActionList& GetActionList() { return GetComponent<ActionList>(); }

  Audio::EngineRobotAudioClient* GetAudioClient() { return &GetComponent<Audio::EngineRobotAudioClient>(); }
  const Audio::EngineRobotAudioClient* GetAudioClient() const { return &GetComponent<Audio::EngineRobotAudioClient>(); }

  RobotStateHistory* GetStateHistory() { return GetComponentPtr<RobotStateHistory>(); }
  const RobotStateHistory* GetStateHistory() const { return GetComponentPtr<RobotStateHistory>(); }

  // Get pointer to robot's runtime context.
  // Nothing outside of robot is allowed to modify robot's context.
  const CozmoContext* GetContext() const { return _context; }

  const Util::RandomGenerator& GetRNG() const;
  Util::RandomGenerator& GetRNG();


  // =========== Localization ===========

  bool IsLocalized() const;
  void Delocalize(bool isCarryingObject);

  // Updates the pose of the robot.
  // Sends new pose down to robot (on next tick).
  Result SetNewPose(const Pose3d& newPose);

  // Get the ID of the object we are localized to
  const ObjectID& GetLocalizedTo() const {return _localizedToID;}

  // Set the object we are localized to.
  // Use nullptr to UnSet the localizedTo object but still mark the robot
  // as localized (i.e. to "odometry").
  Result SetLocalizedTo(const ObservableObject* object);

  // Has the robot moved since it was last localized
  bool HasMovedSinceBeingLocalized() const;

  // Get the squared distance to the closest, most recently observed marker
  // on the object we are localized to
  f32 GetLocalizedToDistanceSq() const;

  Result LocalizeToObject(const ObservableObject* seenObject, ObservableObject* existingObject);

  // Updates pose to be on charger
  Result SetPoseOnCharger();

  // Update's the robot's pose to be in front of the
  // charger as if it had just rolled off the charger.
  Result SetPosePostRollOffCharger();

  // Sets the charger that it's docking to
  void           SetCharger(const ObjectID& chargerID) { _chargerID = chargerID; }
  const ObjectID GetCharger() const                    { return _chargerID; }

  // =========== Cliff reactions ===========

  // whether or not the robot should react (the sensor may still be enabled)
  const bool GetIsCliffReactionDisabled() { return _isCliffReactionDisabled; }

  // =========== Face Display ============
  u32 GetDisplayWidthInPixels() const;
  u32 GetDisplayHeightInPixels() const;

  // =========== Camera / Vision ===========
  Vision::Camera GetHistoricalCamera(const HistRobotState& histState, RobotTimeStamp_t t) const;
  Result         GetHistoricalCamera(RobotTimeStamp_t t_request, Vision::Camera& camera) const;
  Pose3d         GetHistoricalCameraPose(const HistRobotState& histState, RobotTimeStamp_t t) const;

  // Return the timestamp of the last _processed_ image
  RobotTimeStamp_t GetLastImageTimeStamp() const;

  // =========== Pose (of the robot or its parts) ===========
  const Pose3d&       GetPose() const;
  const PoseFrameID_t GetPoseFrameID()  const { return _frameId; }
  const Pose3d&       GetWorldOrigin()  const;
  PoseOriginID_t      GetWorldOriginID()const;

  Pose3d              GetCameraPose(const f32 atAngle) const;
  Transform3d         GetLiftTransformWrtCamera(const f32 atLiftAngle, const f32 atHeadAngle) const;

  OffTreadsState GetOffTreadsState() const;
  EngineTimeStamp_t GetOffTreadsStateLastChangedTime_ms() const { return _timeOffTreadStateChanged_ms; }

  // Return whether the given pose is in the same origin as the robot's current origin
  bool IsPoseInWorldOrigin(const Pose3d& pose) const;

  // Figure out the head angle to look at the given pose. Orientation of pose is
  // ignored. All that matters is its distance from the robot (in any direction)
  // and height. Note that returned head angle can be outside possible range.
  Result ComputeHeadAngleToSeePose(const Pose3d& pose, Radians& headAngle, f32 yTolFrac) const;

  // Figure out absolute body pan and head tilt angles to turn towards a point in an image.
  // Note that the head tilt is approximate because this function makes the simplifying
  // assumption that the head rotates around the camera center.
  // If isPointNormalized=true, imgPoint.x() and .y() must be on the interval [0,1] and
  // are assumed to be relative to image size.
  Result ComputeTurnTowardsImagePointAngles(const Point2f& imgPoint, const RobotTimeStamp_t timestamp,
                                            Radians& absPanAngle, Radians& absTiltAngle,
                                            const bool isPointNormalized = false) const;

  // These change the robot's internal (basestation) representation of its
  // head angle, and lift angle, but do NOT actually command the
  // physical robot to do anything!
  void SetHeadAngle(const f32& angle);
  void SetLiftAngle(const f32& angle);

  void SetHeadCalibrated(bool isCalibrated);
  void SetLiftCalibrated(bool isCalibrated);

  bool IsHeadCalibrated() const;
  bool IsLiftCalibrated() const;

  bool IsHeadMotorOutOfBounds() const { return _isHeadMotorOutOfBounds; }
  bool IsLiftMotorOutOfBounds() const { return _isLiftMotorOutOfBounds; }

  // #notImplemented
  //    // Get 3D bounding box of the robot at its current pose or a given pose
  //    void GetBoundingBox(std::array<Point3f, 8>& bbox3d, const Point3f& padding_mm) const;
  //    void GetBoundingBox(const Pose3d& atPose, std::array<Point3f, 8>& bbox3d, const Point3f& padding_mm) const;

  // Get the bounding quad of the robot at its current or a given pose
  Quad2f GetBoundingQuadXY(const f32 padding_mm = 0.f) const; // at current pose
  static Quad2f GetBoundingQuadXY(const Pose3d& atPose, const f32 paddingScale = 0.f); // at specific pose

  // Return current height of lift's gripper
  f32 GetLiftHeight() const;

  // Leaves input liftPose's parent alone and computes its position w.r.t.
  // liftBasePose, given the angle
  static void ComputeLiftPose(const f32 atAngle, Pose3d& liftPose);

  // Get pitch angle of robot
  Radians GetPitchAngle() const;

  // Get roll angle of robot
  Radians GetRollAngle() const;

  // Return current bounding height of the robot, taking into account whether lift
  // is raised
  f32 GetHeight() const;

  // Wheel speeds, mm/sec
  f32 GetLeftWheelSpeed() const { return _leftWheelSpeed_mmps; }
  f32 GetRightWheelSpeed() const { return _rightWheelSpeed_mmps; }

  // Return pose of robot's drive center based on what it's currently carrying
  const Pose3d& GetDriveCenterPose() const;

  // Computes the drive center offset from origin based on current carrying state
  f32 GetDriveCenterOffset() const;

  // Computes pose of drive center for the given robot pose
  void ComputeDriveCenterPose(const Pose3d &robotPose, Pose3d &driveCenterPose) const;

  // Computes robot origin pose for the given drive center pose
  void ComputeOriginPose(const Pose3d &driveCenterPose, Pose3d &robotPose) const;

  // Returns true if robot is not in the OnTreads position
  bool IsPickedUp() const { return _isPickedUp; }

  // Returns true if being moved enough to believe robot is being held by a person.
  // Note: Can only be true if IsPickedUp() is also true.
  bool IsBeingHeld() const { return _isBeingHeld; }
  EngineTimeStamp_t GetBeingHeldLastChangedTime_ms() const { return _timeHeldStateChanged_ms; }

  // =========== IMU Data =============

  // Returns pointer to robot accelerometer readings in mm/s^2 with respect to head frame.
  // x-axis: points out face
  // y-axis: points out left ear
  // z-axis: points out top of head
  const AccelData& GetHeadAccelData() const {return _robotAccel; }

  // Returns pointer to robot gyro readings in rad/s with respect to head frame.
  // x-axis: points out face
  // y-axis: points out left ear
  // z-axis: points out top of head
  const GyroData& GetHeadGyroData() const {return _robotGyro; }

  // Returns the current accelerometer magnitude (norm of all 3 axes).
  float GetHeadAccelMagnitude() const {return _robotAccelMagnitude; }

  // Returns the current accelerometer magnitude, after being low-pass filtered.
  float GetHeadAccelMagnitudeFiltered() const {return _robotAccelMagnitudeFiltered; }

  // IMU temperature sent from the robot
  void SetImuTemperature(const float temp) { _robotImuTemperature_degC = temp; }
  float GetImuTemperature() const {return _robotImuTemperature_degC; }

  // send the request down to the robot
  Result RequestIMU(const u32 length_ms) const;

  // ============ IMU Event Handling/Tracking ==============

  // Event handler for whenever the IMU reports that the robot was poked.
  // Logs the time at which the poke event is received for future reference.
  void HandlePokeEvent();

  // Returns the number of milliseconds elapsed since the IMU reported being poked.
  EngineTimeStamp_t GetTimeSinceLastPoke_ms() const;

  // =========== Animation Commands =============

  // Returns true if the robot is currently playing an animation, according
  // to most recent state message. NOTE: Will also be true if the animation
  // is the "idle" animation!
  bool IsAnimating() const;

  u8 GetEnabledAnimationTracks() const;

  // =========== Mood =============

  // Load in all data-driven emotion events // TODO: move to mood manager?
  void LoadEmotionEvents();

  // =========== Pose history =============

  // Adds robot state information to history at t = state.timestamp
  // Only state updates should be calling this, however, it is exposed for unit tests
  Result AddRobotStateToHistory(const Pose3d& pose, const RobotState& state);

  // Increments frameID and adds a vision-only pose to history
  // Sets a flag to send a localization update on the next tick
  Result AddVisionOnlyStateToHistory(const RobotTimeStamp_t t,
                                     const Pose3d& pose,
                                     const f32 head_angle,
                                     const f32 lift_angle);

  // Updates the current pose to the best estimate based on
  // historical poses including vision-based poses.
  // Returns true if the pose is successfully updated, false otherwise.
  bool UpdateCurrPoseFromHistory();

  Result GetComputedStateAt(const RobotTimeStamp_t t_request, Pose3d& pose) const;

  // =========  Block messages  ============

  bool WasObjectTappedRecently(const ObjectID& objectID) const;

  // ======== Power button ========

  bool IsPowerButtonPressed() const { return _powerButtonPressed; }
  TimeStamp_t GetTimeSincePowerButtonPressed_ms() const;

  // Abort everything the robot is doing, including path following, actions,
  // animations, and docking. This is like the big red E-stop button.
  // TODO: Probably need a more elegant way of doing this.
  Result AbortAll();

  // Abort things individually
  Result AbortAnimation();

  // Helper template for sending Robot messages with clean syntax
  template<typename T, typename... Args>
  Result SendRobotMessage(Args&&... args) const
  {
    return SendMessage(RobotInterface::EngineToRobot(T(std::forward<Args>(args)...)));
  }

  // Send a message to the physical robot
  Result SendMessage(const RobotInterface::EngineToRobot& message,
                     bool reliable = true, bool hot = false) const;


  // =========  Events  ============
  bool HasExternalInterface() const;
  bool HasGatewayInterface() const;

  IExternalInterface* GetExternalInterface() const;
  IGatewayInterface* GetGatewayInterface() const;

  RobotInterface::MessageHandler* GetRobotMessageHandler() const;
  RobotEventHandler& GetRobotEventHandler();
  void SetSDKRequestingImage(bool requestingImage) { _sdkRequestingImage = requestingImage; }
  const bool GetSDKRequestingImage() const { return _sdkRequestingImage; }

  // Handle various message types
  template<typename T>
  void HandleMessage(const T& msg);

  // Convenience wrapper for broadcasting an event if the robot has an ExternalInterface.
  // Does nothing if not. Returns true if event was broadcast, false if not (i.e.
  // if there was no external interface).
  bool Broadcast(ExternalInterface::MessageEngineToGame&& event);

  bool Broadcast(VizInterface::MessageViz&& event);

  Util::Data::DataPlatform* GetContextDataPlatform();

  // Populate a RobotState clad message with robot's current state information (suitable for sending to external listeners)
  ExternalInterface::RobotState GetRobotState() const;

  // Populate a RobotState proto message with robot's current state information (suitable for sending to external listeners)
  external_interface::RobotState* GenerateRobotStateProto() const;

  // Populate a RobotState message with default values (suitable for sending to the robot itself, e.g. in unit tests)
  static RobotState GetDefaultRobotState();

  const u32 GetHeadSerialNumber() const { return _serialNumberHead; }

  void Shutdown(ShutdownReason reason);
  bool ToldToShutdown(ShutdownReason& reason) const { reason = _shutdownReason; return _toldToShutdown; }

  bool SetLocale(const std::string & locale);

protected:
  bool _toldToShutdown = false;
  ShutdownReason _shutdownReason = ShutdownReason::SHUTDOWN_UNKNOWN;

  CozmoContext* _context;
  std::unique_ptr<PoseOriginList> _poseOrigins;

  using EntityType = DependencyManagedEntity<RobotComponentID>;
  using ComponentPtr = std::unique_ptr<EntityType>;

  ComponentPtr _components;

  // The robot's identifier
  RobotID_t _ID;
  u32       _serialNumberHead = 0;

  // Whether or not sync was acknowledged by physical robot
  bool _syncRobotAcked = false;

  // Flag indicating whether a robotStateMessage was ever received
  RobotTimeStamp_t _lastMsgTimestamp;
  bool             _newStateMsgAvailable = false;

  Pose3d         _driveCenterPose;
  PoseFrameID_t  _frameId                   = 0;
  ObjectID       _localizedToID; // ID of mat object robot is localized to
  bool           _hasMovedSinceLocalization = false;
  u32            _numMismatchedFrameIDs     = 0;

  // May be true even if not localized to an object, if robot has not been picked up
  bool _isLocalized                  = true;
  bool _localizedToFixedObject; // false until robot sees a _fixed_ mat
  bool _needToSendLocalizationUpdate = false;

  // Whether or not to ignore all external action messages
  bool _ignoreExternalActions = false;

  // Stores (squared) distance to the closest observed marker of the object we're localized to
  f32 _localizedMarkerDistToCameraSq = -1.0f;

  f32              _leftWheelSpeed_mmps;
  f32              _rightWheelSpeed_mmps;

  // We can assume the motors are calibrated by the time
  // engine connects to robot
  bool             _isHeadCalibrated = true;
  bool             _isLiftCalibrated = true;

  // flags that represent whether the motor values exceeded
  // the expected range of the respective motors. If it is
  // out of bounds, it'll trigger a calibration
  bool             _isHeadMotorOutOfBounds = false;
  bool             _isLiftMotorOutOfBounds = false;

  // Charge base ID that is being docked to
  ObjectID         _chargerID;

  // State
  bool               _powerButtonPressed        = false;
  EngineTimeStamp_t  _timePowerButtonPressed_ms = 0;
  bool               _isPickedUp                = false;
  EngineTimeStamp_t  _timeLastPoked             = 0;
  bool               _isBeingHeld               = false;
  EngineTimeStamp_t  _timeHeldStateChanged_ms   = 0;
  bool               _isCliffReactionDisabled   = false;
  bool               _gotStateMsgAfterRobotSync = false;
  u32                _lastStatusFlags           = 0;
  bool               _sdkRequestingImage        = false;

  OffTreadsState     _offTreadsState;
  OffTreadsState     _awaitingConfirmationTreadState;
  EngineTimeStamp_t  _timeOffTreadStateChanged_ms    = 0;
  RobotTimeStamp_t   _fallingStartedTime_ms          = 0;

  // IMU data
  AccelData        _robotAccel;
  GyroData         _robotGyro;
  float            _robotAccelMagnitude = 0.0f; // current magnitude of accelerometer data (norm of all three axes)
  float            _robotAccelMagnitudeFiltered = 0.0f; // low-pass filtered accelerometer magnitude
  AccelData        _robotAccelFiltered; // low-pass filtered robot accelerometer data (for each axis)
  float            _robotImuTemperature_degC = 0.f;

  // Whether or not we have sent the engine is fully loaded message
  bool _sentEngineLoadedMsg = false;

  // Sets robot pose but does not update the pose on the robot.
  // Unless you know what you're doing you probably want to use
  // the public function SetNewPose()
  void SetPose(const Pose3d &newPose);

  // Takes startPose and moves it forward as if it were a robot pose by distance mm and
  // puts result in movedPose.
  static void MoveRobotPoseForward(const Pose3d &startPose, const f32 distance, Pose3d &movedPose);

  CPUStats     _cpuStats;

  // returns whether the tread state was updated or not
  bool CheckAndUpdateTreadsState(const RobotState& msg);

  Result SendAbsLocalizationUpdate(const Pose3d&             pose,
                                   const RobotTimeStamp_t&   t,
                                   const PoseFrameID_t&      frameId) const;

  // Sync with physical robot
  Result SendSyncRobot() const;

  float _syncRobotSentTime_sec = 0.0f;

  // Send robot's current pose
  Result SendAbsLocalizationUpdate() const;

  // Request imu log from robot
  Result SendIMURequest(const u32 length_ms) const;

  Result SendAbortAnimation();

  // As a temp dev step, try swapping out aiComponent after construction/initialization
  // for something like unit tests - theoretically this should be handled by having a non
  // fully enumerated constructor option, but for the time being use this for dev/testing purposes
  // only since caching etc could blow it all to shreds
  void DevReplaceAIComponent(AIComponent* aiComponent, bool shouldManage = false);

  // Performs various startup checks and displays fault codes as appropriate

  // Returns true if the check is complete, false if the check is still running
  // If return true, then res will be set appropriately
  bool UpdateStartupChecks(Result& res);
  bool UpdateCameraStartupChecks(Result& res);
  bool UpdateGyroCalibChecks(Result& res);
  bool UpdateToFStartupChecks(Result& res);

  bool IsStatusFlagSet(RobotStatusFlag flag) const { return _lastStatusFlags & static_cast<u32>(flag); }

}; // class Robot


//
// Inline accessors:
//
inline const RobotID_t Robot::GetID(void) const
{ return _ID; }

inline const Pose3d& Robot::GetPose(void) const
{
  ANKI_VERIFY(GetComponent<FullRobotPose>().GetPose().GetRootID() == GetWorldOriginID(),
              "Robot.GetPose.BadPoseRootOrWorldOriginID",
              "WorldOriginID:%d(%s), RootID:%d",
              GetWorldOriginID(), GetWorldOrigin().GetName().c_str(),
              GetComponent<FullRobotPose>().GetPose().GetRootID());

  // TODO: COZMO-1637: Once we figure this out, switch this back to dev_assert for efficiency
  ANKI_VERIFY(GetComponent<FullRobotPose>().GetPose().HasSameRootAs(GetWorldOrigin()),
              "Robot.GetPose.PoseOriginNotWorldOrigin",
              "WorldOrigin: %s, Pose: %s",
              GetWorldOrigin().GetNamedPathToRoot(false).c_str(),
              GetComponent<FullRobotPose>().GetPose().GetNamedPathToRoot(false).c_str());

  return GetComponent<FullRobotPose>().GetPose();
}

inline const Pose3d& Robot::GetDriveCenterPose(void) const
{
  // TODO: COZMO-1637: Once we figure this out, switch this back to dev_assert for efficiency
  ANKI_VERIFY(_driveCenterPose.HasSameRootAs(GetWorldOrigin()),
              "Robot.GetDriveCenterPose.PoseOriginNotWorldOrigin",
              "WorldOrigin: %s, Pose: %s",
              GetWorldOrigin().GetNamedPathToRoot(false).c_str(),
              _driveCenterPose.GetNamedPathToRoot(false).c_str());

  return _driveCenterPose;
}

inline f32 Robot::GetLocalizedToDistanceSq() const {
  return _localizedMarkerDistToCameraSq;
}

inline bool Robot::HasMovedSinceBeingLocalized() const {
  return _hasMovedSinceLocalization;
}

inline bool Robot::IsLocalized() const {

  DEV_ASSERT(_isLocalized || (!_isLocalized && !_localizedToID.IsSet()),
             "Robot can't think it is localized and have localizedToID set!");

  return _isLocalized;
}

} // namespace Vector
} // namespace Anki

#endif // ANKI_VECTOR_BASESTATION_ROBOT_H
