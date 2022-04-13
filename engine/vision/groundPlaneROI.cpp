/**
 * File: groundPlaneROI.cpp
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

#include "engine/vision/groundPlaneROI.h"

#include "coretech/common/engine/math/quad.h"
#include "coretech/common/shared/math/matrix.h"

#include "opencv2/core/core.hpp"
#include "opencv2/imgproc/imgproc.hpp"

namespace Anki {
namespace Vector {

const f32 GroundPlaneROI::_dist = 45.f;
const f32 GroundPlaneROI::_length = 150.f;
const f32 GroundPlaneROI::_widthFar = 180.f;
const f32 GroundPlaneROI::_widthClose = 45.f;

 
const Vision::Image& GroundPlaneROI::GetOverheadMask() const
{
  if(_overheadMask.IsEmpty())
  {
    _overheadMask.Allocate(_widthFar, _length);
    const s32 w = std::round(0.5f*(_widthFar - _widthClose));
    _overheadMask.FillWith(0);
    cv::fillConvexPoly(_overheadMask.get_CvMat_(), std::vector<cv::Point>{
      cv::Point(0, w),
      cv::Point(_length-1,0),
      cv::Point(_length-1,_widthFar-1),
      cv::Point(0, w + _widthClose)
    }, 255);
  }
  return _overheadMask;
}

Quad3f GroundPlaneROI::GetGroundQuad(f32 zHeight)
{
  return Quad3f{
    {_dist + _length   ,  0.5f*_widthFar   , zHeight},
    {_dist             ,  0.5f*_widthClose , zHeight},
    {_dist + _length   , -0.5f*_widthFar   , zHeight},
    {_dist             , -0.5f*_widthClose , zHeight}
  };
}

bool GroundPlaneROI::GetVisibleGroundQuad(const Matrix_3x3f& H, s32 imgWidth, s32 imgHeight,
                                          Quad3f& groundQuad, f32 z) const
{
  Quad2f imgQuad;
  const bool intersectsBorder = GetImageQuad(H, imgWidth, imgHeight, imgQuad);
  
  // Start with full ground quad. If the projected image doesn't intersect
  // the image border, we'll just return that.
  groundQuad = GetGroundQuad(z);
  
  if(intersectsBorder)
  {
    // Project back into ground
    //  Technically, we are only checking for "near" intersection in GetImageQuad above,
    //  so the far points can't move, so there's no reason to warp them.
    Matrix_3x3f invH = H.GetInverse();
    for(Quad::CornerName iCorner : {Quad::BottomLeft, Quad::BottomRight})
    {
      Point3f temp = invH * Point3f(imgQuad[iCorner].x(), imgQuad[iCorner].y(), 1.f);
      DEV_ASSERT(temp.z() > 0, "GroundPlaneROI.GetVisibleGroundQuad.BadProjectedZ");
      const f32 divisor = 1.f / temp.z();
      groundQuad[iCorner].x() = temp.x() * divisor;
      groundQuad[iCorner].y() = temp.y() * divisor;
    }
  }

  return intersectsBorder;
} // GetVisibleGroundQuad()

const Vision::Image GroundPlaneROI::GetVisibleOverheadMask(const Matrix_3x3f& H, s32 imgWidth, s32 imgHeight) const
{
  // Start with full overhead mask
  Vision::Image mask(GetOverheadMask());
  
  // Blank out anything that is closer than nearx or farther than farx
  f32 xnearF32, xfarF32;
  GetVisibleX(H, imgWidth, imgHeight, xnearF32, xfarF32);
  const s32 xnear = std::round(xnearF32);
  const s32 xfar  = std::round(xfarF32);
  if(xnear > 0)
  {
    Rectangle<s32> nearROI(0,0,xnear,mask.GetNumRows());
    Vision::Image ROI = mask.GetROI(nearROI);
    ROI.FillWith(0);
  }
  
  if(xfar < mask.GetNumCols())
  {
    Rectangle<s32> farROI(xfar,0,mask.GetNumCols()-xfar,mask.GetNumRows());
    Vision::Image ROI = mask.GetROI(farROI);
    ROI.FillWith(0);
  }
  
  return mask;
}

  
void GroundPlaneROI::GetVisibleX(const Matrix_3x3f& H, s32 imageWidth, s32 imageHeight,
                                 f32& near, f32& far) const
{
  Matrix_3x3f invH = H.GetInverse();
  
  Point3f temp = invH * Point3f(imageWidth/2, 0/*-imageHeight/2*/, 1.f);
  //ASSERT_NAMED(temp.z() > 0, "Projected points should have z > 0");
  if(temp.z() <= 0) {
    far = _dist + _length;
  } else {
    far = std::min(temp.x() / temp.z(), _dist + _length);
  }
  
  temp = invH * Point3f(imageWidth/2, imageHeight-1/*imageHeight/2*/, 1.f);
  //ASSERT_NAMED(temp.z() > 0, "Projected points should have z > 0");
  if(temp.z() <= 0) {
    near = _dist;
  } else {
    near = std::max(temp.x() / temp.z(), _dist);
  }
}
  
bool GroundPlaneROI::GetImageQuad(const Matrix_3x3f& H, s32 imgWidth, s32 imgHeight, Quad2f& imgQuad) const
{
  // Note that the z coordinate is actually 0, but in the mapping to the
  // image plane below, we are actually doing K[R t]* [Px Py Pz 1]',
  // and Pz == 0 and we thus drop out the third column, making it
  // K[R t] * [Px Py 0 1]' or H * [Px Py 1]', so for convenience, we just
  // go ahead and fill in that 1 here as if it were the "z" coordinate:
  const Quad3f groundQuad = GetGroundQuad(1.f);
  
  // Project ground quad in camera image
  // (This could be done by Camera::ProjectPoints, but that would duplicate
  //  the computation of H we did above, which here we need to use below)
  for(Quad::CornerName iCorner = Quad::CornerName::FirstCorner;
      iCorner != Quad::CornerName::NumCorners; ++iCorner)
  {
    Point3f temp = H * groundQuad[iCorner];
    DEV_ASSERT(!Util::IsNearZero(temp.z()), "GroundPlaneROI.GetImageQuad.ProjectedGroundQuadPointAtZero");
    const f32 divisor = 1.f / temp.z();
    imgQuad[iCorner].x() = temp.x() * divisor;
    imgQuad[iCorner].y() = temp.y() * divisor;
  }
  
  // Clamp to image boundary:
  const Point2f imgBotLeft(0,imgHeight-1);
  const Point2f imgBotRight(imgWidth-1,imgHeight-1);
  const bool intersectsImageBorder = ClampQuad(imgQuad, imgBotLeft, imgBotRight);
  
  return intersectsImageBorder;
} // GetImageQuad()

template<class PixelType>
void GroundPlaneROI::GetOverheadImageHelper(const Vision::ImageBase<PixelType>& image, const Matrix_3x3f& H,
                                            Vision::ImageBase<PixelType>& overheadImg,
                                            bool useMask) const
{
  // Need to apply a shift after the homography to put things in image
  // coordinates with (0,0) at the upper left (since groundQuad's origin
  // is not upper left). Also mirror Y coordinates since we are looking
  // from above, not below
  Matrix_3x3f InvShift{
    1.f, 0.f, _dist, // Negated b/c we're using inv(Shift)
    0.f,-1.f, _widthFar*0.5f,
    0.f, 0.f, 1.f};
  
  // Note that we're applying the inverse homography, so we're doing
  //  inv(Shift * inv(H)), which is the same as  (H * inv(Shift))
  cv::warpPerspective(image.get_CvMat_(), overheadImg.get_CvMat_(), (H*InvShift).get_CvMatx_(),
                      cv::Size(_length, _widthFar), cv::INTER_LINEAR | cv::WARP_INVERSE_MAP);
  
  if(useMask)
  {
    const Vision::Image& mask = GetOverheadMask();
    
    DEV_ASSERT(overheadImg.IsContinuous() && mask.IsContinuous(), "Overhead image and mask should be continuous");
    
    // Zero out masked regions
    PixelType* imgData = overheadImg.GetDataPointer();
    const u8* maskData = mask.GetDataPointer();
    for(s32 i=0; i<_overheadMask.GetNumElements(); ++i) {
      if(maskData[i] == 0) {
        imgData[i] = PixelType(0);
      }
    }
  }
} // GetOverheadImage()

Vision::ImageRGB GroundPlaneROI::GetOverheadImage(const Vision::ImageRGB &image, const Matrix_3x3f &H,
                                                  bool useMask) const
{
  Vision::ImageRGB overheadImg(_overheadMask.GetNumRows(), _overheadMask.GetNumCols());
  GetOverheadImageHelper(image, H, overheadImg, useMask);
  return overheadImg;
}

Vision::Image GroundPlaneROI::GetOverheadImage(const Vision::Image &image, const Matrix_3x3f &H,
                                               bool useMask) const
{
  Vision::Image overheadImg(_overheadMask.GetNumRows(), _overheadMask.GetNumCols());
  GetOverheadImageHelper(image, H, overheadImg, useMask);
  return overheadImg;
}


namespace {
  inline kmRay2 Point2fToRay(const Point2f& from, const Point2f& to ) {
    kmRay2 retRay;
    kmVec2 kmFrom{from.x(), from.y() };
    kmVec2 kmTo  {to.x()  , to.y()   };
    kmRay2FillWithEndpoints(&retRay, &kmFrom, &kmTo);
    return retRay;
  }
} // anonymous namespace
  
  
bool GroundPlaneROI::ClampQuad(Quad2f& quad, const Point2f& groundLeft, const Point2f& groundRight)
{
  // this is a trick to prevent precision errors around the borders. We are just trying to find intersection
  // with a line, not a segment, so we artificially extend the segment given to provide a safer line
  Vec2f clampLineDir = groundLeft - groundRight;
  const Point2f botClampLeft ( groundLeft  + clampLineDir );
  const Point2f botClampRight( groundRight - clampLineDir );
  
  // Create lines for collision check
  kmRay2 groundBotLine = Point2fToRay(botClampLeft, botClampRight);
  kmRay2 segmentLeftLine  = Point2fToRay(quad[Quad::BottomLeft],  quad[Quad::TopLeft] );
  kmRay2 segmentRightLine = Point2fToRay(quad[Quad::BottomRight], quad[Quad::TopRight]);
  
  // find intersections of segment lines (it should always happen unless there's a precision error in the border,
  // which can happen)
  kmVec2 interBL, interBR;
  const kmBool leftBotInter  = kmSegment2WithSegmentIntersection(&groundBotLine, &segmentLeftLine , &interBL);
  const kmBool rightBotInter = kmSegment2WithSegmentIntersection(&groundBotLine, &segmentRightLine, &interBR);
  if ( leftBotInter && rightBotInter )
  {
    Anki::Point2f clampedBotLeft (interBL.x, interBL.y);
    Anki::Point2f clampedBotRight(interBR.x, interBR.y);
    quad[Quad::BottomLeft ] = clampedBotLeft;
    quad[Quad::BottomRight] = clampedBotRight;
    return true;
  }
  else
  {
    //PRINT_NAMED_ERROR("GroundPlaneROI.ClampQuad.NoCollisionFound",
    //                  "Could not find intersection of fake vision quad with ground plane. Ignoring segment");
    return false;
  }
  
}

  
} // namespace Vector
} // namespace Anki
