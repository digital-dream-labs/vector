/**
 * File: trackFaceAction.cpp
 *
 * Author: Andrew Stein
 * Date:   12/11/2015
 *
 * Description: Defines an action for tracking (human) faces, derived from ITrackAction
 *
 *
 * Copyright: Anki, Inc. 2015
 **/

#include "engine/actions/trackFaceAction.h"
#include "engine/components/movementComponent.h"
#include "engine/components/visionComponent.h"
#include "engine/faceWorld.h"
#include "engine/robot.h"
#include "coretech/common/engine/utils/timer.h"

#define DEBUG_TRACKING_ACTIONS 0
#define LOG_CHANNEL "Actions"

namespace Anki {
namespace Vector {
  
static const char * const kLogChannelName = "Actions";
  
TrackFaceAction::TrackFaceAction(FaceID rawFaceID)
  : ITrackAction("TrackFace",
                 RobotActionType::TRACK_FACE)
  , _tmpFaceID(rawFaceID)
{
  SetName("TrackFace" + std::to_string(rawFaceID));
}

TrackFaceAction::TrackFaceAction(SmartFaceID faceID)
  : ITrackAction("TrackFace",
                 RobotActionType::TRACK_FACE)
  , _faceID(faceID)
{
  SetName("TrackFace" + _faceID.GetDebugStr());
}

TrackFaceAction::~TrackFaceAction()
{
}

void TrackFaceAction::OnRobotSet()
{
  if( !_faceID.IsValid() ) {
    _faceID = GetRobot().GetFaceWorld().GetSmartFaceID(_tmpFaceID);
  }
  GetRobot().GetVisionComponent().AddAllowedTrackedFace(_faceID.GetID());
}

void TrackFaceAction::GetRequiredVisionModes(std::set<VisionModeRequest>& requests) const
{
  requests.insert({ VisionMode::Faces, EVisionUpdateFrequency::High });
}

ActionResult TrackFaceAction::InitInternal()
{  
  _lastFaceUpdate = 0;
  
  return ActionResult::SUCCESS;
} // InitInternal()

void TrackFaceAction::GetCompletionUnion(ActionCompletedUnion& completionUnion) const
{
  GetRobot().GetVisionComponent().ClearAllowedTrackedFaces();
  TrackFaceCompleted completion;
  completion.faceID = static_cast<s32>(_faceID.GetID());
  completionUnion.Set_trackFaceCompleted(std::move(completion));
}
  
ITrackAction::UpdateResult TrackFaceAction::UpdateTracking(Radians& absPanAngle, Radians& absTiltAngle, f32& distance_mm)
{
  const Vision::TrackedFace* face = GetRobot().GetFaceWorld().GetFace(_faceID);
  distance_mm = 0.f;
  
  if(nullptr == face) {
    // No such face
    PRINT_CH_INFO(kLogChannelName, "TrackFaceAction.UpdateTracking.BadFaceID",
                  "No face %s in FaceWorld",
                  _faceID.GetDebugStr().c_str());
    return UpdateResult::NoNewInfo;
  }
  
  // Only update pose if we've actually observed the face again since last update
  if(face->GetTimeStamp() <= _lastFaceUpdate) {
    return UpdateResult::NoNewInfo;
  }
  _lastFaceUpdate = face->GetTimeStamp();
  
  Pose3d headPoseWrtRobot;
  if(false == face->GetHeadPose().GetWithRespectTo(GetRobot().GetPose(), headPoseWrtRobot)) {
    PRINT_NAMED_ERROR("TrackFaceAction.UpdateTracking.PoseOriginError",
                      "Could not get pose of face w.r.t. robot.");
    return UpdateResult::NoNewInfo;
  }
  
  const f32 xDist = headPoseWrtRobot.GetTranslation().x();
  const f32 yDist = headPoseWrtRobot.GetTranslation().y();
  
  // NOTE: This isn't perfectly accurate since it doesn't take into account the
  // the head angle and is simply using the neck joint (which should also
  // probably be queried from the robot instead of using the constant here)
  const f32 zDist = headPoseWrtRobot.GetTranslation().z() - NECK_JOINT_POSITION[2];

  if(DEBUG_TRACKING_ACTIONS)
  {
    LOG_INFO("TrackFaceAction.UpdateTracking.HeadPose",
             "Translation w.r.t. robot = (%.1f, %.1f, %.1f) [t=%d]",
             xDist, yDist, zDist, face->GetTimeStamp());
  }
  
  const f32 xyDistSq = xDist*xDist + yDist*yDist;
  if (xyDistSq <= 0.f)
  {
    DEV_ASSERT(false, "TrackFaceAction.UpdateTracking.ZeroDistance");
    return UpdateResult::NoNewInfo;
  }
  
  absTiltAngle = std::atan(zDist/std::sqrt(xyDistSq));
  absPanAngle  = std::atan2(yDist, xDist) + GetRobot().GetPose().GetRotation().GetAngleAroundZaxis();

  return UpdateResult::NewInfo;

} // UpdateTracking()

bool TrackFaceAction::AreContinueCriteriaMet(const f32 currentTime_sec)
{
  if (!Util::IsFltNear(_eyeContactCriteria.earliestStoppingTime_sec, -1.f) &&
      Util::IsFltGTZero(_eyeContactCriteria.noEyeContactTimeout_sec))
  {
    // Always update the time of last eye contact before we do anything else
    // TODO it would ideal to make sure we only use eye contact from the
    // face we're tracking VIC-5557
    const bool eyeContact = GetRobot().GetFaceWorld().IsMakingEyeContact(_eyeContactCriteria.eyeContactWithinLast_ms);
    if (eyeContact)
    {
      _eyeContactCriteria.timeOfLastEyeContact_sec = currentTime_sec;
    }

    // If the current time is less than the earliest stopping time,
    // we will always return true (which means we should continue
    // tracking). Once current time is larger than the earliest
    // stopping time, we apply the rest of the continue criteria.
    if (currentTime_sec < _eyeContactCriteria.earliestStoppingTime_sec)
    {
      return true;
    }
    else
    {
      if (eyeContact)
      {
        return true;
      }
      else if ((currentTime_sec - _eyeContactCriteria.timeOfLastEyeContact_sec) <= _eyeContactCriteria.noEyeContactTimeout_sec)
      {
        return true;
      }
    }
  }
  else
  {
    // We need both earliest stopping time and no eye contact timeout
    // to be provided for continue criteria to work, if one of these are
    // missing false will be returned (indicating we should stop tracking)
    PRINT_NAMED_ERROR("TrackFaceAction.AreContinueCriteriaMet.MissingContinueCriteria",
                      "Both earliest stopping time and no eye contact timeout must be provided");
  }
  return false;
}

void TrackFaceAction::SetEyeContactContinueCriteria(const f32 minTimeToTrack_sec, const f32 noEyeContactTimeout_sec,
                                                    const TimeStamp_t eyeContactWithinLast_ms)
{
  DEV_ASSERT(!HasStarted(), "ITrackAction.SetEyeContactContinueCriteria.ActionAlreadyStarted");

  // This call configures AreContinueCriteriaMet to be called
  // when determining whether to continue/stop tracking instead
  // of AreStopCriteriaMet.
  UseContinueCriteria(true);

  const auto currentTime_sec = BaseStationTimer::getInstance()->GetCurrentTimeInSeconds();
  _eyeContactCriteria.earliestStoppingTime_sec = currentTime_sec + minTimeToTrack_sec;
  _eyeContactCriteria.noEyeContactTimeout_sec = noEyeContactTimeout_sec;
  _eyeContactCriteria.eyeContactWithinLast_ms = eyeContactWithinLast_ms;
}
  
} // namespace Vector
} // namespace Anki
