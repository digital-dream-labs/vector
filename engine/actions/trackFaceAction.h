/**
 * File: trackFaceAction.h
 *
 * Author: Andrew Stein
 * Date:   12/11/2015
 *
 * Description: Defines an action for tracking (human) faces, derived from ITrackAction
 *
 *
 * Copyright: Anki, Inc. 2015
 **/

#ifndef __Anki_Cozmo_Basestation_TrackFaceAction_H__
#define __Anki_Cozmo_Basestation_TrackFaceAction_H__

#include "coretech/common/engine/robotTimeStamp.h"
#include "coretech/vision/engine/trackedFace.h"
#include "engine/actions/trackActionInterface.h"
#include "engine/smartFaceId.h"
#include "util/signals/simpleSignal_fwd.h"

namespace Anki {
namespace Vector {

class TrackFaceAction : public ITrackAction
{
public:
  
  using FaceID = Vision::FaceID_t;
  
  explicit TrackFaceAction(FaceID faceID);
  explicit TrackFaceAction(SmartFaceID faceID);
  virtual ~TrackFaceAction();

  virtual void GetCompletionUnion(ActionCompletedUnion& completionInfo) const override;
  void SetEyeContactContinueCriteria(const f32 minTimeToTrack_sec, const f32 noEyeContactTimeout_sec,
                                     const TimeStamp_t eyeContactWithinLast_ms);
  
protected:
  
  virtual void GetRequiredVisionModes(std::set<VisionModeRequest>& requests) const override;

  virtual ActionResult InitInternal() override;
  
  // Required by ITrackAction:
  virtual UpdateResult UpdateTracking(Radians& absPanAngle, Radians& absTiltAngle, f32& distance_mm) override;
  
  virtual void OnRobotSet() override final;

private:
  virtual bool AreContinueCriteriaMet(const f32 currentTime_sec) override;
  struct {
    f32     noEyeContactTimeout_sec     = 0.f;
    f32     timeOfLastEyeContact_sec    = 0.f;
    // This is the earliest time that tracking will attempt
    // to apply the other continue criteria. It will continue
    // to track if and only if the other continue criteria is
    // is satisified.
    f32     earliestStoppingTime_sec    = -1.f;
    TimeStamp_t eyeContactWithinLast_ms = 0;
  } _eyeContactCriteria;

  // store face id as non-smart until robot is accessible
  FaceID               _tmpFaceID;
  
  SmartFaceID          _faceID;
  RobotTimeStamp_t     _lastFaceUpdate = 0;

}; // class TrackFaceAction
    
} // namespace Vector
} // namespace Anki

#endif /* __Anki_Cozmo_Basestation_TrackFaceAction_H__ */
