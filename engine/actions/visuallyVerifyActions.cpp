/**
 * File: visuallyVerifyActions.cpp
 *
 * Author: Andrew Stein
 * Date:   6/16/2016
 *
 * Description: Actions for visually verifying the existance of objects or face.
 *
 *
 * Copyright: Anki, Inc. 2016
 **/

#include "engine/actions/visuallyVerifyActions.h"
#include "clad/externalInterface/messageEngineToGame.h"
#include "engine/blockWorld/blockWorld.h"
#include "engine/drivingAnimationHandler.h"
#include "engine/externalInterface/externalInterface.h"
#include "engine/robot.h"

#define LOG_CHANNEL "Actions"

namespace Anki {
namespace Vector {
  
#pragma mark - 
#pragma mark IVisuallyVerifyAction
  
  IVisuallyVerifyAction::IVisuallyVerifyAction(const std::string name,
                                               const RobotActionType type,
                                               VisionMode imageTypeToWaitFor,
                                               LiftPreset liftPosition)
  : IAction(name,
            type,
            (u8)AnimTrackFlag::HEAD_TRACK)
  , _imageTypeToWaitFor(imageTypeToWaitFor)
  , _liftPreset(liftPosition)
  {
    
  }
  
  IVisuallyVerifyAction::~IVisuallyVerifyAction()
  {
    if(nullptr != _compoundAction) {
      _compoundAction->PrepForCompletion();
    }
  }
  
  ActionResult IVisuallyVerifyAction::Init()
  {
    _compoundAction.reset(new CompoundActionParallel({
      new MoveLiftToHeightAction(_liftPreset),
      new WaitForImagesAction(GetNumImagesToWaitFor(), _imageTypeToWaitFor),
    }));
    _compoundAction->SetRobot(&GetRobot());
    _compoundAction->ShouldSuppressTrackLocking(true);

    return InitInternal();
  }
  
  void IVisuallyVerifyAction::SetupEventHandler(EngineToGameTag tag, EventCallback callback)
  {
    _observationHandle = GetRobot().GetExternalInterface()->Subscribe(tag, callback);      
  }
  
  ActionResult IVisuallyVerifyAction::CheckIfDone()
  {
    if(HaveSeenObject()) {
      // Saw what we're looking for!
      return ActionResult::SUCCESS;
    }
    
    // Keep waiting for lift to get out of the way and number of images to come in
    const ActionResult compoundResult = _compoundAction->Update();
    if(ActionResult::RUNNING != compoundResult)
    {
      LOG_INFO("IVisuallyVerifyAction.CheckIfDone.TimedOut",
               "%s: Did not see object before processing %d images",
               GetName().c_str(), GetNumImagesToWaitFor());
      
      return ActionResult::VISUAL_OBSERVATION_FAILED;
    }
    
    return ActionResult::RUNNING;
  }
  
#pragma mark -
#pragma mark VisuallyVerifyObjectAction
  
VisuallyVerifyObjectAction::VisuallyVerifyObjectAction(ObjectID objectID,
                                                       Vision::Marker::Code whichCode)
  : IVisuallyVerifyAction("VisuallyVerifyObject" + std::to_string(objectID.GetValue()),
                          RobotActionType::VISUALLY_VERIFY_OBJECT,
                          VisionMode::Markers,
                          LiftPreset::OUT_OF_FOV)
, _objectID(objectID)
, _whichCode(whichCode)
{
  
}

VisuallyVerifyObjectAction::~VisuallyVerifyObjectAction()
{

}

void VisuallyVerifyObjectAction::SetUseCyclingExposure()
{
  _useCyclingExposure = true;

  // The CyclingExposure mode cycles exposures every 5 frames, with a cycle length of 3. Therefore, wait for 15 images.
  // Note: This should be computed directly from the vision config instead (VIC-12803)
  const int kNumImagesToWaitFor = 15;
  SetNumImagesToWaitFor(kNumImagesToWaitFor);
}

void VisuallyVerifyObjectAction::GetRequiredVisionModes(std::set<VisionModeRequest>& requests) const
{
  requests.insert({ VisionMode::Markers, EVisionUpdateFrequency::High });
  if (_useCyclingExposure) {
    requests.insert({ VisionMode::AutoExp_Cycling, EVisionUpdateFrequency::High });
  }
}

ActionResult VisuallyVerifyObjectAction::InitInternal()
{
  using namespace ExternalInterface;
  
  _objectSeen = false;
  
  auto obsObjLambda = [this](const AnkiEvent<MessageEngineToGame>& event)
  {
    const auto& objectObservation = event.GetData().Get_RobotObservedObject();
    // ID has to match and we have to actually have seen a marker (not just
    // saying part of the object is in FOV due to assumed projection)
    if(!_objectSeen && objectObservation.objectID == _objectID)
    {
      _objectSeen = true;
    }
  };
  
  SetupEventHandler(MessageEngineToGameTag::RobotObservedObject, obsObjLambda);
  
  if(_whichCode == Vision::Marker::ANY_CODE) {
    _markerSeen = true;
  } else {
    _markerSeen = false;
  }
  
  return ActionResult::SUCCESS;
}

bool VisuallyVerifyObjectAction::HaveSeenObject()
{
  if(_objectSeen)
  {
    if(!_markerSeen)
    {
      // We've seen the object, check if we've seen the correct marker if one was
      // specified and we haven't seen it yet
      ObservableObject* object = GetRobot().GetBlockWorld().GetLocatedObjectByID(_objectID);
      if(object == nullptr) {
        PRINT_NAMED_WARNING("VisuallyVerifyObjectAction.HaveSeenObject.ObjectNotFound",
                            "[%d] Object with ID=%d no longer exists in the world.",
                            GetTag(),
                            _objectID.GetValue());
        return false;
      }
      
      // Look for which markers were seen since (and including) last observation time
      std::vector<const Vision::KnownMarker*> observedMarkers;
      object->GetObservedMarkers(observedMarkers, object->GetLastObservedTime());
      
      for(auto marker : observedMarkers) {
        if(marker->GetCode() == _whichCode) {
          _markerSeen = true;
          break;
        }
      }
      
      if(!_markerSeen) {
        // Seeing wrong marker(s). Log this for help in debugging
        std::string observedMarkerNames;
        for(auto marker : observedMarkers) {
          observedMarkerNames += marker->GetCodeName();
          observedMarkerNames += " ";
        }
        
        LOG_INFO("VisuallyVerifyObjectAction.HaveSeenObject.WrongMarker",
                 "[%d] Have seen object %d, but not marker code %d. Have seen: %s",
                 GetTag(), _objectID.GetValue(), _whichCode, observedMarkerNames.c_str());
      }
    } // if(!_markerSeen)
    
    if(_markerSeen) {
      // We've seen the object and the correct marker: we're good to go!
      return true;
    }
    
  }
  
  return false;
  
} // VisuallyVerifyObjectAction::HaveSeenObject()

#pragma mark -
#pragma mark VisuallyVerifyFaceAction

VisuallyVerifyFaceAction::VisuallyVerifyFaceAction(Vision::FaceID_t faceID)
: IVisuallyVerifyAction("VisuallyVerifyFace" + std::to_string(faceID),
                        RobotActionType::VISUALLY_VERIFY_FACE,
                        VisionMode::Faces,
                        LiftPreset::LOW_DOCK)
, _faceID(faceID)
{
  
}

VisuallyVerifyFaceAction::~VisuallyVerifyFaceAction()
{
  
}

void VisuallyVerifyFaceAction::GetRequiredVisionModes(std::set<VisionModeRequest>& requests) const
{
  requests.insert({ VisionMode::Faces, EVisionUpdateFrequency::High });
}

ActionResult VisuallyVerifyFaceAction::InitInternal()
{
  using namespace ExternalInterface;
  
  _faceSeen = false;
  
  auto obsFaceLambda = [this](const AnkiEvent<MessageEngineToGame>& event)
  {
    if(!_faceSeen)
    {
      if(_faceID == Vision::UnknownFaceID)
      {
        // Happy to see _any_ face
        _faceSeen = true;
      }
      else if(event.GetData().Get_RobotObservedFace().faceID == _faceID)
      {
        _faceSeen = true;
      }
    }
  };
  
  SetupEventHandler(MessageEngineToGameTag::RobotObservedFace, obsFaceLambda);
  
  return ActionResult::SUCCESS;
}

bool VisuallyVerifyFaceAction::HaveSeenObject()
{
  return _faceSeen;
}

#pragma mark -
#pragma mark VisuallyVerifyNoObjectAtPoseAction

VisuallyVerifyNoObjectAtPoseAction::VisuallyVerifyNoObjectAtPoseAction(const Pose3d& pose,
                                                                       const Point3f& thresholds_mm)
: IAction("VisuallyVerifyNoObjectAtPose",
          RobotActionType::VISUALLY_VERIFY_NO_OBJECT_AT_POSE,
          (u8)AnimTrackFlag::HEAD_TRACK | (u8)AnimTrackFlag::BODY_TRACK)
, _pose(pose)
, _thresholds_mm(thresholds_mm)
{
  std::string name = "VisuallyVerifyNoObjectAtPose(";
  name += std::to_string((int)_pose.GetTranslation().x()) + ",";
  name += std::to_string((int)_pose.GetTranslation().y()) + ",";
  name += std::to_string((int)_pose.GetTranslation().z()) + ")";
  SetName(name);
  
  // Augment the default filter (object not in unknown pose state) with one that
  // checks that this object was observed in the last frame
  _filter.AddFilterFcn([this](const ObservableObject* object)
                       {
                         if(object->GetLastObservedTime() >= GetRobot().GetLastImageTimeStamp())
                         {
                           return true;
                         }
                         return false;
                       });

}

VisuallyVerifyNoObjectAtPoseAction::~VisuallyVerifyNoObjectAtPoseAction()
{
  if(_turnTowardsPoseAction != nullptr)
  {
    _turnTowardsPoseAction->PrepForCompletion();
  }
  
  if(_waitForImagesAction != nullptr)
  {
    _waitForImagesAction->PrepForCompletion();
  }
}

void VisuallyVerifyNoObjectAtPoseAction::GetRequiredVisionModes(std::set<VisionModeRequest>& requests) const
{
  requests.insert({ VisionMode::Markers, EVisionUpdateFrequency::High });
}

ActionResult VisuallyVerifyNoObjectAtPoseAction::Init()
{
  // Turn towards the pose and move the lift out of the way while we turn
  // then wait for a number of images
  _turnTowardsPoseAction.reset(new CompoundActionParallel({
    new TurnTowardsPoseAction(_pose),
    new MoveLiftToHeightAction(MoveLiftToHeightAction::Preset::OUT_OF_FOV)
  }));
  _turnTowardsPoseAction->SetRobot(&GetRobot());
  if (_waitForImagesAction != nullptr) {
    _waitForImagesAction->PrepForCompletion();
  }
  _waitForImagesAction.reset(new WaitForImagesAction(_numImagesToWaitFor, VisionMode::Markers));
  _waitForImagesAction->SetRobot(&GetRobot());

  _turnTowardsPoseAction->ShouldSuppressTrackLocking(true);
  
  return ActionResult::SUCCESS;
}

void VisuallyVerifyNoObjectAtPoseAction::AddIgnoreID(const ObjectID& objID)
{
  if (HasStarted()) {
    // You're too late! Set objects to ignore before you start the action!
    PRINT_NAMED_WARNING("VisuallyVerifyNoObjectAtPoseAciton.AddIgnoreID.ActionAlreadyStarted", "");
  } else {
    _filter.AddIgnoreID(objID);
  }
}

ActionResult VisuallyVerifyNoObjectAtPoseAction::CheckIfDone()
{
  // Tick the turnTowardsPoseAction first until it completes and then delete it
  if(_turnTowardsPoseAction != nullptr)
  {
    ActionResult res = _turnTowardsPoseAction->Update();
    if(res != ActionResult::SUCCESS)
    {
      return res;
    }
    
    _turnTowardsPoseAction->PrepForCompletion();
    _turnTowardsPoseAction.reset();
    
    return ActionResult::RUNNING;
  }
  // If the turnTowardsPoseAction is null then it must have completed so tick the waitForImagesAction
  // if it succeeds then that means we went _numImagesToWaitFor without seeing an object close to _pose so
  // this action will succeed
  else if(_waitForImagesAction != nullptr)
  {
    ActionResult res = _waitForImagesAction->Update();
    
    // If there is an object at the given pose within the threshold then fail
    // Only do this check once we have turned towards the pose and have started waiting for images in case
    // there isn't actually an object at the pose but blockworld thinks there is
    if(GetRobot().GetBlockWorld().FindLocatedObjectClosestTo(_pose, _thresholds_mm, _filter) != nullptr)
    {
      LOG_DEBUG("VisuallyVerifyNoObjectAtPose.FoundObject",
                "Seeing object near pose (%f %f %f)",
                _pose.GetTranslation().x(),
                _pose.GetTranslation().y(),
                _pose.GetTranslation().z());
      return ActionResult::VISUAL_OBSERVATION_FAILED;
    }
    
    return res;
  }
  
  PRINT_NAMED_WARNING("VisuallyVerifyNoObjectAtPoseAction.NullSubActions",
                      "Both subActions are null returning failure");
  
  return ActionResult::NULL_SUBACTION;
}


  
} // namespace Vector
} // namesace Anki
