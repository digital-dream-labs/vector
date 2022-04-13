/**
 * File: preActionPose.h
 *
 * Author: Andrew Stein
 * Date:   7/9/2014
 *
 * Description: Defines a "Pre-Action" pose, which is used by ActionableObjects
 *              to define a position to be in before acting on the object with a 
 *              given type of ActionType.
 *
 *
 * Copyright: Anki, Inc. 2014
 **/

#ifndef ANKI_COZMO_BASESTATION_PREACTIONPOSE_H
#define ANKI_COZMO_BASESTATION_PREACTIONPOSE_H

#include "coretech/common/engine/math/pose.h"

namespace Anki {
  
  // Forward declarations:
  class ColorRGBA;
  namespace Vision {
    class KnownMarker;
  }
  
  namespace Vector {
    
    class PreActionPose
    {
    public:
      enum ActionType {
        DOCKING,         // for picking up a specified object
        PLACE_RELATIVE,  // for placing a carried object on top of / in front of the specified object
        PLACE_ON_GROUND, // for putting a carried object down
        ENTRY,           // for entering a bridge or ascending/descending a ramp
        ROLLING,         // for rolling a block towards the robot
        FLIPPING,        // for flipping a block
        
        // Add new ActionTypes here
        
        NONE
      };
      
      // Simple case: pose is along the normal to the marker, at the given distance
      // (Aligned with center of marker)
      PreActionPose(ActionType type,
                    const Vision::KnownMarker* marker,
                    const f32 distance,
                    const f32 length_mm);
      
      // Pose is aligned with normal (facing the marker), but offset by the given
      // vector. Note that a shift along the negative Y axis is equivalent to
      // the simple case above. (The marker is in the X-Z plane.
      PreActionPose(ActionType type,
                    const Vision::KnownMarker* marker,
                    const Vec3f& offset,
                    const f32 length_mm);
      
      // Specify arbitrary position relative to marker
      // poseWrtMarker's parent should be the marker's pose.
      PreActionPose(ActionType type,
                    const Vision::KnownMarker* marker,
                    const Pose3d&  poseWrtMarker,
                    const f32 length_mm);
      
      // For creating a pre-action pose in its current position, given the
      // canonical pre-action pose and the currennt pose of its marker's
      // parent. Probably not generally useful, but used by ActionableObject.
      PreActionPose(const PreActionPose& canonicalPose,
                    const Pose3d& markerParentPose,
                    const f32 length_mm,
                    const f32 offset_mm);

      // Copy constructor
      PreActionPose(const PreActionPose& other);
      
      // Get the type of action associated with this PreActionPose
      ActionType GetActionType() const;
      
      // Get marker associated with thise PreActionPose
      const Vision::KnownMarker* GetMarker() const;
      
      // Get the current PreActionPose, given the current pose of the
      // its marker's parent.
      //Result GetCurrentPose(const Pose3d& markerParentPose,
      //                    Pose3d& currentPose) const;
      
      // Get the Code of the Marker this PreActionPose is "attached" to.
      //const Vision::Marker::Code& GetMarkerCode() const;
      
      // Get PreActionPose w.r.t. the parent of marker it is "attached" to. It is
      // the caller's responsibility to make it w.r.t. the world origin
      // (or other pose) if desired.
      const Pose3d& GetPose() const; // w.r.t. marker's parent!
      
      constexpr static const f32 ANGLE_TOLERANCE = DEG_TO_RAD(30);
      f32 GetHeightTolerance() const { return _heightTolerance; }
      
      static const ColorRGBA& GetVisualizeColor(ActionType type);
      
      f32 GetLineLength() const { return _preActionPoseLineLength_mm; }
      
    protected:
      
      void SetHeightTolerance();
      
      ActionType   _type;
      
      const Vision::KnownMarker* _marker;
      
      Pose3d _poseWrtMarkerParent;
      
      f32     _heightTolerance;
      
      // Length of the preActionLine, this is a line extending away from
      // _poseWrtMarkerParent on which the preActionPose can fall
      f32     _preActionPoseLineLength_mm;
      
    }; // class PreActionPose
    
    
#pragma mark ---- Inlined Implementations ----
    
    inline PreActionPose::ActionType PreActionPose::GetActionType() const {
      return _type;
    }
    
    inline const Vision::KnownMarker* PreActionPose::GetMarker() const {
      return _marker;
    }
    
    inline const Pose3d& PreActionPose::GetPose() const {
      return _poseWrtMarkerParent;
    }
    
    
  } // namespace Vector
} // namespace Anki

#endif // ANKI_COZMO_BASESTATION_PREACTIONPOSE_H
