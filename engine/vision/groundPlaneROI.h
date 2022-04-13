/**
 * File: groundPlaneROI.h
 *
 * Author: Andrew Stein
 * Date:   11/20/15
 *
 * Description: Defines a class for visual reasoning about a region of interest (ROI)
 *              on the ground plane immediately in front of the robot.
 *
 *
 * Copyright: Anki, Inc. 2015
 **/

#ifndef __Anki_Cozmo_Basestation_GroundPlaneROI_H__
#define __Anki_Cozmo_Basestation_GroundPlaneROI_H__

#include "coretech/vision/engine/image.h"
#include "coretech/common/shared/math/matrix_fwd.h"

namespace Anki {
namespace Vector {

class GroundPlaneROI
{
public:
  // Define ROI quad on ground plane, in robot-centric coordinates (origin is *)
  // The region is "length" mm long and starts "dist" mm from the robot origin.
  // It is "w_close" mm wide at the end close to the robot and "w_far" mm
  // wide at the opposite end
  //                              _____
  //  +---------+    _______------     |
  //  | Robot   |   |                  |
  //  |       * |   | w_close          | w_far
  //  |         |   |_______           |
  //  +---------+           ------_____|
  //
  //          |<--->|<---------------->|
  //           dist         length
  //
  
  static f32 GetDist()       { return _dist; }
  static f32 GetWidthFar()   { return _widthFar; }
  static f32 GetWidthClose() { return _widthClose; }
  static f32 GetLength()     { return _length; }
  
  // Full, fixed ground quad, as illustrated above, at the specified z height
  static Quad3f GetGroundQuad(f32 z=0.f);
  
  // Get just the portion of the ground quad that is visible in the image.
  // Returns whether the ground quad projected into the image intersected the
  // image's border. (I.e., if full ground quad is visible in image, returns false.)
  bool GetVisibleGroundQuad(const Matrix_3x3f& H, s32 imgWidth, s32 imgHeight,
                            Quad3f& groundQuad, f32 z=0.f) const;
  // 2D Version
  bool GetVisibleGroundQuad(const Matrix_3x3f& H, s32 imgWidth, s32 imgHeight, Quad2f& groundQuad) const;

  // Get the ground quad projected into the image, cropped to the image borders.
  // Returns true if the ground quad intersects the image's border.
  bool GetImageQuad(const Matrix_3x3f& H, s32 imgWidth, s32 imgHeight, Quad2f& imgQuad) const;
  
  Vision::ImageRGB GetOverheadImage(const Vision::ImageRGB& image,
                                    const Matrix_3x3f& H,
                                    bool useMask = true) const;
  
  Vision::Image GetOverheadImage(const Vision::Image& image,
                                 const Matrix_3x3f& H,
                                 bool useMask = true) const;
  
  // Creates the mask on first request and then just returns that one from then on
  const Vision::Image& GetOverheadMask() const;

  // Returns the overhead mask updated with the current head position
  const Vision::Image GetVisibleOverheadMask(const Matrix_3x3f& H, s32 imgWidth, s32 imgHeight) const;
  
  Point2f GetOverheadImageOrigin() const { return Point2f{_dist, -_widthFar*0.5f}; }
  
  // Get the near and far points on the ground plane that are visible in image
  void GetVisibleX(const Matrix_3x3f& H, s32 imageWidth, s32 imageHeight,
                   f32& near, f32& far) const;
  
  // Clamps the given quad on the bottom with the given left/right points
  static bool ClampQuad(Quad2f& quad, const Point2f& groundLeft, const Point2f& groundRight);
  
private:
  // In mm
  static const f32 _dist;
  static const f32 _length;
  static const f32 _widthFar;
  static const f32 _widthClose;
  
  mutable Vision::Image _overheadMask;
  
  template<class PixelType>
  void GetOverheadImageHelper(const Vision::ImageBase<PixelType>& image,
                              const Matrix_3x3f& H,
                              Vision::ImageBase<PixelType>& overheadImg,
                              bool useMask) const;
}; // class GroundPlaneROI

  
inline bool GroundPlaneROI::GetVisibleGroundQuad(const Matrix_3x3f& H, s32 imgWidth, s32 imgHeight, Quad2f& groundQuad) const
{
  Quad3f groundQuad3d;
  const bool retVal = GetVisibleGroundQuad(H, imgWidth, imgHeight, groundQuad3d);
  groundQuad = Quad2f(groundQuad3d); // drop z coordinates
  return retVal;
}
  
  
} // namespace Vector
} // namespace Anki

#endif // __Anki_Cozmo_Basestation_GroundPlaneROI_H__

