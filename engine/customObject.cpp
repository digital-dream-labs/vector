/**
 * File: CustomObject.cpp
 *
 * Author: Alec Solder
 * Date:   06/20/16
 *
 *
 * Copyright: Anki, Inc. 2016
 **/

#include "engine/customObject.h"

#include "coretech/common/engine/math/quad.h"

namespace Anki {
namespace Vector {

IMPLEMENT_ENUM_INCREMENT_OPERATORS(CustomObjectMarker);
  
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -- - - - - - -
const std::vector<Point3f>& CustomObject::GetCanonicalCorners() const
{
  return _canonicalCorners;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -- - - - - - -
CustomObject::CustomObject(ObjectType objectType,
                           CustomObjectMarker markerFront,
                           CustomObjectMarker markerBack,
                           CustomObjectMarker markerTop,
                           CustomObjectMarker markerBottom,
                           CustomObjectMarker markerLeft,
                           CustomObjectMarker markerRight,
                           f32 xSize_mm, f32 ySize_mm, f32 zSize_mm,
                           f32 markerWidth_mm, f32 markerHeight_mm,
                           bool isUnique,
                           CustomShape shape)
: ObservableObject(objectType)
, _size(xSize_mm, ySize_mm, zSize_mm)
, _markerSize(markerWidth_mm, markerHeight_mm)
, _vizHandle(VizManager::INVALID_HANDLE)
, _customShape(shape)
, _isUnique(isUnique)
{
  SetCanonicalCorners();
  
  // Ensure all markers are default initialized (AddFace can fail)
  _markersByFace.fill(CustomObjectMarker::Count);
  
  AddFace(FrontFace,  markerFront );
  AddFace(BackFace,   markerBack  );
  AddFace(LeftFace,   markerLeft  );
  AddFace(RightFace,  markerRight );
  AddFace(TopFace,    markerTop   );
  AddFace(BottomFace, markerBottom);
  
  switch(_customShape)
  {
    case BoxShape:
    case UnmarkedBoxShape:
      // No rotation ambiguities, nothing to do
      break;
      
    case CubeShape:
      // A cube with the same marker on all faces has complete rotation ambiguity
      _rotationAmbiguities = RotationAmbiguities(true, {
        RotationMatrix3d({1,0,0,  0,1,0,  0,0,1}),
        RotationMatrix3d({0,1,0,  1,0,0,  0,0,1}),
        RotationMatrix3d({0,1,0,  0,0,1,  1,0,0}),
        RotationMatrix3d({0,0,1,  0,1,0,  1,0,0}),
        RotationMatrix3d({0,0,1,  1,0,0,  0,1,0}),
        RotationMatrix3d({1,0,0,  0,0,1,  0,1,0})
      });
      break;
      
    case WallShape:
      // A wall with the same marker on both sides has 180deg rotation ambiguity around the Z axis
      _rotationAmbiguities = RotationAmbiguities(false, {
        RotationMatrix3d({ 1,0,0,  0, 1,0,  0,0,1}),
        RotationMatrix3d({-1,0,0,  0,-1,0,  0,0,1}),
      });
      break;
  }
  
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -- - - - - - -
bool CustomObject::IsValidCustomType(ObjectType objectType)
{
  static_assert(((ObjectType)(Util::EnumToUnderlying(ObjectType::CustomType19) + 1)) == ObjectType::CustomFixedObstacle,
                "CustomFixedObstacle should immediately follow the max CustomType");
  
  if(objectType < ObjectType::CustomType00 || objectType >= ObjectType::CustomFixedObstacle)
  {
    PRINT_NAMED_WARNING("CustomObject.IsValidCustomType.BadObjectType",
                        "Type should be CustomTypeNN");
    return false;
  }
  
  return true;
}
  
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -- - - - - - -
CustomObject* CustomObject::CreateBox(ObjectType objectType,
                                      CustomObjectMarker markerFront,
                                      CustomObjectMarker markerBack,
                                      CustomObjectMarker markerTop,
                                      CustomObjectMarker markerBottom,
                                      CustomObjectMarker markerLeft,
                                      CustomObjectMarker markerRight,
                                      f32 xSize_mm, f32 ySize_mm, f32 zSize_mm,
                                      f32 markerWidth_mm, f32 markerHeight_mm,
                                      bool isUnique)
{
  if(!IsValidCustomType(objectType))
  {
    return nullptr;
  }
  
  // Validate that all markers are different, since that is required for BoxShape custom objects
  const std::set<CustomObjectMarker> uniqueMarkers{
    markerFront, markerBack, markerTop, markerBottom, markerLeft, markerRight
  };
  if(uniqueMarkers.size() != NumFaces)
  {
    PRINT_NAMED_WARNING("CustomObject.CreateCustomBox.DuplicateMarkers",
                        "Expecting custom box object to have 6 different markers");
    return nullptr;
  }
  
  CustomObject* object = new CustomObject(objectType,
                                          markerFront, markerBack, markerTop, markerBottom, markerLeft, markerRight,
                                          xSize_mm, ySize_mm, zSize_mm,
                                          markerWidth_mm, markerHeight_mm,
                                          isUnique,
                                          BoxShape);
  
  return object;
}
  
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -- - - - - - -
CustomObject* CustomObject::CreateWall(ObjectType objectType,
                                       CustomObjectMarker marker,
                                       f32 width_mm, f32 height_mm,
                                       f32 markerWidth_mm, f32 markerHeight_mm,
                                       bool isUnique)
{
  if(!IsValidCustomType(objectType))
  {
    return nullptr;
  }
  
  return new CustomObject(objectType,
                          marker, marker, // Only define front/back markers
                          CustomObjectMarker::Count, CustomObjectMarker::Count,
                          CustomObjectMarker::Count, CustomObjectMarker::Count,
                          kWallThickness_mm, width_mm, height_mm,
                          markerWidth_mm, markerHeight_mm,
                          isUnique,
                          WallShape);
}
  
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -- - - - - - -
CustomObject* CustomObject::CreateCube(ObjectType objectType,
                                       CustomObjectMarker marker, f32 size_mm,
                                       f32 markerWidth_mm, f32 markerHeight_mm,
                                       bool isUnique)
{
  if(!IsValidCustomType(objectType))
  {
    return nullptr;
  }
  
  return new CustomObject(objectType,
                          marker, marker, marker, marker, marker, marker, // same marker on all faces
                          size_mm, size_mm, size_mm,
                          markerWidth_mm, markerHeight_mm,
                          isUnique,
                          CubeShape);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -- - - - - - -
CustomObject* CustomObject::CreateFixedObstacle(f32 xSize_mm, f32 ySize_mm, f32 zSize_mm)
{
  return new CustomObject(ObjectType::CustomFixedObstacle,
                          CustomObjectMarker::Count, CustomObjectMarker::Count, CustomObjectMarker::Count,
                          CustomObjectMarker::Count, CustomObjectMarker::Count, CustomObjectMarker::Count,
                          xSize_mm, ySize_mm, zSize_mm,
                          0.f, 0.f, false, UnmarkedBoxShape);
}
  
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -- - - - - - -
Vision::MarkerType CustomObject::GetVisionMarkerType(const CustomObjectMarker customMarker)
{
  switch(customMarker)
  {
    case CustomObjectMarker::Circles2:
      return Vision::MARKER_SDK_2CIRCLES;
    case CustomObjectMarker::Circles3:
      return Vision::MARKER_SDK_3CIRCLES;
    case CustomObjectMarker::Circles4:
      return Vision::MARKER_SDK_4CIRCLES;
    case CustomObjectMarker::Circles5:
      return Vision::MARKER_SDK_5CIRCLES;
      
    case CustomObjectMarker::Diamonds2:
      return Vision::MARKER_SDK_2DIAMONDS;
    case CustomObjectMarker::Diamonds3:
      return Vision::MARKER_SDK_3DIAMONDS;
    case CustomObjectMarker::Diamonds4:
      return Vision::MARKER_SDK_4DIAMONDS;
    case CustomObjectMarker::Diamonds5:
      return Vision::MARKER_SDK_5DIAMONDS;
      
    case CustomObjectMarker::Hexagons2:
      return Vision::MARKER_SDK_2HEXAGONS;
    case CustomObjectMarker::Hexagons3:
      return Vision::MARKER_SDK_3HEXAGONS;
    case CustomObjectMarker::Hexagons4:
      return Vision::MARKER_SDK_4HEXAGONS;
    case CustomObjectMarker::Hexagons5:
      return Vision::MARKER_SDK_5HEXAGONS;
      
    case CustomObjectMarker::Triangles2:
      return Vision::MARKER_SDK_2TRIANGLES;
    case CustomObjectMarker::Triangles3:
      return Vision::MARKER_SDK_3TRIANGLES;
    case CustomObjectMarker::Triangles4:
      return Vision::MARKER_SDK_4TRIANGLES;
    case CustomObjectMarker::Triangles5:
      return Vision::MARKER_SDK_5TRIANGLES;
      
    case CustomObjectMarker::Count:
      return Vision::MARKER_INVALID;
  }
}
  
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -- - - - - - -
void CustomObject::SetCanonicalCorners()
{
  _canonicalCorners = {{
    Point3f(-0.5f*_size.x(), -0.5f*_size.y(),  0.5f*_size.z()),
    Point3f( 0.5f*_size.x(), -0.5f*_size.y(),  0.5f*_size.z()),
    Point3f(-0.5f*_size.x(), -0.5f*_size.y(), -0.5f*_size.z()),
    Point3f( 0.5f*_size.x(), -0.5f*_size.y(), -0.5f*_size.z()),
    Point3f(-0.5f*_size.x(),  0.5f*_size.y(),  0.5f*_size.z()),
    Point3f( 0.5f*_size.x(),  0.5f*_size.y(),  0.5f*_size.z()),
    Point3f(-0.5f*_size.x(),  0.5f*_size.y(), -0.5f*_size.z()),
    Point3f( 0.5f*_size.x(),  0.5f*_size.y(), -0.5f*_size.z())
  }};
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -- - - - - - -
 
  
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -- - - - - - -
void CustomObject::AddFace(const FaceName whichFace, const CustomObjectMarker customMarker)
{
  Vision::MarkerType visionMarkerType = GetVisionMarkerType(customMarker);
  if(Vision::MarkerType::MARKER_INVALID == visionMarkerType)
  {
    return;
  }
  
  // NOTE: these poses intentionally have no parent. That is handled by AddMarker below.
  Pose3d facePose;
  switch(whichFace)
  {
    case FrontFace:
      facePose = Pose3d(-M_PI_2_F, Z_AXIS_3D(), {-_size.x() * 0.5f, 0.f, 0.f});
      break;
      
    case LeftFace:
      facePose = Pose3d(M_PI,      Z_AXIS_3D(), {0.f, _size.y() * 0.5f, 0.f});
      break;
      
    case BackFace:
      facePose = Pose3d(M_PI_2,    Z_AXIS_3D(), {_size.x() * 0.5f, 0.f, 0.f});
      break;
      
    case RightFace:
      facePose = Pose3d(0.0f,      Z_AXIS_3D(), {0.f, -_size.y() * 0.5f, 0.f});
      break;
      
    case TopFace:
      // Rotate -90deg around X, then -90 around Z
      facePose = Pose3d(2.09439510f, {-0.57735027f, 0.57735027f, -0.57735027f}, {0.f, 0.f, _size.z() * 0.5f});
      break;
      
    case BottomFace:
      // Rotate +90deg around X, then -90 around Z
      facePose = Pose3d(2.09439510f, {0.57735027f, -0.57735027f, -0.57735027f}, {0.f, 0.f, -_size.z() * 0.5f});
      break;
      
    case NumFaces:
      PRINT_NAMED_ERROR("CustomObject.AddFace.NumFaces", "Attempting to add NumFaces as a custom object face.");
      return;
  }
  
  // Keep track of what is on each face, for cloning
  _markersByFace[whichFace] = customMarker;
  AddMarker(visionMarkerType, facePose, _markerSize);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -- - - - - - -
CustomObject::~CustomObject()
{
  EraseVisualization();
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -- - - - - - -
Point3f CustomObject::GetSameDistanceTolerance() const
{
  // COZMO-9440: Not really correct for non-cube-shaped custom objects
  return _size * kSameDistToleranceFraction;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -- - - - - - -
void CustomObject::Visualize(const ColorRGBA& color) const
{
  DEV_ASSERT(nullptr != _vizManager, "CustomObject.Visualize.VizManagerNotSet");
  Pose3d vizPose = GetPose().GetWithRespectToRoot();
  _vizHandle = _vizManager->DrawCuboid(GetID().GetValue(), _size, vizPose, color);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -- - - - - - -
void CustomObject::EraseVisualization() const
{
  // Erase the main object
  if(_vizHandle != VizManager::INVALID_HANDLE) {
    _vizManager->EraseVizObject(_vizHandle);
    _vizHandle = VizManager::INVALID_HANDLE;
  }
}


} // namespace Vector
} // namespace Anki
