/**
 * File: customObject.h
 *
 * Author: Alec Solder / Andrew Stein
 * Date:   06/20/16
 *
 * Description: Implements CustomObject which is an object type that is created from external sources, such as via the SDK
 *              They can optionally be created with markers associated with them so they are observable in the world.
 *
 *
 * Copyright: Anki, Inc. 2016
 **/

#ifndef __Anki_Cozmo_CustomObject_H__
#define __Anki_Cozmo_CustomObject_H__

#include "engine/cozmoObservableObject.h"
#include "engine/viz/vizManager.h"

#include "clad/types/customObjectMarkers.h"
#include "clad/types/objectTypes.h"

#include "coretech/vision/shared/MarkerCodeDefinitions.h"

#include "util/enums/enumOperators.h"

namespace Anki {
namespace Vector {
  
DECLARE_ENUM_INCREMENT_OPERATORS(CustomObjectMarker);
  
class CustomObject : public ObservableObject
{
public:
  
  // NOTE: You cannot directly construct a CustomObject, but instead must use one of these
  // static Create methods
  
  // Creates a fully custom box with a specific marker on each side
  static CustomObject* CreateBox(ObjectType objectType,
                                 CustomObjectMarker markerFront,
                                 CustomObjectMarker markerBack,
                                 CustomObjectMarker markerTop,
                                 CustomObjectMarker markerBottom,
                                 CustomObjectMarker markerLeft,
                                 CustomObjectMarker markerRight,
                                 f32 xSize_mm, f32 ySize_mm, f32 zSize_mm,
                                 f32 markerWidth_mm, f32 markerHeight_mm,
                                 bool isUnique);
  
  // Create a wall with the same marker on the front and back
  static CustomObject* CreateWall(ObjectType objectType,
                                  CustomObjectMarker marker,
                                  f32 width_mm, f32 height_mm,
                                  f32 markerWidth_mm, f32 markerHeight_mm,
                                  bool isUnique);
  
  // Create a cube with the same marker on all sides
  static CustomObject* CreateCube(ObjectType objectType,
                                  CustomObjectMarker marker, f32 size_mm,
                                  f32 markerWidth_mm, f32 markerHeight_mm,
                                  bool isUnique);
  
  // Create a box with no markers (not actually observable, but can be treated as a fixed obstacle)
  static CustomObject* CreateFixedObstacle(f32 xSize_mm, f32 ySize_mm, f32 zSize_mm);
  
  virtual ~CustomObject();
  
  static Vision::MarkerType GetVisionMarkerType(const CustomObjectMarker customMarker);
  
  //
  // Inherited Virtual Methods
  //
  
  virtual CustomObject* CloneType() const override;
  
  virtual void Visualize(const ColorRGBA& color) const override;
  virtual void EraseVisualization() const override;
  
  virtual Point3f GetSameDistanceTolerance() const override;      
  
  virtual const Point3f& GetSize() const override { return _size; }

  virtual bool IsUnique() const override { return _isUnique; }
  
  virtual RotationAmbiguities const& GetRotationAmbiguities() const override { return _rotationAmbiguities; }
  
private:
  
  enum FaceName {
    FrontFace = 0,
    LeftFace,
    BackFace,
    RightFace,
    TopFace,
    BottomFace,
    
    NumFaces
  };
  
  enum CustomShape {
    BoxShape,  // All six sides different: no rotation ambiguity
    CubeShape, // All six sides same: full rotation ambiguity
    WallShape, // Both sides same: two-rotation ambiguity
    UnmarkedBoxShape,  // Box with no markers (no ambiguity required)
  };
  
  constexpr static const f32 kSameDistToleranceFraction    = 0.5f;
  constexpr static const f32 kWallThickness_mm             = 10.f; // x dimension of "walls"
  
  CustomObject(ObjectType objectType,
               CustomObjectMarker markerFront,
               CustomObjectMarker markerBack,
               CustomObjectMarker markerTop,
               CustomObjectMarker markerBottom,
               CustomObjectMarker markerLeft,
               CustomObjectMarker markerRight,
               f32 xSize_mm, f32 ySize_mm, f32 zSize_mm,
               f32 markerWidth_mm, f32 markerHeight_mm,
               bool isUnique,
               CustomShape shape);
  
  static bool IsValidCustomType(ObjectType objectType);
  
  virtual const std::vector<Point3f>& GetCanonicalCorners() const override;
  
  void SetCanonicalCorners();
  void AddFace(const FaceName whichFace, const CustomObjectMarker marker);

  std::array<CustomObjectMarker, NumFaces> _markersByFace;
  std::vector<Point3f>                     _canonicalCorners;
  RotationAmbiguities                      _rotationAmbiguities;

  Point3f _size;
  Point2f _markerSize;
  
  mutable VizManager::Handle_t _vizHandle;
  const CustomShape            _customShape;
  const bool                   _isUnique;
  
}; // class CustomObject


inline CustomObject* CustomObject::CloneType() const
{
  return new CustomObject(this->GetType(),
                          _markersByFace[FrontFace],
                          _markersByFace[BackFace],
                          _markersByFace[TopFace],
                          _markersByFace[BottomFace],
                          _markersByFace[LeftFace],
                          _markersByFace[RightFace],
                          _size.x(), _size.y(), _size.z(),
                          _markerSize.x(), _markerSize.y(),
                          _isUnique,
                          _customShape);
}
  
} // namespace Vector
} // namespace Anki

#endif // __Anki_Cozmo_CustomObject_H__
