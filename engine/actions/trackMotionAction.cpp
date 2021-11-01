/**
 * File: trackMotionAction.cpp
 *
 * Author: Andrew Stein
 * Date:   12/11/2015
 *
 * Description: Defines an action for tracking motion (on the ground), derived from ITrackAction.
 *
 *
 * Copyright: Anki, Inc. 2015
 **/

#include "engine/actions/trackMotionAction.h"
#include "engine/components/visionComponent.h"
#include "engine/externalInterface/externalInterface.h"
#include "engine/robot.h"

#include "clad/externalInterface/messageEngineToGameTag.h"
#include "clad/externalInterface/messageEngineToGame.h"

#define DEBUG_TRACKING_ACTIONS 0

namespace Anki {
namespace Vector {
  
static const char * const kLogChannelName = "Actions";

void TrackMotionAction::GetRequiredVisionModes(std::set<VisionModeRequest>& requests) const
{
  requests.insert({ VisionMode::Motion, EVisionUpdateFrequency::High });
}

ActionResult TrackMotionAction::InitInternal()
{
  if(false == GetRobot().HasExternalInterface()) {
    PRINT_NAMED_ERROR("TrackMotionAction.Init.NoExternalInterface",
                      "Robot must have an external interface so action can "
                      "subscribe to motion observation events.");
    return ActionResult::ABORT;
  }
  
  _gotNewMotionObservation = false;
  
  using namespace ExternalInterface;
  auto HandleObservedMotion = [this](const AnkiEvent<MessageEngineToGame>& event)
  {
    _gotNewMotionObservation = true;
    this->_motionObservation = event.GetData().Get_RobotObservedMotion();
  };
  
  _signalHandle = GetRobot().GetExternalInterface()->Subscribe(ExternalInterface::MessageEngineToGameTag::RobotObservedMotion, HandleObservedMotion);
  
  return ActionResult::SUCCESS;
} // InitInternal()

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -  
ITrackAction::UpdateResult TrackMotionAction::UpdateTracking(Radians& absPanAngle, Radians& absTiltAngle, f32& distance_mm)
{
  distance_mm = 0.f;
  
  if(_gotNewMotionObservation && _motionObservation.img_area > 0)
  {
    _gotNewMotionObservation = false;
    
    const Point2f motionCentroid(_motionObservation.img_x, _motionObservation.img_y);
    
    // Note: we start with relative angles here, but make them absolute below.
    GetRobot().GetVisionComponent().GetCamera().ComputePanAndTiltAngles(motionCentroid, absPanAngle, absTiltAngle);
    
    // Find pose of robot at time motion was observed
    HistRobotState* histStatePtr = nullptr;
    RobotTimeStamp_t junkTime;
    if(RESULT_OK != GetRobot().GetStateHistory()->ComputeAndInsertStateAt(_motionObservation.timestamp, junkTime, &histStatePtr)) {
      PRINT_NAMED_ERROR("TrackMotionAction.UpdateTracking.PoseHistoryError",
                        "Could not get historical pose for motion observed at t=%d (lastRobotMsgTime = %d)",
                        _motionObservation.timestamp,
                        (TimeStamp_t)GetRobot().GetLastMsgTimestamp());
      return UpdateResult::NoNewInfo;
    }
    
    DEV_ASSERT(nullptr != histStatePtr, "TrackMotionAction.UpdateTracking.NullHistStatePtr");
    
    // Make absolute
    absTiltAngle += histStatePtr->GetHeadAngle_rad();
    absPanAngle  += histStatePtr->GetPose().GetRotation().GetAngleAroundZaxis();
    
    if(DEBUG_TRACKING_ACTIONS)
    {
      PRINT_CH_INFO(kLogChannelName, "TrackMotionAction.UpdateTracking.Motion",
                    "Motion area=%.1f%%, centroid=(%.1f,%.1f)",
                    _motionObservation.img_area * 100.f,
                    motionCentroid.x(), motionCentroid.y());
    }
    
    return UpdateResult::NewInfo;
    
  } // if(_gotNewMotionObservation && _motionObservation.img_area > 0)
  
  return UpdateResult::NoNewInfo;
  
} // UpdateTracking()
  
} // namespace Vector
} // namespace Anki
