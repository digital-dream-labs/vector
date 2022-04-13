/**
 * File: preActionPose.cpp
 *
 * Author: Andrew Stein
 * Date:   7/9/2014
 *
 * Description: Implements a "Pre-Action" pose, which is used by ActionableObjects
 *              to define a position to be in before acting on the object with a
 *              given type of ActionType.
 *
 *
 * Copyright: Anki, Inc. 2014
 **/

#include "engine/preActionPose.h"

#include "coretech/common/engine/colorRGBA.h"
#include "coretech/vision/engine/visionMarker.h"

#include "coretech/common/engine/math/quad.h"
 
namespace Anki {
  
  namespace Vector {
    
    const ColorRGBA& PreActionPose::GetVisualizeColor(ActionType type)
    {
      static const std::map<ActionType, ColorRGBA> ColorLUT = {
        {DOCKING,         ColorRGBA(0.0f,0.0f,1.0f,0.5f)},
        {PLACE_RELATIVE,  ColorRGBA(0.0f,0.8f,0.2f,0.5f)},
        {PLACE_ON_GROUND, ColorRGBA(0.5f,0.5f,0.0f,0.5f)},
        {ENTRY,           ColorRGBA(1.f,0.f,0.f,0.5f)},
        {FLIPPING,        ColorRGBA(0.5f,0.f,0.5f,0.5f)}
      };
      
      static const ColorRGBA Default(1.0f,0.0f,0.0f,0.5f);
      
      auto iter = ColorLUT.find(type);
      if(iter == ColorLUT.end()) {
        PRINT_NAMED_WARNING("PreActionPose.GetVisualizationColor.ColorNotDefined",
                            "Color not defined for ActionType=%d. Returning default color.", type);
        return Default;
      } else {
        return iter->second;
      }
    } // GetVisualizeColor()
    
    void PreActionPose::SetHeightTolerance()
    {
      const Point3f T = _poseWrtMarkerParent.GetTranslation().GetAbs();
      const f32 distance = std::max(T.x(), std::max(T.y(), T.z()));
      _heightTolerance = distance * std::tan(ANGLE_TOLERANCE);
    }
    
    PreActionPose::PreActionPose(ActionType type,
                                 const Vision::KnownMarker* marker,
                                 const f32 distance,
                                 const f32 length_mm)
    : PreActionPose(type, marker, Y_AXIS_3D() * -distance, length_mm)
    {
      
    } // PreActionPose Constructor
    
    
    PreActionPose::PreActionPose(ActionType type,
                                 const Vision::KnownMarker* marker,
                                 const Vec3f& offset,
                                 const f32 length_mm)
    : _type(type)
    , _marker(marker)
    , _poseWrtMarkerParent(M_PI_2, Z_AXIS_3D(), offset, marker->GetPose()) // init w.r.t. marker
    , _preActionPoseLineLength_mm(length_mm)
    {
      // Now make pose w.r.t. marker parent
      if(_poseWrtMarkerParent.GetWithRespectTo(_marker->GetPose().GetParent(), _poseWrtMarkerParent) == false) {
        PRINT_NAMED_ERROR("PreActionPose.GetPoseWrtMarkerParentFailed",
                          "Could not get the pre-action pose w.r.t. the marker's parent.");
      }
      _poseWrtMarkerParent.SetName("PreActionPose");
      
      SetHeightTolerance();
      
    } // PreActionPose Constructor
    
    
    PreActionPose::PreActionPose(ActionType type,
                                 const Vision::KnownMarker* marker,
                                 const Pose3d& poseWrtMarker,
                                 const f32 length_mm)
    : _type(type)
    , _marker(marker)
    , _preActionPoseLineLength_mm(length_mm)
    {
      if(!poseWrtMarker.IsChildOf(marker->GetPose())) {
        PRINT_NAMED_ERROR("PreActionPose.PoseWrtMarkerParentInvalid",
                          "Given pose w.r.t. marker should have the marker as its parent pose.");
      }
      if(poseWrtMarker.GetWithRespectTo(_marker->GetPose().GetParent(), _poseWrtMarkerParent) == false) {
        PRINT_NAMED_ERROR("PreActionPose.GetPoseWrtMarkerParentFailed",
                          "Could not get the pre-action pose w.r.t. the marker's parent.");
      }
      _poseWrtMarkerParent.SetName("PreActionPose");
      
      SetHeightTolerance();
      
    } // PreActionPose Constructor
    
    
    PreActionPose::PreActionPose(const PreActionPose& canonicalPose,
                                 const Pose3d& markerParentPose,
                                 const f32 length_mm,
                                 const f32 offset_mm)
    : _type(canonicalPose.GetActionType())
    , _marker(canonicalPose.GetMarker())
    , _preActionPoseLineLength_mm(length_mm)
    //, _poseWrtMarkerParent(markerParentPose*canonicalPose._poseWrtMarkerParent)
    {
      // Extend pose translation by offset
      Vec3f trans = canonicalPose._poseWrtMarkerParent.GetTranslation();
      f32 length = trans.MakeUnitLength();
      trans = trans * (length + offset_mm);
      Pose3d canonicalPoseWithOffset(canonicalPose._poseWrtMarkerParent.GetRotationMatrix(), trans);
      _poseWrtMarkerParent = markerParentPose * canonicalPoseWithOffset;
      
      _poseWrtMarkerParent.SetParent(markerParentPose.GetParent());
      _poseWrtMarkerParent.SetName("PreActionPose");
      
      SetHeightTolerance();
    }
    
    PreActionPose::PreActionPose(const PreActionPose& other)
    : _type(other._type)
    , _marker(other._marker)
    , _poseWrtMarkerParent(other._poseWrtMarkerParent)
    , _heightTolerance(other._heightTolerance)
    , _preActionPoseLineLength_mm(other._preActionPoseLineLength_mm)
    {
      
    }
    
    
  } // namespace Vector
} // namespace Anki
