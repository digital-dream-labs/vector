/**
 * File: trackObjectAction.cpp
 *
 * Author: Andrew Stein
 * Date:   12/11/2015
 *
 * Description: Defines an action for tracking objects (from BlockWorld), derived from ITrackAction
 *
 *
 * Copyright: Anki, Inc. 2015
 **/


#include "engine/actions/trackObjectAction.h"
#include "engine/blockWorld/blockWorld.h"
#include "engine/blockWorld/blockWorldFilter.h"
#include "engine/components/movementComponent.h"
#include "engine/externalInterface/externalInterface.h"
#include "engine/robot.h"

#include "clad/externalInterface/messageEngineToGameTag.h"
#include "clad/externalInterface/messageEngineToGame.h"

#define DEBUG_TRACKING_ACTIONS 0

namespace Anki {
namespace Vector {
  
static const char * const kLogChannelName = "Actions";
  
TrackObjectAction::TrackObjectAction(const ObjectID& objectID, bool trackByType)
: ITrackAction("TrackObject",
               RobotActionType::TRACK_OBJECT)
, _objectID(objectID)
, _trackByType(trackByType)
{
  SetName("TrackObject" + std::to_string(_objectID));
}
  
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
TrackObjectAction::~TrackObjectAction()
{
  if(HasRobot()){
    GetRobot().GetMoveComponent().UnSetTrackToObject(); 
  }   
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void TrackObjectAction::GetRequiredVisionModes(std::set<VisionModeRequest>& requests) const
{
  requests.insert({ VisionMode::Markers, EVisionUpdateFrequency::High });
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
ActionResult TrackObjectAction::InitInternal()
{
  if(!_objectID.IsSet()) {
    PRINT_NAMED_ERROR("TrackObjectAction.Init.ObjectIdNotSet", "");
    return ActionResult::BAD_OBJECT;
  }
  
  const ObservableObject* object = GetRobot().GetBlockWorld().GetLocatedObjectByID(_objectID);
  if(nullptr == object) {
    PRINT_NAMED_ERROR("TrackObjectAction.Init.InvalidObject",
                      "Object %d does not exist in BlockWorld", _objectID.GetValue());
    return ActionResult::BAD_OBJECT;
  }
  
  _objectType = object->GetType();
  if(_trackByType) {
    SetName("TrackObject" + std::string(EnumToString(_objectType)));
  }
  
  _lastTrackToPose = object->GetPose();
  
  GetRobot().GetMoveComponent().SetTrackToObject(_objectID);
  
  return ActionResult::SUCCESS;
} // InitInternal()
  
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
ITrackAction::UpdateResult TrackObjectAction::UpdateTracking(Radians& absPanAngle, Radians& absTiltAngle, f32& distance_mm)
{
  ObservableObject* matchingObject = nullptr;
  
  if(_trackByType) {
    BlockWorldFilter filter;
    filter.AddFilterFcn([this](const ObservableObject* obj){
      return (obj->GetLastObservedTime() == GetRobot().GetLastImageTimeStamp());
    });
    
    matchingObject = GetRobot().GetBlockWorld().FindLocatedClosestMatchingObject(_objectType, _lastTrackToPose, 1000.f, DEG_TO_RAD(180), filter);
    
    if(nullptr == matchingObject) {
      // Did not see an object of the right type during latest blockworld update
      if(DEBUG_TRACKING_ACTIONS)
      {
        PRINT_CH_INFO(kLogChannelName, "TrackObjectAction.UpdateTracking.NoMatchingTypeFound",
                      "Could not find matching %s object.",
                      EnumToString(_objectType));
      }
      return UpdateResult::NoNewInfo;
    } else {
      // We've possibly switched IDs that we're tracking. Keep MovementComponent's ID in sync.
      GetRobot().GetMoveComponent().SetTrackToObject(matchingObject->GetID());
    }
  } else {
    matchingObject = GetRobot().GetBlockWorld().GetLocatedObjectByID(_objectID);
    if(nullptr == matchingObject) {
      PRINT_NAMED_WARNING("TrackObjectAction.UpdateTracking.ObjectNoLongerExists",
                          "Object %d no longer exists in BlockWorld",
                          _objectID.GetValue());
      return UpdateResult::NoNewInfo;
    }
  }
  
  DEV_ASSERT(nullptr != matchingObject, "TrackObjectAction.UpdateTracking.NullMatchingObject");
  
  _lastTrackToPose = matchingObject->GetPose();
  
  // Find the observed marker closest to the robot and use that as the one we
  // track to
  std::vector<const Vision::KnownMarker*> observedMarkers;
  matchingObject->GetObservedMarkers(observedMarkers, matchingObject->GetLastObservedTime());
  
  if(observedMarkers.empty()) {
    PRINT_NAMED_ERROR("TrackObjectAction.UpdateTracking.NoObservedMarkers",
                      "No markers on observed object %d marked as observed since time %d, "
                      "expecting at least one.",
                      matchingObject->GetID().GetValue(),
                      matchingObject->GetLastObservedTime());
    return UpdateResult::NoNewInfo;
  }
  
  const Vision::KnownMarker* closestMarker = nullptr;
  f32 minDistSq = std::numeric_limits<f32>::max();
  f32 xDist = 0.f, yDist = 0.f, zDist = 0.f;
  
  for(auto marker : observedMarkers) {
    Pose3d markerPoseWrtRobot;
    if(false == marker->GetPose().GetWithRespectTo(GetRobot().GetPose(), markerPoseWrtRobot)) {
      PRINT_NAMED_ERROR("TrackObjectAction.UpdateTracking.PoseOriginError",
                        "Could not get pose of observed marker w.r.t. robot");
      return UpdateResult::NoNewInfo;
    }
    
    const f32 xDist_crnt = markerPoseWrtRobot.GetTranslation().x();
    const f32 yDist_crnt = markerPoseWrtRobot.GetTranslation().y();
    
    const f32 currentDistSq = xDist_crnt*xDist_crnt + yDist_crnt*yDist_crnt;
    if(currentDistSq < minDistSq) {
      closestMarker = marker;
      minDistSq = currentDistSq;
      xDist = xDist_crnt;
      yDist = yDist_crnt;
      
      // Keep track of best zDist too, so we don't have to redo the GetWithRespectTo call outside this loop
      // NOTE: This isn't perfectly accurate since it doesn't take into account the
      // the head angle and is simply using the neck joint (which should also
      // probably be queried from the robot instead of using the constant here)
      zDist = markerPoseWrtRobot.GetTranslation().z() - NECK_JOINT_POSITION[2];
    }
    
  } // For all markers
  
  if(closestMarker == nullptr) {
    PRINT_NAMED_ERROR("TrackObjectAction.UpdateTracking.NoClosestMarker", "");
    return UpdateResult::NoNewInfo;
  }
  
  DEV_ASSERT(minDistSq > 0.f, "Distance to closest marker should be > 0");
  
  absTiltAngle = std::atan(zDist/std::sqrt(minDistSq));
  absPanAngle  = std::atan2(yDist, xDist) + GetRobot().GetPose().GetRotation().GetAngleAroundZaxis();
  
  return UpdateResult::NewInfo;
  
} // UpdateTracking()

  
} // namespace Vector
} // namespace Anki
