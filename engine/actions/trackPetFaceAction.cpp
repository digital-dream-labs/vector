/**
 * File: trackPetFaceAction.cpp
 *
 * Author: Andrew Stein
 * Date:   12/11/2015
 *
 * Description: Defines an action for tracking pet faces, derived from ITrackAction.
 *
 *
 * Copyright: Anki, Inc. 2015
 **/


#include "engine/actions/trackPetFaceAction.h"
#include "engine/petWorld.h"
#include "engine/robot.h"

namespace Anki {
namespace Vector {
  
static const char * const kLogChannelName = "Actions";
  
TrackPetFaceAction::TrackPetFaceAction(FaceID faceID)
: ITrackAction("TrackPetFace" + std::to_string(faceID),
               RobotActionType::TRACK_PET_FACE)
, _faceID(faceID)
{

}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
TrackPetFaceAction::TrackPetFaceAction(Vision::PetType petType)
: ITrackAction("TrackPetFace",
               RobotActionType::TRACK_PET_FACE)
, _petType(petType)
{
  switch(_petType)
  {
    case Vision::PetType::Cat:
      SetName("TrackCatFace");
      break;
   
    case Vision::PetType::Dog:
      SetName("TrackDogFace");
      break;
      
    case Vision::PetType::Unknown:
      SetName("TrackAnyPetFace");
      break;
  }
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void TrackPetFaceAction::GetRequiredVisionModes(std::set<VisionModeRequest>& requests) const
{
  requests.insert({ VisionMode::Pets, EVisionUpdateFrequency::Med });
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
ActionResult TrackPetFaceAction::InitInternal()
{
  _lastFaceUpdate = 0;
  
  return ActionResult::SUCCESS;
} // InitInternal()

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void TrackPetFaceAction::GetCompletionUnion(ActionCompletedUnion& completionUnion) const
{
  TrackFaceCompleted completion;
  completion.faceID = static_cast<s32>(_faceID);
  completionUnion.Set_trackFaceCompleted(std::move(completion));
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
ITrackAction::UpdateResult TrackPetFaceAction::UpdateTracking(Radians& absPanAngle, Radians& absTiltAngle, f32& distance_mm)
{
  const Vision::TrackedPet* petFace = nullptr;
  
  if(_faceID != Vision::UnknownFaceID)
  {
    petFace = GetRobot().GetPetWorld().GetPetByID(_faceID);
    
    if(nullptr == petFace)
    {
      // No such face
      PRINT_CH_INFO(kLogChannelName, "TrackPetFaceAction.UpdateTracking.BadFaceID",
                    "No face %d in PetWorld", _faceID);
      return UpdateResult::NoNewInfo;
    }
  }
  else
  {
    auto petIDs = GetRobot().GetPetWorld().GetKnownPetsWithType(_petType);
    if(!petIDs.empty())
    {
      petFace = GetRobot().GetPetWorld().GetPetByID(*petIDs.begin());
    }
    else
    {
      PRINT_CH_INFO(kLogChannelName, "TrackPetFaceAction.UpdateTracking.NoPetsWithType", "Type=%s",
                    EnumToString(_petType));
      return UpdateResult::NoNewInfo;
    }
  }

  // Only update pose if we've actually observed the face again since last update
  if(petFace->GetTimeStamp() <= _lastFaceUpdate) {
    return UpdateResult::NoNewInfo;
  }
  _lastFaceUpdate = petFace->GetTimeStamp();
  
  Result result = GetRobot().ComputeTurnTowardsImagePointAngles(petFace->GetRect().GetMidPoint(), petFace->GetTimeStamp(),
                                                            absPanAngle, absTiltAngle);
  if(RESULT_OK != result)
  {
    PRINT_NAMED_WARNING("TrackpetFaceAction.UpdateTracking.ComputeTurnTowardsImagePointAnglesFailed", "t=%u", petFace->GetTimeStamp());
    return UpdateResult::NoNewInfo;
  }
  
  return UpdateResult::NewInfo;

} // UpdateTracking()

} // namespace Vector
} // namespace Anki
