/**
 * File: trackGroundPointAction.cpp
 *
 * Author: Andrew Stein
 * Date:   04/20/2017
 *
 * Description: ITrackingAction for tracking points on the ground.
 *
 *
 * Copyright: Anki, Inc. 2017
 **/


#include "engine/actions/trackGroundPointAction.h"

#include "engine/components/movementComponent.h"
#include "engine/components/visionComponent.h"
#include "engine/externalInterface/externalInterface.h"
#include "engine/robot.h"

#include "clad/externalInterface/messageEngineToGameTag.h"
#include "clad/externalInterface/messageEngineToGame.h"

#include "coretech/common/engine/utils/timer.h"

#include "util/math/math.h"

#define DEBUG_TRACKING_ACTIONS 0
#define LOG_CHANNEL "Actions"

namespace Anki {
namespace Vector {

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
TrackGroundPointAction::TrackGroundPointAction(const ExternalInterface::MessageEngineToGameTag& salientPointTag)
: ITrackAction("TrackGroundPoint", RobotActionType::TRACK_GROUND_POINT)
{
  _salientTag = salientPointTag;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void TrackGroundPointAction::GetRequiredVisionModes(std::set<VisionModeRequest>& requests) const
{
  switch(_salientTag)
  {
    case ExternalInterface::MessageEngineToGameTag::RobotObservedLaserPoint:
    {
      requests.insert({ VisionMode::Lasers, EVisionUpdateFrequency::High });
      break;
    }
      
    // If other messages yield valid points for tracking, add support for them here (to enable the vision mode
    // which produces them)
      
    default:
      ANKI_VERIFY(false, "TrackGroundPointAction.Constructor.NoVisionModeForTag",
                  "Unsupported Tag: %s", ExternalInterface::MessageEngineToGameTagToString(_salientTag));
  }
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
ActionResult TrackGroundPointAction::InitInternal()
{
  if(!GetRobot().HasExternalInterface()) {
    PRINT_NAMED_ERROR("TrackGroundPointAction.Init.NoExternalInterface",
                      "Robot must have an external interface so action can "
                      "subscribe to motion observation events.");
    return ActionResult::ABORT;
  }
  
  _gotNewPointObservation = false;
  _pointObservation.timestamp = 0;
  _prevPointObservation.timestamp = 0;

  // Subscribe to the right message based on the saliency tag specified at construction
  {
    using namespace ExternalInterface;
    
    switch(_salientTag)
    {
      case MessageEngineToGameTag::RobotObservedLaserPoint:
        _getObservedPointFromEvent = [](const MessageEngineToGame& msg) {
          auto & data = msg.Get_RobotObservedLaserPoint();
          return PointObservation{
            .timestamp = data.timestamp,
            .groundArea = data.ground_area_fraction,
            .groundPoint = {(f32)data.ground_x_mm, (f32)data.ground_y_mm},
          };
        };
        break;
        
      default:
        PRINT_NAMED_ERROR("TrackGroundPointAction.InitInternal.UnsupportedMessageTag",
                          "%s", MessageEngineToGameTagToString(_salientTag));
        return ActionResult::BAD_MESSAGE_TAG;
    }
    
    DEV_ASSERT(_getObservedPointFromEvent != nullptr,
               "TrackGroundPointAction.InitInternal.GetObservationFunctionNotSet");
    
    auto handler = [this](const AnkiEvent<ExternalInterface::MessageEngineToGame>& event)
    {
      auto const& newObservation = _getObservedPointFromEvent( event.GetData() );
      
      if(newObservation.groundArea > 0.f)
      {
        _gotNewPointObservation = true;
        std::swap(_prevPointObservation, _pointObservation);
        _pointObservation = newObservation;
        
        if(_isXPredictionEnabled || _isYPredictionEnabled)
        {
          // Can predict once we get two observations close enough together
          const TimeStamp_t kMaxPredictionTimeDelta_ms = 250;
          _canPredict = (_pointObservation.timestamp - _prevPointObservation.timestamp < kMaxPredictionTimeDelta_ms);
        }
      }
    };
    
    _signalHandle = GetRobot().GetExternalInterface()->Subscribe(_salientTag, handler);
  }
  
  // TODO: Use an action or animation for this? (Not super simple b/c base class prevents overloading CheckIfDone)
  GetRobot().GetMoveComponent().MoveHeadToAngle(MIN_HEAD_ANGLE, MAX_HEAD_SPEED_RAD_PER_S, MAX_HEAD_ACCEL_RAD_PER_S2);
  
  return ActionResult::SUCCESS;
} // InitInternal()

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
TrackGroundPointAction::~TrackGroundPointAction()
{

}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
ITrackAction::UpdateResult TrackGroundPointAction::UpdateTrackingHelper(Radians& absPanAngle_out,
                                                                        Radians& absTiltAngle_out,
                                                                        f32&     distance_mm_out)
{
  // Find pose of robot at time point was observed
  HistRobotState* histStatePtr = nullptr;
  RobotTimeStamp_t junkTime;
  if(RESULT_OK != GetRobot().GetStateHistory()->ComputeAndInsertStateAt(_pointObservation.timestamp, junkTime, &histStatePtr))
  {
    PRINT_NAMED_ERROR("TrackGroundPointAction.UpdateTrackingHelper.PoseHistoryError",
                      "Could not get historical pose for point observed at t=%d (lastRobotMsgTime = %d)",
                      (TimeStamp_t)_pointObservation.timestamp,
                      (TimeStamp_t)GetRobot().GetLastMsgTimestamp());
    
    return UpdateResult::NoNewInfo;
  }
  
  DEV_ASSERT(nullptr != histStatePtr, "TrackGroundPointAction.UpdateTrackingHelper.NullHistStatePtr");
 
  const Pose3d& histPose = histStatePtr->GetPose();
  
  const Point2f& groundPoint = ComputeGroundPointWrtCurrentRobot(histPose,
                                                                 GetRobot().GetPose(),
                                                                 _pointObservation.groundPoint);
  
  ComputeAbsAngles(GetRobot(), histPose, groundPoint, absPanAngle_out, absTiltAngle_out);
  
  if(DEBUG_TRACKING_ACTIONS)
  {
    LOG_DEBUG("TrackGroundPointAction.UpdateTrackingHelper.GotObservation",
              "Ground: area=%.3f%% centroid=(%.1f,%.1f)",
              _pointObservation.groundArea * 100.f,
              _pointObservation.groundPoint.x(), _pointObservation.groundPoint.y());
  }
  
  // If too close: distance will remain 0.f
  distance_mm_out = 0.f;
  
  const f32 groundDistFromRobot = groundPoint.x();
  if(groundDistFromRobot > _minDistance_mm)
  {
    distance_mm_out = groundPoint.x();
  }
  
  return UpdateResult::NewInfo;
}
  
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
ITrackAction::UpdateResult TrackGroundPointAction::PredictTrackingHelper(Radians& absPanAngle_out,
                                                                         Radians& absTiltAngle_out,
                                                                         f32&     distance_mm_out)
{
  DEV_ASSERT(_isXPredictionEnabled || _isYPredictionEnabled,
             "TrackGroundPointAction.PredictTrackingHelper.PredictionNotEnabled");
  
  // Convert observations to absolute coordinates so we can compare them relative to a common origin
  HistRobotState* histStatePtr1 = nullptr;
  RobotTimeStamp_t t1;
  if(RESULT_OK != GetRobot().GetStateHistory()->ComputeAndInsertStateAt(_prevPointObservation.timestamp, t1, &histStatePtr1))
  {
    PRINT_NAMED_ERROR("TrackGroundPointAction.PredictTrackingHelper.PoseHistoryError",
                      "Could not get historical pose for point observed at t=%d (lastRobotMsgTime = %d)",
                      (TimeStamp_t)_prevPointObservation.timestamp,
                      (TimeStamp_t)GetRobot().GetLastMsgTimestamp());
    
    return UpdateResult::NoNewInfo;
  }
  
  // Previous observation's ground point w.r.t. current robot position
  const Point2f& groundPoint1 = ComputeGroundPointWrtCurrentRobot(histStatePtr1->GetPose(),
                                                                  GetRobot().GetPose(),
                                                                  _prevPointObservation.groundPoint);
  
  if(DEBUG_TRACKING_ACTIONS)
  {
    LOG_DEBUG("TrackGroundPointAction.PredictTrackingHelper.GroundPoint1",
              "PrevPoint:(%.1f,%.1f) WrtCurrentRobot:%s",
              _prevPointObservation.groundPoint.x(), _prevPointObservation.groundPoint.y(),
              groundPoint1.ToString().c_str());
  }
  
  HistRobotState* histStatePtr2 = nullptr;
  RobotTimeStamp_t t2;
  if(RESULT_OK != GetRobot().GetStateHistory()->ComputeAndInsertStateAt(_pointObservation.timestamp, t2, &histStatePtr2))
  {
    PRINT_NAMED_ERROR("TrackGroundPointAction.PredictTrackingHelper.PoseHistoryError",
                      "Could not get historical pose for point observed at t=%d (lastRobotMsgTime = %d)",
                      (TimeStamp_t)_pointObservation.timestamp,
                      (TimeStamp_t)GetRobot().GetLastMsgTimestamp());
    
    return UpdateResult::NoNewInfo;
  }
  
  // Last observation's ground point w.r.t. current robot position
  const Point2f& groundPoint2 = ComputeGroundPointWrtCurrentRobot(histStatePtr2->GetPose(),
                                                                  GetRobot().GetPose(),
                                                                  _pointObservation.groundPoint);
  
  if(DEBUG_TRACKING_ACTIONS)
  {
    LOG_DEBUG("TrackGroundPointAction.PredictTrackingHelper.GroundPoint2",
              "LastPoint:(%.1f,%.1f) WrtCurrentRobot:%s",
              _pointObservation.groundPoint.x(), _pointObservation.groundPoint.y(),
              groundPoint2.ToString().c_str());
  }
  
  // Estimate ground point's velocity, relative to the current robot's position
  Point2f groundPointVel(groundPoint2);
  groundPointVel -= groundPoint1;
  groundPointVel *= (1.f / (f32)(t2 - t1));
  
  if(!_isXPredictionEnabled) {
    groundPointVel.x() = 0.f;
  }
  
  if(!_isYPredictionEnabled) {
    groundPointVel.y() = 0.f;
  }
  
  // Estimate the current position of the ground point assuming it continued traveling the
  // same velocity since it was last seen up until "now" (the last message timestamp)
  const RobotTimeStamp_t now = GetRobot().GetLastMsgTimestamp();
  DEV_ASSERT(now >= t2, "TrackGroundPointAction.PredictTrackingHelper.BadTimestamp");
  Point2f predictedGroundPoint(groundPoint2);
  predictedGroundPoint += groundPointVel * (f32)(now - t2);
  
  // Get angles using faked ground point
  // Note: not predicting head tilt!
  ComputeAbsAngles(GetRobot(), histStatePtr2->GetPose(), predictedGroundPoint, absPanAngle_out, absTiltAngle_out);
  
  // Compute the distance for tracking from the predicted ground point
  distance_mm_out = 0.f; // If too close: distance will remain 0.f
  
  if(predictedGroundPoint.x() > _minDistance_mm)
  {
    distance_mm_out = predictedGroundPoint.x();
  }
  
  if(DEBUG_TRACKING_ACTIONS)
  {
    LOG_DEBUG("TrackGroundPointAction.PredictTrackingHelper.Prediction",
              "t: %u->%u->%u x: %.2f->%.2f->%.2f y: %.2f->%.2f->%.2f "
              "pan:%.1fdeg tilt:%.1fdeg d:%.1fmm",
              (TimeStamp_t)_prevPointObservation.timestamp, (TimeStamp_t)_pointObservation.timestamp, (TimeStamp_t)now,
              _prevPointObservation.groundPoint.x(),  _pointObservation.groundPoint.x(),
              predictedGroundPoint.x(),
              _prevPointObservation.groundPoint.y(),  _pointObservation.groundPoint.y(),
              predictedGroundPoint.y(),
              absPanAngle_out.getDegrees(), absTiltAngle_out.getDegrees(), distance_mm_out);
  }
  
  return UpdateResult::PredictedInfo;
}
  
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void TrackGroundPointAction::ComputeAbsAngles(const Robot&   robot,
                                              const Pose3d&  histRobotPose,
                                              const Point2f& groundPoint,
                                              Radians&       absPanAngle,
                                              Radians&       absTiltAngle)
{
  // Tilt angle
  const f32 kComputeHeadAngleTol = 2.f*HEAD_ANGLE_TOL; // How accurately we want to compute the head angle
  const Pose3d groundPoseWrtRobot(0, Z_AXIS_3D(), {groundPoint.x(), groundPoint.y(), 0.f}, histRobotPose);
  Result result = robot.ComputeHeadAngleToSeePose(groundPoseWrtRobot, absTiltAngle, kComputeHeadAngleTol);
  if(RESULT_OK != result)
  {
    // Approximation:
    PRINT_NAMED_WARNING("TrackGroundPointAction.ComputeAbsAngles.ComputeHeadAngleToSeePoseFailed", "");
    absTiltAngle = atan2f(-NECK_JOINT_POSITION[2], groundPoint.x());
  }
  
  // Don't look up too high (so we stay focused on the ground plane)
  const f32 kMaxHeadAngle_deg = -10;
  absTiltAngle = std::min(DEG_TO_RAD(kMaxHeadAngle_deg), absTiltAngle.ToFloat());
  
  // Pan Angle:
  absPanAngle = std::atan2f(groundPoint.y(), groundPoint.x()); // starts relative
  absPanAngle  += histRobotPose.GetRotation().GetAngleAroundZaxis();
}
  
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
Point2f TrackGroundPointAction::ComputeGroundPointWrtCurrentRobot(const Pose3d&  histRobotPose,
                                                                  const Pose3d&  currentRobotPose,
                                                                  const Point2f& observedGroundPt)
{
  // Compute the ground point relative to the _current_ robot pose, based on its position relative
  // to the _historical_ pose when it was observed.
  
  Pose3d groundPose(0, Z_AXIS_3D(), {observedGroundPt.x(), observedGroundPt.y(), 0.f}, histRobotPose);
  
  const bool success = groundPose.GetWithRespectTo(currentRobotPose, groundPose);
  ANKI_VERIFY(success, "TrackGroundPointAction.GetGroundWrtCurrentRobot.GetWrtFailed", "");
  
  const Point2f groundPoint( groundPose.GetTranslation() );
  
  return groundPoint;
}
  
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
ITrackAction::UpdateResult TrackGroundPointAction::UpdateTracking(Radians& absPanAngle_out,
                                                                  Radians& absTiltAngle_out,
                                                                  f32&     distance_mm_out)
{
  UpdateResult updateResult = UpdateResult::NoNewInfo;
  
  if(_gotNewPointObservation)
  {
    _gotNewPointObservation = false;
    
    if(_pointObservation.groundPoint.x() < _maxDistance_mm)
    {
      // Normal case: just compute update from current observation
      updateResult = UpdateTrackingHelper(absPanAngle_out, absTiltAngle_out, distance_mm_out); 
    }
    else
    {
      // Got a new observation but couldn't use it
      updateResult = UpdateResult::ShouldStop;
    }
  }
  else
  {
    DEV_ASSERT_MSG(GetRobot().GetLastImageTimeStamp() >= _pointObservation.timestamp,
                   "TrackGroundPointAction.UpdateTracking.BadTimeStamps",
                   "LastImageTimestamp=%u PointObservationTimestamp=%u",
                   (TimeStamp_t)GetRobot().GetLastImageTimeStamp(), (TimeStamp_t)_pointObservation.timestamp);
    
    const RobotTimeStamp_t timeSinceLastPoint_ms = GetRobot().GetLastImageTimeStamp() - _pointObservation.timestamp;
    
    // Didn't see the point in the last image
    if(_canPredict && (timeSinceLastPoint_ms < _maxPredictionWindow_ms))
    {
      updateResult = PredictTrackingHelper(absPanAngle_out, absTiltAngle_out, distance_mm_out);
    }
    else if(timeSinceLastPoint_ms > 0)
    {
      return UpdateResult::ShouldStop;
    }
  }
  
  return updateResult;
  
} // UpdateTracking()
  
  
} // namespace Vector
} // namespace Anki
