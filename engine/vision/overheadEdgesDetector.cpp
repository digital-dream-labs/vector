/**
 * File: detectOverHeadEdges.cpp
 *
 * Author: Lorenzo Riano
 * Date:   2017-08-31
 *
 * Description: Vision system component for detecting edges in the ground plane.
 *
 *
 * Copyright: Anki, Inc. 2017
 **/

#include "overheadEdgesDetector.h"
#include "coretech/vision/engine/imageCache.h"
#include "coretech/common/engine/math/quad.h"
#include "engine/robot.h"

namespace Anki {
namespace Vector {

// Declaration
namespace {

inline bool SetEdgePosition(const Matrix_3x3f &invH,
                            const s32 i, const s32 j,
                            OverheadEdgePoint &edgePoint);

bool LiftInterferesWithEdges(bool isLiftTopInCamera, float liftTopY,
                             bool isLiftBotInCamera, float liftBotY,
                             int planeTopY, int planeBotY);

bool CheckThreshold(Vision::PixelRGB_<f32> pixel, f32 threshold);

bool CheckThreshold(f32 pixel, f32 threshold);

struct ImageRGBTrait
{
  typedef Vision::ImageRGB ImageType;
  typedef Vision::PixelRGB_<s16> SPixelType;
  typedef Vision::PixelRGB_<f32> FPixelType;
  typedef Vision::PixelRGB UPixelType;
};

struct ImageGrayTrait
{
  typedef Vision::Image ImageType;
  typedef s16 SPixelType;
  typedef f32 FPixelType;
  typedef u8 UPixelType;
};

}

OverheadEdgesDetector::OverheadEdgesDetector(const Vision::Camera &camera, VizManager *vizManager,
                                             Vision::Profiler &profiler, f32 edgeThreshold, u32 minChainLength) :
                                             _camera(camera),
                                             _vizManager(vizManager),
                                             _profiler(profiler),
                                             _kEdgeThreshold(edgeThreshold),
                                             _kMinChainLength(minChainLength)
{

}

Result OverheadEdgesDetector::Detect(Anki::Vision::ImageCache &imageCache, const VisionPoseData &crntPoseData,
                                     VisionProcessingResult &currentResult)
{
  if (imageCache.HasColor()) {
    return DetectHelper<ImageRGBTrait>(imageCache.GetRGB(), crntPoseData, currentResult);
  }
  else {
    return DetectHelper<ImageGrayTrait>(imageCache.GetGray(), crntPoseData, currentResult);
  }
}

template<typename ImageTraitType>
Result OverheadEdgesDetector::DetectHelper(const typename ImageTraitType::ImageType &image,
                                           const VisionPoseData &crntPoseData,
                                           VisionProcessingResult &currentResult)
{
  // if the ground plane is not currently visible, do not detect edges
  if (!crntPoseData.groundPlaneVisible) {
    OverheadEdgeFrame edgeFrame;
    edgeFrame.timestamp = image.GetTimestamp();
    edgeFrame.groundPlaneValid = false;
    currentResult.overheadEdges.push_back(std::move(edgeFrame));
    return RESULT_OK;
  }

  // if the lift is moving it's probably not a good idea to detect edges, it might be entering our view
  // if we are carrying an object, it's also not probably a good idea, since we would most likely detect
  // its edges (unless it's carrying high and we are looking down, but that requires modeling what
  // objects can be carried here).
  if (crntPoseData.histState.WasLiftMoving() || crntPoseData.histState.WasCarryingObject()) {
    return RESULT_OK;
  }

  // Get ROI around ground plane quad in image
  const Matrix_3x3f &H = crntPoseData.groundPlaneHomography;
  const GroundPlaneROI &roi = crntPoseData.groundPlaneROI;
  Quad2f groundInImage;
  roi.GetImageQuad(H, image.GetNumCols(), image.GetNumRows(), groundInImage);

  Anki::Rectangle<s32> bbox(groundInImage);

  // rsam: I tried to create a mask for the lift, calculating top and bottom sides of the lift and projecting
  // onto camera plane. Turns out that physical robots have a lot of slack in the lift, so this projection,
  // despite being correct on the paper, was not close to where the camera was seeing the lift.
  // For this reason we have to completely prevent edge detection unless the lift is fairly up (beyond ground plane),
  // or fairly low. Fairly up and fairly low are the parameters set here. Additionally, instead of trying to
  // detect borders below the bottom margin line, if any of the margin lines are inside the projected quad, we stop
  // edge detection altogether. This means that unless the lift is totally out of the ground plane, we will not
  // do edge detection at all. Note: if this becomes a nuisance, we can revisit this and craft a better
  // hardware slack margin, and try to detect edges below the lift when the lift is on the ground plane projection
  // by shrinking bbox's top Y to liftBottomY
  static const bool kDebugRenderBboxVsLift = false;

  // virtual points in the lift to identify whether the lift is our camera view
  float liftBotY = .0f;
  float liftTopY = .0f;
  bool isLiftTopInCamera = true;
  bool isLiftBotInCamera = true;
  {
    // we only need to provide slack to the bottom edge (empirically), because of two reasons:
    // 1) slack makes the lift fall with respect to its expected position, not lift even higher
    // 2) the ground plane does not start at the robot, but in front of it, which accounts for the top of the lift
    //    when the camera is pointing down. Once we start moving the lift up, the fall slack kicks in and gives
    //    breathing room with respect to the top of
    static const float kHardwareFallSlackMargin_mm = LIFT_HARDWARE_FALL_SLACK_MM;

    // offsets we are going to calculate (point at the top and front of the lift, and at the bottom and back of the lift)
    static const Anki::Vec3f offsetTopFrontPoint{LIFT_FRONT_WRT_WRIST_JOINT, 0.f, LIFT_XBAR_HEIGHT_WRT_WRIST_JOINT};
    static const Anki::Vec3f offsetBotBackPoint{LIFT_BACK_WRT_WRIST_JOINT, 0.f,
                                   LIFT_XBAR_BOTTOM_WRT_WRIST_JOINT - kHardwareFallSlackMargin_mm};

    // calculate the lift pose with respect to the poseStamp's origin
    const Pose3d liftBasePose(0.f, Y_AXIS_3D(), {LIFT_BASE_POSITION[0], LIFT_BASE_POSITION[1], LIFT_BASE_POSITION[2]},
                              crntPoseData.histState.GetPose(), "RobotLiftBase");
    Pose3d liftPose(0, Y_AXIS_3D(), {0.f, 0.f, 0.f}, liftBasePose, "RobotLift");
    Robot::ComputeLiftPose(crntPoseData.histState.GetLiftAngle_rad(), liftPose);

    // calculate lift wrt camera
    Pose3d liftPoseWrtCamera;
    if (! liftPose.GetWithRespectTo(crntPoseData.cameraPose, liftPoseWrtCamera)) {
      PRINT_NAMED_ERROR("VisionSystem.DetectOverheadEdges.PoseTreeError",
                        "Could not get lift pose w.r.t. camera pose.");
      return RESULT_FAIL;
    }

    // project lift's top onto camera and store Y
    const Anki::Vec3f liftTopWrtCamera = liftPoseWrtCamera * offsetTopFrontPoint;
    Anki::Point2f liftTopCameraPoint;
    isLiftTopInCamera = _camera.Project3dPoint(liftTopWrtCamera, liftTopCameraPoint);
    liftTopY = liftTopCameraPoint.y();

    // project lift's bot onto camera and store Y
    const Anki::Vec3f liftBotWrtCamera = liftPoseWrtCamera * offsetBotBackPoint;
    Anki::Point2f liftBotCameraPoint;
    isLiftBotInCamera = _camera.Project3dPoint(liftBotWrtCamera, liftBotCameraPoint);
    liftBotY = liftBotCameraPoint.y();

    if (kDebugRenderBboxVsLift) {
      _vizManager->DrawCameraOval(liftTopCameraPoint, 3, 3, NamedColors::YELLOW);
      _vizManager->DrawCameraOval(liftBotCameraPoint, 3, 3, NamedColors::YELLOW);
    }
  }

  // render ground plane Y if needed
  const int planeTopY = bbox.GetY();
  const int planeBotY = bbox.GetYmax();
  if (kDebugRenderBboxVsLift) {
    _vizManager->DrawCameraOval(Anki::Point2f{120, planeTopY}, 3, 3, NamedColors::WHITE);
    _vizManager->DrawCameraOval(Anki::Point2f{120, planeBotY}, 3, 3, NamedColors::WHITE);
  }

  // check if the lift interferes with the edge detection, and if so, do not detect edges
  const bool liftInterferesWithEdges = LiftInterferesWithEdges(isLiftTopInCamera, liftTopY, isLiftBotInCamera, liftBotY,
                                                               planeTopY, planeBotY);
  if (liftInterferesWithEdges) {
    return RESULT_OK;
  }

  // we are going to detect edges, grab relevant image
  typename ImageTraitType::ImageType imageROI = image.GetROI(bbox);

  // Find edges in that ROI
  // Custom Gaussian derivative in x direction, sigma=1, with a little extra space
  // in the middle to help detect soft edges
  // (scaled such that each half has absolute sum of 1.0, so it's normalized)
  _profiler.Tic("EdgeDetection");
  static const SmallMatrix<7, 5, f32> kernel{
      0.0168, 0.0754, 0.1242, 0.0754, 0.0168,
      0.0377, 0.1689, 0.2784, 0.1689, 0.0377,
      0, 0, 0, 0, 0,
      0, 0, 0, 0, 0,
      0, 0, 0, 0, 0,
      -0.0377, -0.1689, -0.2784, -0.1689, -0.0377,
      -0.0168, -0.0754, -0.1242, -0.0754, -0.0168,
  };

  /*
   const SmallMatrix<7, 5, s16> kernel{
   9,    39,    64,    39,     9,
   19,    86,   143,    86,    19,
   0,0,0,0,0,
   0,0,0,0,0,
   0,0,0,0,0,
   -19,   -86,  -143,   -86,   -19,
   -9,   -39,   -64,   -39,    -9
   };
   */

  Array2d<typename ImageTraitType::SPixelType> edgeImgX(image.GetNumRows(), image.GetNumCols());
  cv::filter2D(imageROI.get_CvMat_(), edgeImgX.GetROI(bbox).get_CvMat_(), CV_16S, kernel.get_CvMatx_());
  _profiler.Toc("EdgeDetection");

  _profiler.Tic("GroundQuadEdgeMasking");
  // Remove edges that aren't in the ground plane quad (as opposed to its bounding rectangle)
  Vision::Image mask(edgeImgX.GetNumRows(), edgeImgX.GetNumCols());
  mask.FillWith(255);
  cv::fillConvexPoly(mask.get_CvMat_(), std::vector<cv::Point>{
      groundInImage[Quad::CornerName::TopLeft].get_CvPoint_(),
      groundInImage[Quad::CornerName::TopRight].get_CvPoint_(),
      groundInImage[Quad::CornerName::BottomRight].get_CvPoint_(),
      groundInImage[Quad::CornerName::BottomLeft].get_CvPoint_(),
  }, 0);

  edgeImgX.SetMaskTo(mask, 0);
  _profiler.Toc("GroundQuadEdgeMasking");

  // create edge frame info to send
  OverheadEdgeFrame edgeFrame;
  OverheadEdgeChainVector& candidateChains = edgeFrame.chains;
  
  // Find first strong edge in each column, in the ground plane mask, working
  // upward from bottom.
  // Note: looping only over the ROI portion of full image, but working in
  //       full-image coordinates so that H directly applies
  // Note: transposing so we can work along rows, which is more efficient.
  //       (this also means using bbox.X for transposed rows and bbox.Y for transposed cols)
  _profiler.Tic("FindingGroundEdgePoints");
  Matrix_3x3f invH = H.GetInverse();
  Array2d<typename ImageTraitType::FPixelType> edgeTrans(edgeImgX.get_CvMat_().t());
  OverheadEdgePoint edgePoint;
  for (s32 i = bbox.GetX(); i < bbox.GetXmax(); ++i)
  {
    bool foundBorder = false;
    const typename ImageTraitType::FPixelType *edgeTrans_i = edgeTrans.GetRow(i);

    // Right to left in transposed image ==> bottom to top in original image
    for (s32 j = bbox.GetYmax() - 1; j >= bbox.GetY(); --j) {
      const typename ImageTraitType::FPixelType &edgePixelX = edgeTrans_i[j];

      if (CheckThreshold(edgePixelX, _kEdgeThreshold)) {
        // Project point onto ground plane
        // Note that b/c we are working transposed, i is x and j is y in the
        // original image.
        const bool success = SetEdgePosition(invH, i, j, edgePoint);
        if (success) {

          //type dependent code, use anonymous struct and overload
          struct {
            Vec3f operator()(const f32 pixel) {
              return Vec3f(pixel, pixel, pixel);
            };
            Vec3f operator()(const Vision::PixelRGB_<f32>& pixel) {
              return Vec3f(pixel.r(), pixel.g(), pixel.b());
            };
          } getGradient;

          edgePoint.gradient = getGradient(edgePixelX);
          foundBorder = true;
          candidateChains.AddEdgePoint(edgePoint, foundBorder);
        }
        break; // only keep first edge found in each row (working right to left)
      }
    }

    // if we did not find border, report lack of border for this row
    if (!foundBorder) {
      const bool isInsideGroundQuad = (i >= groundInImage[Quad::TopLeft].x() &&
                                       i <= groundInImage[Quad::TopRight].x());

      if (isInsideGroundQuad) {
        // Project point onto ground plane
        // Note that b/c we are working transposed, i is x and j is y in the
        // original image.
        const bool success = SetEdgePosition(invH, i, bbox.GetY(), edgePoint);
        if (success) {
          edgePoint.gradient = 0.0f;
          candidateChains.AddEdgePoint(edgePoint, foundBorder);
        }
      }
    }

  }
  _profiler.Toc("FindingGroundEdgePoints");

  #define DRAW_OVERHEAD_IMAGE_EDGES_DEBUG 0
  if (DRAW_OVERHEAD_IMAGE_EDGES_DEBUG) {
    Vision::ImageRGB overheadImg = roi.GetOverheadImage(image, H);

    static const std::vector<ColorRGBA> lineColorList = {
        NamedColors::RED, NamedColors::GREEN, NamedColors::BLUE,
        NamedColors::ORANGE, NamedColors::CYAN, NamedColors::YELLOW,
    };
    auto color = lineColorList.begin();
    Vision::ImageRGB dispImg(overheadImg.GetNumRows(), overheadImg.GetNumCols());
    overheadImg.CopyTo(dispImg);
    static const Anki::Point2f dispOffset(-roi.GetDist(), roi.GetWidthFar() * 0.5f);
    Quad2f tempQuad(roi.GetGroundQuad());
    tempQuad += dispOffset;
    dispImg.DrawQuad(tempQuad, NamedColors::RED, 1);

    for (const auto &chain : candidateChains.GetVector()) {
      if (chain.points.size() >= _kMinChainLength) {
        for (s32 i = 1; i < chain.points.size(); ++i) {
          Anki::Point2f startPoint(chain.points[i - 1].position);
          startPoint.y() = -startPoint.y();
          startPoint += dispOffset;
          Anki::Point2f endPoint(chain.points[i].position);
          endPoint.y() = -endPoint.y();
          endPoint += dispOffset;
          dispImg.DrawLine(startPoint, endPoint, *color, 1);
        }
        ++color;
        if (color == lineColorList.end()) {
          color = lineColorList.begin();
        }
      }
    }
    typename ImageTraitType::ImageType dispEdgeImg(edgeImgX.GetNumRows(), edgeImgX.GetNumCols());

    // The behavior depends on the type of the pixel, so anonym struct and overload
    struct {
      Vision::PixelRGB operator()(const Vision::PixelRGB_<s16>& pixelS16) {
        return Vision::PixelRGB((u8)std::abs(pixelS16.r()),
                                (u8)std::abs(pixelS16.g()),
                                (u8)std::abs(pixelS16.b()));
      }
      u8 operator()(const s16& pixelS16) {
        return u8(std::abs(pixelS16));
      }
    } fcn_struct;

    //still needs to create an std::function here, the compiler doesn't get it
    std::function<typename ImageTraitType::UPixelType(const typename ImageTraitType::SPixelType&)> fcn = fcn_struct;
    edgeImgX.ApplyScalarFunction(fcn, dispEdgeImg);

    // Project edges on the ground back into image for display
    for (const auto &chain : candidateChains.GetVector()) {
      for (s32 i = 0; i < chain.points.size(); ++i) {
        const Anki::Point2f &groundPoint = chain.points[i].position;
        Point3f temp = H * Anki::Point3f(groundPoint.x(), groundPoint.y(), 1.f);
        DEV_ASSERT(temp.z() > 0.f, "VisionSystem.DetectOverheadEdges.BadDisplayZ");
        const f32 divisor = 1.f / temp.z();
        if(chain.isBorder) {
          dispEdgeImg.DrawCircle({temp.x() * divisor, temp.y() * divisor}, NamedColors::RED, 1);
        } else {
          dispEdgeImg.DrawCircle({temp.x() * divisor, temp.y() * divisor}, NamedColors::WHITE, 1);
        }
      }
    }
    dispEdgeImg.DrawQuad(groundInImage, NamedColors::GREEN, 1);
    //dispImg.Display("OverheadImage", 1);
    //dispEdgeImg.Display("OverheadEdgeImage");
    currentResult.debugImages.emplace_back("OverheadImage", dispImg);
    currentResult.debugImages.emplace_back("EdgeImage", dispEdgeImg);
  } // if(DRAW_OVERHEAD_IMAGE_EDGES_DEBUG)

  edgeFrame.timestamp = image.GetTimestamp();
  edgeFrame.groundPlaneValid = true;

  roi.GetVisibleGroundQuad(H, image.GetNumCols(), image.GetNumRows(), edgeFrame.groundplane);

  // Copy only the chains with at least k points (less is considered noise)
  edgeFrame.chains.RemoveChainsShorterThan(_kMinChainLength);
  
  // Transform border points into 3D, and into camera view and render
  static const bool kRenderEdgesInCameraView = false;
  if (kRenderEdgesInCameraView) {
    _vizManager->EraseSegments("kRenderEdgesInCameraView");
    for (const auto &chain : edgeFrame.chains.GetVector()) {
      if (!chain.isBorder) {
        continue;
      }
      for (const auto &point : chain.points) {
        // project the point to 3D
        const Pose3d pointAt3D(0.f, Y_AXIS_3D(), Point3f(point.position.x(), point.position.y(), 0.0f),
                         crntPoseData.histState.GetPose(), "ChainPoint");
        const Pose3d pointWrtOrigin = pointAt3D.GetWithRespectToRoot();
        // disabled 3D render
        // _vizManager->DrawSegment("kRenderEdgesInCameraView", pointWrtOrigin.GetTranslation(), pointWrtOrigin.GetTranslation() + Vec3f{0,0,30}, NamedColors::WHITE, false);

        // project it back to 2D
        Pose3d pointWrtCamera;
        if (pointWrtOrigin.GetWithRespectTo(crntPoseData.cameraPose, pointWrtCamera)) {
          Anki::Point2f pointInCameraView;
          _camera.Project3dPoint(pointWrtCamera.GetTranslation(), pointInCameraView);
          _vizManager->DrawCameraOval(pointInCameraView, 1, 1, NamedColors::BLUE);
        }
      }
    }
  }

  // put in mailbox
  currentResult.overheadEdges.push_back(std::move(edgeFrame));

  return RESULT_OK;
}

namespace {

inline bool SetEdgePosition(const Matrix_3x3f& invH,
                            const s32 i, const s32 j,
                            OverheadEdgePoint& edgePoint)
{
  // Project point onto ground plane
  // Note that b/c we are working transposed, i is x and j is y in the
  // original image.
  const Point3f temp = invH * Point3f(i, j, 1.f);
  if(temp.z() <= 0.f) {
    PRINT_NAMED_WARNING("VisionSystem.SetEdgePositionHelper.BadProjectedZ", "z=%f", temp.z());
    return false;
  }

  const f32 divisor = 1.f / temp.z();

  edgePoint.position.x() = temp.x() * divisor;
  edgePoint.position.y() = temp.y() * divisor;
  return true;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
bool LiftInterferesWithEdges(bool isLiftTopInCamera, float liftTopY,
                             bool isLiftBotInCamera, float liftBotY,
                             int planeTopY, int planeBotY)
{
  // note that top in an image is a smaller value than bottom because 0,0 starts at left,top corner, so
  // you may find '>' and '<' confusing. They should appear reversed with respect to what you would think.
  bool ret = false;

  // enable debug
  // #define DEBUG_LIFT_INTERFERES_WITH_EDGES(x) printf("[D_LIFT_EDGES] %s", x);
  // disable debug
#define DEBUG_LIFT_INTERFERES_WITH_EDGES(x)

  if ( !isLiftTopInCamera )
  {
    if ( !isLiftBotInCamera )
    {
      // neither end of the lift is in the camera, we are good
      DEBUG_LIFT_INTERFERES_WITH_EDGES("(OK) Lift is too low or too high, all good\n");
    }
    else
    {
      // bottom end of the lift is in the camera, check if it's beyond the bbox
      if ( liftBotY < planeTopY )
      {
        // bottom end of the lift is above the top of the ground plane, so the lift is above the camera
        DEBUG_LIFT_INTERFERES_WITH_EDGES("(OK) Lift is high, all good\n");
      }
      else
      {
        // the bottom end of the lift is in the camera, and actually in the ground plane projection. This could
        // cause edge detection on the lift itself.
        DEBUG_LIFT_INTERFERES_WITH_EDGES("(BAD) Bottom border of the lift interferes with edges\n");
        ret = true;
      }
    }
  }
  else {
    // lift top is in the camera, check how far into the ground plane
    if ( liftTopY > planeBotY )
    {
      // the top of the lift is below the bottom of the ground plane, we are good
      DEBUG_LIFT_INTERFERES_WITH_EDGES("Lift is low, all good\n");
    }
    else {
      // top of the lift is above the bottom ground plane, check if bottom of the lift is above the top of the plane
      if ( !isLiftBotInCamera ) {
        // bottom of the lift is not in the camera, since bottom is below the top (duh), and the top was in the camera,
        // this means we can see the top of the lift and it interferes with edges, but we can't see the bottom.
        DEBUG_LIFT_INTERFERES_WITH_EDGES("(BAD) Lift is slightly interfering\n");
        ret = true;
      }
      else
      {
        // we can also see the bottom of the lift, check how far into the ground plane
        if ( liftBotY < planeTopY )
        {
          // the bottom of the lift is above the top of the ground plane, we are good
          DEBUG_LIFT_INTERFERES_WITH_EDGES("We can see the lift, but it's above the ground plane, all good\n");
        }
        else
        {
          DEBUG_LIFT_INTERFERES_WITH_EDGES("(BAD) Lift interferes with edges\n");
          ret = true;
        }
      }
    }
  }
  return ret;
}

bool CheckThreshold(Vision::PixelRGB_<f32> pixel, f32 threshold)
{
  return std::abs(pixel.r()) > threshold ||
         std::abs(pixel.g()) > threshold ||
         std::abs(pixel.b()) > threshold;
}

bool CheckThreshold(f32 pixel, f32 threshold)
{
  return std::abs(pixel) > threshold;
}

} // anonymous namespace

// Explicit instantiation of DetectHelper() method for Gray and RGB images
template
Result OverheadEdgesDetector::DetectHelper<ImageGrayTrait>(const Vision::Image &image,
                                                           const VisionPoseData &crntPoseData,
                                                           VisionProcessingResult &currentResult);

template
Result OverheadEdgesDetector::DetectHelper<ImageRGBTrait>(const Vision::ImageRGB &image,
                                                          const VisionPoseData &crntPoseData,
                                                          VisionProcessingResult &currentResult);

} //namespace Vector
} //namespace Anki
