/**
 * File: trackPetFaceAction.h
 *
 * Author: Andrew Stein
 * Date:   12/11/2015
 *
 * Description: Defines an action for tracking pet faces, derived from ITrackAction.
 *
 *
 * Copyright: Anki, Inc. 2015
 **/

#ifndef __Anki_Cozmo_Basestation_TrackPetFaceAction_H__
#define __Anki_Cozmo_Basestation_TrackPetFaceAction_H__

#include "engine/actions/trackActionInterface.h"
#include "coretech/common/engine/robotTimeStamp.h"
#include "coretech/vision/engine/faceIdTypes.h"

#include "clad/types/petTypes.h"

namespace Anki {
namespace Vector {

class TrackPetFaceAction : public ITrackAction
{
public:

  using FaceID = Vision::FaceID_t;
  
  // Track a specific pet ID
  TrackPetFaceAction(FaceID faceID);
  
  // Track first pet with the right type (or any pet at all if PetType set to Unknown).
  // Note the pet being tracked could change during tracking as it is the first one
  // found in PetWorld on each update tick.
  TrackPetFaceAction(Vision::PetType petType);
  
  virtual void GetCompletionUnion(ActionCompletedUnion& completionInfo) const override;
  
protected:
  
  virtual void GetRequiredVisionModes(std::set<VisionModeRequest>& requests) const override;
  
  virtual ActionResult InitInternal() override;
  
  // Required by ITrackAction:
  virtual UpdateResult UpdateTracking(Radians& absPanAngle, Radians& absTiltAngle, f32& distance_mm) override;
  
private:
  
  FaceID             _faceID  = Vision::UnknownFaceID;
  Vision::PetType    _petType = Vision::PetType::Unknown;
  RobotTimeStamp_t   _lastFaceUpdate = 0;
  
}; // class TrackPetFaceAction
    
} // namespace Vector
} // namespace Anki

#endif /* __Anki_Cozmo_Basestation_TrackPetFaceAction_H__ */
