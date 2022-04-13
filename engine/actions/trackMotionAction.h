/**
 * File: trackMotionAction.h
 *
 * Author: Andrew Stein
 * Date:   12/11/2015
 *
 * Description: Defines an action for tracking motion (on the ground), derived from ITrackAction.
 *
 *
 * Copyright: Anki, Inc. 2015
 **/

#ifndef __Anki_Cozmo_Basestation_TrackMotionAction_H__
#define __Anki_Cozmo_Basestation_TrackMotionAction_H__

#include "engine/actions/trackActionInterface.h"

#include "clad/externalInterface/messageEngineToGame.h"

#include "util/signals/simpleSignal_fwd.h"

namespace Anki {
namespace Vector {

class TrackMotionAction : public ITrackAction
{
public:
  
  TrackMotionAction() : ITrackAction("TrackMotion", RobotActionType::TRACK_MOTION) { }
  
protected:

  virtual void GetRequiredVisionModes(std::set<VisionModeRequest>& requests) const override;

  virtual ActionResult InitInternal() override;
  
  // Required by ITrackAction:
  virtual UpdateResult UpdateTracking(Radians& absPanAngle, Radians& absTiltAngle, f32& distance_mm) override;
  
private:
  
  bool _gotNewMotionObservation = false;
  
  ExternalInterface::RobotObservedMotion _motionObservation;
  
  Signal::SmartHandle _signalHandle;
  
}; // class TrackMotionAction
    
} // namespace Vector
} // namespace Anki

#endif /* __Anki_Cozmo_Basestation_TrackMotionAction_H__ */
