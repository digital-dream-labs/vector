/**
 * File: trackGroundPointAction.h
 *
 * Author: Andrew Stein
 * Date:   04/20/2017
 *
 * Description: ITrackingAction for tracking observed salient points on the ground
 *
 *
 * Copyright: Anki, Inc. 2017
 **/

#ifndef __Anki_Cozmo_Basestation_TrackGroundPointAction_H__
#define __Anki_Cozmo_Basestation_TrackGroundPointAction_H__

#include "engine/actions/trackActionInterface.h"

#include "clad/types/visionModes.h"
#include "coretech/common/engine/robotTimeStamp.h"
#include "util/signals/simpleSignal_fwd.h"

namespace Anki {
namespace Vector {

class TrackGroundPointAction : public ITrackAction
{
public:
  
  // The type of RobotObservedX message this action will subscribe to is indicated by the given tag.
  // The implementation must be able to turn that message type into an observation on the ground
  // plane and have a corresponding vision mode, which will be exclusively enabled while tracking
  // is carried out.
  //
  // Currently-Supported Tags:
  //  - RobotObservedLaserPoint
  //  - TODO: Add more...
  TrackGroundPointAction(const ExternalInterface::MessageEngineToGameTag& salientPointTag);
  
  virtual ~TrackGroundPointAction();
  
  // Enable prediction: if the point being tracked has been lost but was last seen within the last duration_ms,
  // try to predict where it went and keep tracking. Can be enabled separately for X and Y.
  // X is forward/backward (along robot's direction of travel)
  // Y is left/right (from robot's POV)
  void EnablePredictionWhenLost(bool enableX, bool enableY, TimeStamp_t duration_ms);
  
protected:

  virtual void GetRequiredVisionModes(std::set<VisionModeRequest>& requests) const override;
  
  virtual ActionResult InitInternal() override;
  
  // Required by ITrackAction:
  virtual UpdateResult UpdateTracking(Radians& absPanAngle, Radians& absTiltAngle, f32& distance_mm) override;
  
private:
  
  struct PointObservation {
    RobotTimeStamp_t timestamp;
    f32              groundArea;
    Point2f          groundPoint;
  };
  
  ExternalInterface::MessageEngineToGameTag _salientTag;
  std::function<PointObservation(const ExternalInterface::MessageEngineToGame&)> _getObservedPointFromEvent = nullptr;
  
  UpdateResult UpdateTrackingHelper(Radians& absPanAngle_out, Radians& absTiltAngle_out, f32& distance_mm_out);
  
  UpdateResult PredictTrackingHelper(Radians& absPanAngle_out, Radians& absTiltAngle_out, f32& distance_mm_out);
  
  static Point2f ComputeGroundPointWrtCurrentRobot(const Pose3d&  histRobotPose,
                                                   const Pose3d&  currentRobotPose,
                                                   const Point2f& observedGroundPt);
  
  static void ComputeAbsAngles(const Robot&   robot,
                               const Pose3d&  histRobotPose,
                               const Point2f& groundPoint,
                               Radians&       absPanAngle,
                               Radians&       absTiltAngle);

  f32  _minDistance_mm = 50.f;
  f32  _maxDistance_mm = 1000.f;
  
  TimeStamp_t _maxPredictionWindow_ms = 1000;
  
  bool _gotNewPointObservation = false;
  bool _isXPredictionEnabled   = false;
  bool _isYPredictionEnabled   = false;
  bool _canPredict             = false;
  
  PointObservation _pointObservation;
  PointObservation _prevPointObservation; // for prediction
  
  Signal::SmartHandle _signalHandle;
  
}; // class TrackMotionAction
  
inline void TrackGroundPointAction::EnablePredictionWhenLost(bool enableX, bool enableY, TimeStamp_t duration_ms)
{
  DEV_ASSERT(!HasStarted(), "TrackGroundPointAction.EnablePredictionWhenLost.ActionAlreadyStarted");
  
  _isXPredictionEnabled   = enableX;
  _isYPredictionEnabled   = enableY;
  _maxPredictionWindow_ms = duration_ms;
}
  
} // namespace Vector
} // namespace Anki

#endif // __Anki_Cozmo_Basestation_TrackGroundPointAction_H__
