/**
 * File: visuallyVerifyActions.h
 *
 * Author: Andrew Stein
 * Date:   6/16/2016
 *
 * Description: Actions for visually verifying the existance of objects or face.
 *              Succeeds if the robot can see the given object or face in its current pose.
 *
 *
 * Copyright: Anki, Inc. 2016
 **/

#ifndef __Anki_Cozmo_Basestation_VisuallyVerifyActions_H__
#define __Anki_Cozmo_Basestation_VisuallyVerifyActions_H__

#include "engine/actions/actionInterface.h"
#include "engine/actions/basicActions.h"
#include "engine/actions/compoundActions.h"
#include "engine/blockWorld/blockWorldFilter.h"
#include "engine/externalInterface/externalInterface.h"
#include "coretech/vision/engine/faceIdTypes.h"

#include "clad/types/actionTypes.h"
#include "clad/types/visionModes.h"

namespace Anki {
namespace Vector {

  class IVisuallyVerifyAction : public IAction
  {
  public:
    using LiftPreset = MoveLiftToHeightAction::Preset;
    
    IVisuallyVerifyAction(const std::string name,
                          const RobotActionType type,
                          VisionMode imageTypeToWaitFor,
                          LiftPreset liftPosition);
    
    virtual ~IVisuallyVerifyAction();
    
    virtual int GetNumImagesToWaitFor() const { return _numImagesToWaitFor; }
    void SetNumImagesToWaitFor(int numImages) { _numImagesToWaitFor = numImages; }
    
  protected:
    
    virtual ActionResult Init() override final;
    virtual ActionResult CheckIfDone() override final;
    
    using EngineToGameEvent = AnkiEvent<ExternalInterface::MessageEngineToGame>;
    using EngineToGameTag   = ExternalInterface::MessageEngineToGameTag;
    using EventCallback     = std::function<void(const EngineToGameEvent&)>;
    
    // Should be called in InitInternal()
    void SetupEventHandler(EngineToGameTag tag, EventCallback callback);
    
    // Derived classes should implement these:
    virtual ActionResult InitInternal() = 0;
    virtual bool HaveSeenObject() = 0;
    
  private:
    
    VisionMode            _imageTypeToWaitFor = VisionMode::Count;
    LiftPreset            _liftPreset = MoveLiftToHeightAction::Preset::LOW_DOCK;
    std::unique_ptr<ICompoundAction> _compoundAction = nullptr;
    Signal::SmartHandle   _observationHandle;
    int                   _numImagesToWaitFor = 10;

  }; // class IVisuallyVerifyAction
  
  
  class VisuallyVerifyObjectAction : public IVisuallyVerifyAction
  {
  public:
    VisuallyVerifyObjectAction(ObjectID objectID,
                               Vision::Marker::Code whichCode = Vision::Marker::ANY_CODE);
    
    virtual ~VisuallyVerifyObjectAction();
    
    // When called, this will cause the action to use the "cycling exposure" vision mode when looking for the object.
    // This is useful for more robustly verifying an object under adverse lighting conditions, for example. Note that
    // this will also set NumImagesToWaitFor to a value appropriate for cycling exposure mode.
    // Note: This must be called before the action is started.
    void SetUseCyclingExposure();
    
  protected:
    virtual void GetRequiredVisionModes(std::set<VisionModeRequest>& requests) const override;
    virtual ActionResult InitInternal() override;
    virtual bool HaveSeenObject() override;
    
    ObjectID                _objectID;
    Vision::Marker::Code    _whichCode;
    bool                    _objectSeen = false;
    bool                    _markerSeen = false;
    bool                    _useCyclingExposure = false;
    
  }; // class VisuallyVerifyObjectAction
  
  
  class VisuallyVerifyFaceAction : public IVisuallyVerifyAction
  {
  public:
    VisuallyVerifyFaceAction(Vision::FaceID_t faceID);
    
    virtual ~VisuallyVerifyFaceAction();
    
  protected:
    virtual void GetRequiredVisionModes(std::set<VisionModeRequest>& requests) const override;
    virtual ActionResult InitInternal() override;
    virtual bool HaveSeenObject() override;
    
    Vision::FaceID_t        _faceID;
    bool                    _faceSeen = false;
    
  }; // class VisuallyVerifyFaceAction
  
  
  class VisuallyVerifyNoObjectAtPoseAction : public IAction
  {
  public:
    VisuallyVerifyNoObjectAtPoseAction(const Pose3d& pose,
                                       const Point3f& thresholds_mm = {10, 10, 10});
    virtual ~VisuallyVerifyNoObjectAtPoseAction();
    
    void AddIgnoreID(const ObjectID& objID);
    
  protected:
    virtual void GetRequiredVisionModes(std::set<VisionModeRequest>& requests) const override;
    virtual ActionResult Init() override;
    virtual ActionResult CheckIfDone() override;
    
  private:
    std::unique_ptr<ICompoundAction> _turnTowardsPoseAction = nullptr;
    std::unique_ptr<IAction>         _waitForImagesAction   = nullptr;
    Pose3d                _pose;
    Point3f               _thresholds_mm;
    int                   _numImagesToWaitFor = 10;
    
    BlockWorldFilter      _filter;
  };
  
} // namespace Vector
} // namespace Anki

#endif /* __Anki_Cozmo_Basestation_VisuallyVerifyActions_H__ */
