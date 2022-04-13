/**
 * File: cameraCalibrator.cpp
 *
 * Author: Al Chaussee
 * Created: 08/16/17
 *
 * Description:
 *
 * Copyright: Anki, Inc. 2017
 *
 **/

#include "engine/vision/cameraCalibrator.h"

#include "coretech/common/engine/math/pose.h"
#include "coretech/common/engine/math/quad.h"
#include "coretech/common/shared/math/rotation.h"

#include "anki/cozmo/shared/cozmoConfig.h"

#include "opencv2/calib3d/calib3d.hpp"
#include "opencv2/imgproc.hpp"

#include "util/console/consoleInterface.h"
#include "util/helpers/cleanupHelper.h"

#include <set>

#define BLEACHER_CALIB_MARKER_SIZE_MM 14.f
#define BLEACHER_CALIB_TARGET_FACE_SIZE_MM 20.f

#define INVERTEDBOX_CALIB_MARKER_SIZE_MM 15.f
#define INVERTEDBOX_CALIB_TARGET_FACE_SIZE_MM 30.f

#define DRAW_CALIB_IMAGES 0

namespace Anki {
namespace Vector {

// Min/max size of calibration pattern blobs and distance between them
CONSOLE_VAR(float, kMaxCalibBlobPixelArea,         "Vision.Calibration", 800.f);
CONSOLE_VAR(float, kMinCalibBlobPixelArea,         "Vision.Calibration", 20.f);
CONSOLE_VAR(float, kMinCalibPixelDistBetweenBlobs, "Vision.Calibration", 5.f);
CONSOLE_VAR(bool,  kDrawCalibImages,               "Vision.Calibration", false);
CONSOLE_VAR(u32,   kMinNumCalibImages,             "Vision.Calibration", 1);
CONSOLE_VAR(u32,   kCheckerboardWidth,             "Vision.Calibration", 11);
CONSOLE_VAR(u32,   kCheckerboardHeight,            "Vision.Calibration", 4);
CONSOLE_VAR(f32,   kCheckerboardSquareSize_mm,     "Vision.Calibration", 0.05);
CONSOLE_VAR(f32,   kSingleTargetReprojErr_pix,     "Vision.Calibration", 1.5);

// TODO Figure out min number of markers (what if top row is cut off thats like 12 markers)
CONSOLE_VAR(u32,   kNumMarkersNeededForCalibration,"Vision.Calibration", 10);

namespace {
static const char* const kLogChannelName = "CameraCalibrator";
}
    
CameraCalibrator::CameraCalibrator()
{
  
}

CameraCalibrator::~CameraCalibrator()
{
  
}

Result CameraCalibrator::ComputeCalibrationFromCheckerboard(std::list<Vision::CameraCalibration>& calibration_out,
                                                            Vision::DebugImageList<Vision::CompressedImage>& debugImages_out)
{
  std::unique_ptr<Vision::CameraCalibration> calibration;
  _isCalibrating = true;
  
  // Guarantee Calibration mode gets disabled and computed calibration gets sent
  // no matter how we return from this function
  Util::CleanupHelper disableCalibration([this, &calibration_out, &calibration]() {
    if(calibration == nullptr)
    {
      PRINT_NAMED_WARNING("CameraCalibrator.ComputeCalibrationFromCheckerboard.NullCalibration", "");
    }
    else
    {
      calibration_out.push_back(*calibration);
    }
    _isCalibrating = false;
  });
  
  // Check that there are enough images
  if (_calibImages.size() < kMinNumCalibImages)
  {
    PRINT_CH_INFO(kLogChannelName, "CameraCalibrator.ComputeCalibrationFromCheckerboard.NotEnoughImages",
                  "Got %u. Need %u.", (u32)_calibImages.size(), kMinNumCalibImages);
    
    return RESULT_FAIL;
  }
  
  PRINT_CH_INFO(kLogChannelName, "CameraCalibrator.ComputeCalibrationFromCheckerboard.NumImages",
                "%u.", (u32)_calibImages.size());
  
  // Description of asymmetric circles calibration target
  const cv::Size boardSize(kCheckerboardHeight, kCheckerboardWidth);
  const Vision::Image& firstImg = _calibImages.front().img;
  cv::Size imageSize(firstImg.GetNumCols(), firstImg.GetNumRows());
  
  std::vector<std::vector<cv::Point2f> > imagePoints;
  std::vector<std::vector<cv::Point3f> > objectPoints(1);
  
  // Parameters for circle grid search
  cv::SimpleBlobDetector::Params params;
  params.maxArea = kMaxCalibBlobPixelArea;
  params.minArea = kMinCalibBlobPixelArea;
  params.minDistBetweenBlobs = kMinCalibPixelDistBetweenBlobs;
  cv::Ptr<cv::SimpleBlobDetector> blobDetector = cv::SimpleBlobDetector::create(params);
  const int findCirclesFlags = cv::CALIB_CB_ASYMMETRIC_GRID | cv::CALIB_CB_CLUSTERING;
  
  int imgCnt = 0;
  Vision::Image img(firstImg.GetNumRows(), firstImg.GetNumCols());
  for (auto & calibImage : _calibImages)
  {
    // Extract the ROI (leaving the rest as zeros)
    img.FillWith(0);
    Vision::Image imgROI = img.GetROI(calibImage.roiRect);
    calibImage.img.GetROI(calibImage.roiRect).CopyTo(imgROI);
    
    // Get image points
    std::vector<cv::Point2f> pointBuf;
    calibImage.dotsFound = cv::findCirclesGrid(img.get_CvMat_(), boardSize, pointBuf, findCirclesFlags, blobDetector);
    
    if (calibImage.dotsFound)
    {
      PRINT_CH_INFO(kLogChannelName, "CameraCalibrator.ComputeCalibrationFromCheckerboard.FoundPoints", "");
      imagePoints.push_back(pointBuf);
    }
    else
    {
      PRINT_CH_INFO(kLogChannelName, "CameraCalibrator.ComputeCalibrationFromCheckerboard.NoPointsFound", "");
    }
    
    
    // Draw image
    if(kDrawCalibImages)
    {
      Vision::ImageRGB dispImg;
      cv::cvtColor(img.get_CvMat_(), dispImg.get_CvMat_(), cv::COLOR_GRAY2BGR);
      
      if (calibImage.dotsFound)
      {
        cv::drawChessboardCorners(dispImg.get_CvMat_(), boardSize, cv::Mat(pointBuf), calibImage.dotsFound);
      }
      
      debugImages_out.emplace_back(std::string("CalibImage") + std::to_string(imgCnt), dispImg);
    }
    
    ++imgCnt;
  }
  
  // Were points found in enough of the images?
  if(imagePoints.size() < kMinNumCalibImages)
  {
    PRINT_CH_INFO(kLogChannelName,
                  "CameraCalibrator.ComputeCalibrationFromCheckerboard.InsufficientImagesWithPoints",
                  "Points detected in only %u images. Need %u.",
                  (u32)imagePoints.size(), kMinNumCalibImages);
    
    return RESULT_FAIL;
  }
  
  // Get object points
  CalcBoardCornerPositions(boardSize, kCheckerboardSquareSize_mm, objectPoints[0]);
  objectPoints.resize(imagePoints.size(), objectPoints[0]);
  
  // Compute calibration
  std::vector<cv::Vec3d> rvecs, tvecs;
  cv::Mat_<f64> cameraMatrix = cv::Mat_<f64>::eye(3, 3);
  cv::Mat_<f64> distCoeffs   = cv::Mat_<f64>::zeros(1, NUM_RADIAL_DISTORTION_COEFFS);
  
  const f64 rms = cv::calibrateCamera(objectPoints, imagePoints, imageSize, cameraMatrix, distCoeffs, rvecs, tvecs);
  
  // Copy distortion coefficients into a f32 vector to set CameraCalibration
  const f64* distCoeffs_data = distCoeffs[0];
  Vision::CameraCalibration::DistortionCoeffs distCoeffsVec;
  std::copy(distCoeffs_data, distCoeffs_data+NUM_RADIAL_DISTORTION_COEFFS, distCoeffsVec.begin());
  
  calibration.reset(new Vision::CameraCalibration(imageSize.height, imageSize.width,
                                                  cameraMatrix(0,0), cameraMatrix(1,1),
                                                  cameraMatrix(0,2), cameraMatrix(1,2),
                                                  0.f, // skew
                                                  distCoeffsVec));
  
  DEV_ASSERT_MSG(rvecs.size() == tvecs.size(),
                 "CameraCalibrator.ComputeCalibrationFromCheckerboard.BadCalibPoseData",
                 "Got %zu rotations and %zu translations",
                 rvecs.size(), tvecs.size());
  
  _calibPoses.reserve(rvecs.size());
  
  for(s32 iPose=0; iPose<rvecs.size(); ++iPose)
  {
    auto rvec = rvecs[iPose];
    auto tvec = tvecs[iPose];
    RotationVector3d R(Vec3f(rvec[0], rvec[1], rvec[2]));
    Vec3f T(tvec[0], tvec[1], tvec[2]);
    
    _calibPoses.emplace_back(Pose3d(R, T));
  }
  
  PRINT_CH_INFO(kLogChannelName,
                "CameraCalibrator.ComputeCalibrationFromCheckerboard.CalibValues",
                "fx: %f, fy: %f, cx: %f, cy: %f (rms %f)",
                calibration->GetFocalLength_x(), calibration->GetFocalLength_y(),
                calibration->GetCenter_x(), calibration->GetCenter_y(), rms);
  
  // Check if average reprojection error is too high
  const f64 reprojErrThresh_pix = 0.5;
  if (rms > reprojErrThresh_pix)
  {
    PRINT_CH_INFO(kLogChannelName,
                  "CameraCalibrator.ComputeCalibrationFromCheckerboard.ReprojectionErrorTooHigh",
                  "%f > %f", rms, reprojErrThresh_pix);
    return RESULT_FAIL;
  }
  
  return RESULT_OK;
}

Result CameraCalibrator::ComputeCalibrationFromSingleTarget(CalibTargetType targetType,
                                                            const std::list<Vision::ObservedMarker>& observedMarkers,
                                                            std::list<Vision::CameraCalibration>& calibration_out,
                                                            Vision::DebugImageList<Vision::CompressedImage>& debugImages_out)
{
  std::unique_ptr<Vision::CameraCalibration> calibration;
  _isCalibrating = true;
  
  // Guarantee Calibration mode gets disabled and computed calibration gets sent
  // no matter how we return from this function
  Util::CleanupHelper disableCalibration([this, &calibration_out, &calibration]() {
    if(calibration == nullptr)
    {
      PRINT_NAMED_WARNING("CameraCalibrator.ComputeCalibrationFromSingleTarget.NullCalibration", "");
    }
    else
    {
      calibration_out.push_back(*calibration);
    }
    _isCalibrating = false;
  });
  
  // Check that there are enough markers
  if(observedMarkers.size() < kNumMarkersNeededForCalibration)
  {
    PRINT_NAMED_WARNING("CameraCalibrator.ComputeCalibrationFromSingleTarget.NotEnoughMarkers",
                        "Seeing only %zu markers, need to be seeing at least %d",
                        observedMarkers.size(),
                        kNumMarkersNeededForCalibration);
    return RESULT_FAIL;
  }
  
  std::map<Vision::MarkerType, Quad3f> markersTo3dCoords;
  std::set<Vision::MarkerType> markersNeededToBeSeen;
  
  switch(targetType)
  {
    case INVERTED_BOX:
    {
      GetCalibTargetMarkersTo3dCoords_InvertedBox(markersTo3dCoords, markersNeededToBeSeen);
      break;
    }
    case QBERT:
    {
      GetCalibTargetMarkersTo3dCoords_Qbert(markersTo3dCoords, markersNeededToBeSeen);
      break;
    }
    case CHECKERBOARD:
    {
      PRINT_NAMED_WARNING("CameraCalibrator.ComputeCalibrationFromSingleTarget.InvalidTarget", "");
      return RESULT_FAIL;
    }
  }
  
  // For each marker we should have 4 points (each corner of the marker)
  std::vector<cv::Vec2f> imgPts;
  imgPts.reserve(observedMarkers.size() * 4);
  
  std::vector<cv::Vec3f> worldPts;
  worldPts.reserve(observedMarkers.size() * 4);
  
  std::set<Vision::Marker::Code> codes;
  for(const auto& marker : observedMarkers)
  {
    if(codes.count(marker.GetCode()) != 0)
    {
      PRINT_NAMED_WARNING("CameraCalibrator.ComputeCalibrationFromSingleTarget.MultipleMarkersWithSameCode",
                          "Observed multiple markers with code %s",
                          marker.GetCodeName());
      return RESULT_FAIL;
    }
    
    codes.insert(marker.GetCode());
    const auto& iter = markersTo3dCoords.find(static_cast<Vision::MarkerType>(marker.GetCode()));
    
    if(iter != markersTo3dCoords.end())
    {
      const auto& corners = marker.GetImageCorners();
      
      imgPts.emplace_back(corners.GetTopLeft().x(),
                          corners.GetTopLeft().y());
      worldPts.emplace_back(iter->second.GetTopLeft().x(),
                            iter->second.GetTopLeft().y(),
                            iter->second.GetTopLeft().z());
      
      imgPts.emplace_back(corners.GetTopRight().x(),
                          corners.GetTopRight().y());
      worldPts.emplace_back(iter->second.GetTopRight().x(),
                            iter->second.GetTopRight().y(),
                            iter->second.GetTopRight().z());
      
      imgPts.emplace_back(corners.GetBottomLeft().x(),
                          corners.GetBottomLeft().y());
      worldPts.emplace_back(iter->second.GetBottomLeft().x(),
                            iter->second.GetBottomLeft().y(),
                            iter->second.GetBottomLeft().z());
      
      imgPts.emplace_back(corners.GetBottomRight().x(),
                          corners.GetBottomRight().y());
      worldPts.emplace_back(iter->second.GetBottomRight().x(),
                            iter->second.GetBottomRight().y(),
                            iter->second.GetBottomRight().z());
    }
  }
  
  std::stringstream ss;
  for(const auto& marker : markersTo3dCoords)
  {
    if(codes.count(marker.first) == 0)
    {
      ss << Vision::MarkerTypeStrings[marker.first] << " ";
    }
  }
  
  std::string s = ss.str();
  if(!s.empty())
  {
    PRINT_CH_INFO(kLogChannelName, "CameraCalibrator.ComputeCalibrationFromSingleTarget.MarkersNotSeen",
                                   "Expected to see the following markers but didnt %s",
                                   s.c_str());
  }
  
  ss.str(std::string());
  for(const auto& marker : markersNeededToBeSeen)
  {
    if(codes.count(marker) == 0)
    {
      ss << Vision::MarkerTypeStrings[marker] << " ";
    }
  }
  
  s = ss.str();
  if(!s.empty())
  {
    PRINT_NAMED_ERROR("CameraCalibrator.ComputeCalibrationFromSingleTarget.MissingMarkers",
                      "Needed to see the following markers but didnt %s",
                      s.c_str());
    return RESULT_FAIL;
  }
  
  if(DRAW_CALIB_IMAGES)
  {
    const Vision::Image& img = _calibImages[0].img;
    
    Vision::ImageRGB dispImg;
    cv::cvtColor(img.get_CvMat_(), dispImg.get_CvMat_(), cv::COLOR_GRAY2BGR);
    for(int i =0; i < imgPts.size(); ++i)
    {
      const auto& p = imgPts[i];
      dispImg.DrawFilledCircle({p[0], p[1]}, NamedColors::RED, 2);
    }
    debugImages_out.emplace_back("CalibImage", dispImg);
  }
  
  // Depending on what type of robot we are running, provide a different initial guess for calibration
#ifdef SIMULATOR
  cv::Mat_<f64> cameraMatrix = (cv::Mat_<f64>(3,3) <<
                                507, 0, 639,
                                0, 507, 359,
                                0, 0, 1);
  
  cv::Mat_<f64> distCoeffs = (cv::Mat_<double>(1, NUM_RADIAL_DISTORTION_COEFFS) <<
                              -0.07f, -0.2f, 0.001f, 0.001f, 0.1f, 0.f, 0.f, 0.f);
#else
  cv::Mat_<f64> cameraMatrix = (cv::Mat_<double>(3,3) <<
                                362, 0, 303,
                                0, 364, 196,
                                0, 0, 1);
  
  cv::Mat_<f64> distCoeffs = (cv::Mat_<double>(1, NUM_RADIAL_DISTORTION_COEFFS) <<
                              -0.1, -0.1, 0.00005, -0.0001, 0.05, 0, 0, 0);
#endif
  
  std::vector<cv::Vec3d> rvecs, tvecs;
  std::vector<std::vector<cv::Vec2f>> vecOfImgPts;
  std::vector<std::vector<cv::Vec3f>> vecOfWorldPts;
  vecOfImgPts.push_back(imgPts);
  vecOfWorldPts.push_back(worldPts);
  
  const s32 numRows = DEFAULT_CAMERA_RESOLUTION_HEIGHT;
  const s32 numCols = DEFAULT_CAMERA_RESOLUTION_WIDTH;
  
  f64 rms = 0;
  try
  {
    rms = cv::calibrateCamera(vecOfWorldPts, vecOfImgPts, cv::Size(numCols, numRows),
                              cameraMatrix, distCoeffs, rvecs, tvecs,
                              cv::CALIB_USE_INTRINSIC_GUESS);
  }
  catch(cv::Exception& e)
  {
    PRINT_NAMED_ERROR("CameraCalibrator.ComputeCalibrationFromSingleTarget.OpenCVError",
                      "%s",
                      e.what());
    return RESULT_FAIL;
  }
  
  ss.str(std::string());
  ss << cameraMatrix << std::endl;
  PRINT_CH_INFO(kLogChannelName,
                "CameraCalibrator.ComputeCalibrationFromSingleImage.K",
                "%s", ss.str().c_str());
  
  ss.str(std::string());
  ss << distCoeffs << std::endl;
  PRINT_CH_INFO(kLogChannelName,
                "CameraCalibrator.ComputeCalibrationFromSingleImage.D",
                "%s", ss.str().c_str());
  
  const f64* distCoeffs_data = distCoeffs[0];
  std::array<f32,NUM_RADIAL_DISTORTION_COEFFS> distCoeffsVec;
  distCoeffsVec.fill(0.f);
  std::copy(distCoeffs_data, distCoeffs_data+distCoeffs.cols, distCoeffsVec.begin());
  
  calibration.reset(new Vision::CameraCalibration(numRows, numCols,
                                                  cameraMatrix(0,0), cameraMatrix(1,1),
                                                  cameraMatrix(0,2), cameraMatrix(1,2),
                                                  0.f, // skew
                                                  distCoeffsVec));
  
  DEV_ASSERT_MSG(rvecs.size() == tvecs.size(),
                 "VisionSystem.ComputeCalibrationFromSingleTarget.BadCalibPoseData",
                 "Got %zu rotations and %zu translations",
                 rvecs.size(), tvecs.size());
  
  PRINT_CH_INFO(kLogChannelName, "CameraCalibrator.ComputeCalibrationFromSingleTarget.CalibValues",
                "fx: %f, fy: %f, cx: %f, cy: %f (rms %f)",
                calibration->GetFocalLength_x(), calibration->GetFocalLength_y(),
                calibration->GetCenter_x(), calibration->GetCenter_y(), rms);
  
  
  // Check if average reprojection error is too high
  if (rms > kSingleTargetReprojErr_pix)
  {
    PRINT_NAMED_WARNING("CameraCalibrator.ComputeCalibrationFromSingleTarget.ReprojectionErrorTooHigh",
                        "%f > %f", rms, kSingleTargetReprojErr_pix);
    return RESULT_FAIL;
  }
  
  return RESULT_OK;
}

Result CameraCalibrator::AddCalibrationImage(const Vision::Image& calibImg,
                                             const Rectangle<s32>& targetROI)
{
  if(_isCalibrating)
  {
    PRINT_CH_INFO(kLogChannelName,"CameraCalibrator.AddCalibrationImage.AlreadyCalibrating",
                  "Cannot add calibration image while already in the middle of doing calibration.");
    
    return RESULT_FAIL;
  }
  
  if(targetROI.GetX() < 0 && targetROI.GetY() < 0 && targetROI.GetWidth() == 0 && targetROI.GetHeight() == 0)
  {
    // Use entire image if negative ROI specified
    const Anki::Rectangle<s32> entireImgROI(0, 0, calibImg.GetNumCols(), calibImg.GetNumRows());
    _calibImages.push_back({.img = calibImg, .roiRect = entireImgROI, .dotsFound = false});
  } 
  else 
  {
    _calibImages.push_back({.img = calibImg, .roiRect = targetROI, .dotsFound = false});
  }

  PRINT_CH_INFO(kLogChannelName, "CameraCalibrator.AddCalibrationImage",
                "Num images including this: %u", (u32)_calibImages.size());

  return RESULT_OK;
}

Result CameraCalibrator::ClearCalibrationImages()
{
  if(_isCalibrating)
  {
    PRINT_CH_INFO(kLogChannelName, "CameraCalibrator.ClearCalibrationImages.AlreadyCalibrating",
                  "Cannot clear calibration images while already in the middle of doing calibration.");
    
    return RESULT_FAIL;
  }
  
  _calibImages.clear();
  
  return RESULT_OK;
}


void CameraCalibrator::CalcBoardCornerPositions(cv::Size boardSize,
                                                float squareSize,
                                                std::vector<cv::Point3f>& corners)
{
  corners.clear();
  
  for( int i = 0; i < boardSize.height; i++ )
  {
    for( int j = 0; j < boardSize.width; j++ )
    {
      corners.push_back(cv::Point3f(float((2*j + i % 2)*squareSize), float(i*squareSize), 0));
    }
  }
}

void CameraCalibrator::GetCalibTargetMarkersTo3dCoords_Qbert(std::map<Vision::MarkerType, Quad3f>& markersTo3dCoords,
                                                             std::set<Vision::MarkerType>& markersNeededToBeSeen)
{
  markersTo3dCoords.clear();

  /*
  Top down view of bottom row
   _
  | |
   - _
    | |
     - _
      | |
       - ...
   
   ^
   Robot
   
   ^ +y
   |
   -> +x
   +z out
   
   Marker corners are defined relative to center of bottom left cube of the target in this orientation
    (before rotations are applied to get cubes to their actual positions)
   FrontFace is the marker that is facing the robot in this orientation. 
   !FrontMarker is the left marker, that will be visible when rotation are applied 
    (rotate 45 degree on Z and then -30 degree in Y in this origin)
   */

#if FACTORY_TEST
  const f32 halfMarkerSize_mm = BLEACHER_CALIB_MARKER_SIZE_MM / 2.f;
  const f32 halfTargetFace_mm = BLEACHER_CALIB_TARGET_FACE_SIZE_MM / 2.f;
  const Quad3f originsFrontFace({
    {-halfMarkerSize_mm, -halfTargetFace_mm,  halfMarkerSize_mm},
    {-halfMarkerSize_mm, -halfTargetFace_mm, -halfMarkerSize_mm},
    { halfMarkerSize_mm, -halfTargetFace_mm,  halfMarkerSize_mm},
    { halfMarkerSize_mm, -halfTargetFace_mm, -halfMarkerSize_mm}
  });
  
  const Quad3f originsLeftFace({
    {-halfTargetFace_mm,  halfMarkerSize_mm,  halfMarkerSize_mm},
    {-halfTargetFace_mm,  halfMarkerSize_mm, -halfMarkerSize_mm},
    {-halfTargetFace_mm, -halfMarkerSize_mm,  halfMarkerSize_mm},
    {-halfTargetFace_mm, -halfMarkerSize_mm, -halfMarkerSize_mm}
  });
  
  auto GetCoordsForFace = [&originsLeftFace, &originsFrontFace](bool isFrontFace,
                                                                int numCubesRightOfOrigin,
                                                                int numCubesAwayRobotFromOrigin,
                                                                int numCubesAboveOrigin)
  {
    
    Quad3f whichFace = (isFrontFace ? originsFrontFace : originsLeftFace);
    
    Pose3d p;
    p.SetTranslation({BLEACHER_CALIB_TARGET_FACE_SIZE_MM * numCubesRightOfOrigin,
                      BLEACHER_CALIB_TARGET_FACE_SIZE_MM * numCubesAwayRobotFromOrigin,
                      BLEACHER_CALIB_TARGET_FACE_SIZE_MM * numCubesAboveOrigin});
    
    p.ApplyTo(whichFace, whichFace);
    return whichFace;
  };

  // Bottom row of cubes
  markersTo3dCoords[Vision::MARKER_LIGHTCUBEK_RIGHT]  = GetCoordsForFace(true, 0, 0, 0);
  
  markersTo3dCoords[Vision::MARKER_LIGHTCUBEK_LEFT]   = GetCoordsForFace(false, 1, -1, 0);
  markersTo3dCoords[Vision::MARKER_LIGHTCUBEK_FRONT]  = GetCoordsForFace(true, 1, -1, 0);
  
  markersTo3dCoords[Vision::MARKER_LIGHTCUBEK_TOP]    = GetCoordsForFace(false, 2, -2, 0);
  markersTo3dCoords[Vision::MARKER_LIGHTCUBEK_BACK]   = GetCoordsForFace(true, 2, -2, 0);
  
  markersTo3dCoords[Vision::MARKER_LIGHTCUBEJ_TOP]    = GetCoordsForFace(false, 3, -3, 0);
  markersTo3dCoords[Vision::MARKER_LIGHTCUBEJ_RIGHT]  = GetCoordsForFace(true, 3, -3, 0);
  
  markersTo3dCoords[Vision::MARKER_LIGHTCUBEJ_LEFT]   = GetCoordsForFace(false, 4, -4, 0);
  
  // Second row of cubes
  markersTo3dCoords[Vision::MARKER_ARROW]             = GetCoordsForFace(true, 0, 1, 1);
  markersNeededToBeSeen.insert(Vision::MARKER_ARROW);
  
  markersTo3dCoords[Vision::MARKER_SDK_2HEXAGONS]     = GetCoordsForFace(true, 1, 0, 1);
  markersNeededToBeSeen.insert(Vision::MARKER_SDK_2HEXAGONS);
  
  markersTo3dCoords[Vision::MARKER_SDK_5DIAMONDS]     = GetCoordsForFace(false, 2, -1, 1);
  markersTo3dCoords[Vision::MARKER_SDK_4DIAMONDS]     = GetCoordsForFace(true, 2, -1, 1);
  markersNeededToBeSeen.insert(Vision::MARKER_SDK_4DIAMONDS);
  
  markersTo3dCoords[Vision::MARKER_SDK_3DIAMONDS]     = GetCoordsForFace(false, 3, -2, 1);
  markersNeededToBeSeen.insert(Vision::MARKER_SDK_3DIAMONDS);
  markersTo3dCoords[Vision::MARKER_SDK_2DIAMONDS]     = GetCoordsForFace(true, 3, -2, 1);
  
  markersTo3dCoords[Vision::MARKER_SDK_5CIRCLES]      = GetCoordsForFace(false, 4, -3, 1);
  markersNeededToBeSeen.insert(Vision::MARKER_SDK_5CIRCLES);
  
  markersTo3dCoords[Vision::MARKER_SDK_3CIRCLES]      = GetCoordsForFace(false, 5, -4, 1);
  markersNeededToBeSeen.insert(Vision::MARKER_SDK_3CIRCLES);
  
  // Third row of cubes
  markersTo3dCoords[Vision::MARKER_SDK_4HEXAGONS]     = GetCoordsForFace(true, 0, 2, 2);
  markersNeededToBeSeen.insert(Vision::MARKER_SDK_4HEXAGONS);
  
  markersTo3dCoords[Vision::MARKER_SDK_2CIRCLES]      = GetCoordsForFace(true, 1, 1, 2);
  markersNeededToBeSeen.insert(Vision::MARKER_SDK_2CIRCLES);
  
  markersTo3dCoords[Vision::MARKER_LIGHTCUBEJ_FRONT]  = GetCoordsForFace(false, 2, 0, 2);
  markersTo3dCoords[Vision::MARKER_LIGHTCUBEK_TOP]    = GetCoordsForFace(true, 2, 0, 2);
  markersNeededToBeSeen.insert(Vision::MARKER_LIGHTCUBEK_TOP);
  
  markersTo3dCoords[Vision::MARKER_STAR5]             = GetCoordsForFace(false, 3, -1, 2);
  markersNeededToBeSeen.insert(Vision::MARKER_STAR5);
  markersTo3dCoords[Vision::MARKER_BULLSEYE2]         = GetCoordsForFace(true, 3, -1, 2);
  
  markersTo3dCoords[Vision::MARKER_SDK_5TRIANGLES]    = GetCoordsForFace(false, 4, -2, 2);
  markersNeededToBeSeen.insert(Vision::MARKER_SDK_5TRIANGLES);
  markersTo3dCoords[Vision::MARKER_SDK_4TRIANGLES]    = GetCoordsForFace(true, 4, -2, 2);
  
  markersTo3dCoords[Vision::MARKER_SDK_3TRIANGLES]    = GetCoordsForFace(false, 5, -3, 2);
  markersNeededToBeSeen.insert(Vision::MARKER_SDK_3TRIANGLES);
  
  markersTo3dCoords[Vision::MARKER_SDK_5HEXAGONS]     = GetCoordsForFace(false, 6, -4, 2);
  markersNeededToBeSeen.insert(Vision::MARKER_SDK_5HEXAGONS);
  
  // Fourth row of cubes (top row)
  markersTo3dCoords[Vision::MARKER_SDK_4CIRCLES]      = GetCoordsForFace(true, 0, 3, 3);
  
  markersTo3dCoords[Vision::MARKER_LIGHTCUBEJ_BACK]   = GetCoordsForFace(true, 1, 2, 3);
  
  markersTo3dCoords[Vision::MARKER_LIGHTCUBEI_RIGHT]  = GetCoordsForFace(true, 2, 1, 3);
  
  markersTo3dCoords[Vision::MARKER_LIGHTCUBEI_LEFT]   = GetCoordsForFace(false, 3, 0, 3);
  markersTo3dCoords[Vision::MARKER_LIGHTCUBEI_FRONT]  = GetCoordsForFace(true, 3, 0, 3);
  
  markersTo3dCoords[Vision::MARKER_LIGHTCUBEI_BOTTOM] = GetCoordsForFace(false, 4, -1, 3);
  markersTo3dCoords[Vision::MARKER_LIGHTCUBEI_BACK]   = GetCoordsForFace(true, 4, -1, 3);
  
  markersTo3dCoords[Vision::MARKER_LIGHTCUBEI_TOP]    = GetCoordsForFace(false, 5, -2, 3);
  
  markersTo3dCoords[Vision::MARKER_LIGHTCUBEJ_BOTTOM] = GetCoordsForFace(false, 6, -3, 3);
  
  markersTo3dCoords[Vision::MARKER_SDK_2TRIANGLES]    = GetCoordsForFace(false, 7, -4, 3);
#else
  PRINT_NAMED_ERROR("CameraCalibrator.GetCalibTargetMarkersTo3dCoords_Qbert.NotInFactoryTest",
                    "Markers have diverged from factory test build");
#endif
}

// TODO: Populate markersNeededToBeSeen should we end up using this target again
void CameraCalibrator::GetCalibTargetMarkersTo3dCoords_InvertedBox(std::map<Vision::MarkerType, Quad3f>& markersTo3dCoords,
                                                                   std::set<Vision::MarkerType>& markersNeededToBeSeen)
{
  markersTo3dCoords.clear();
  
  /*
  Top down view bottom row
   _  _  _  _
  |  |* |  |  |
   -  -  -  - _
             | |
              -
             | |
              -
             | |
              -
             | |
              -
   
   ^
   Robot
   
   ^ +y
   |
   -> +x
   +z out
   
   Marker corners are defined relative to center of the * cube of the target in this orientation
   (before rotations are applied to get cubes to their actual positions)
   isFrontFace are the markers that is facing the robot in this orientation.
   !isFrontFace are the left markers, that will be visible when rotation are applied
   isBottomFace are the markers on the top face
   */
#if FACTORY_TEST
  const f32 halfMarkerSize_mm = INVERTEDBOX_CALIB_MARKER_SIZE_MM / 2.f;
  const f32 halfTargetFace_mm = INVERTEDBOX_CALIB_TARGET_FACE_SIZE_MM / 2.f;
  const Quad3f originsFrontFace({
    {-halfMarkerSize_mm, -halfTargetFace_mm,  halfMarkerSize_mm},
    {-halfMarkerSize_mm, -halfTargetFace_mm, -halfMarkerSize_mm},
    { halfMarkerSize_mm, -halfTargetFace_mm,  halfMarkerSize_mm},
    { halfMarkerSize_mm, -halfTargetFace_mm, -halfMarkerSize_mm}
  });
  
  const Quad3f originsLeftFace({
    {-halfTargetFace_mm,  halfMarkerSize_mm,  halfMarkerSize_mm},
    {-halfTargetFace_mm,  halfMarkerSize_mm, -halfMarkerSize_mm},
    {-halfTargetFace_mm, -halfMarkerSize_mm,  halfMarkerSize_mm},
    {-halfTargetFace_mm, -halfMarkerSize_mm, -halfMarkerSize_mm}
  });
  
  const Quad3f originsBottomFace({
    {-halfMarkerSize_mm, -halfMarkerSize_mm, -halfTargetFace_mm},
    {-halfMarkerSize_mm,  halfMarkerSize_mm, -halfTargetFace_mm},
    { halfMarkerSize_mm, -halfMarkerSize_mm, -halfTargetFace_mm},
    { halfMarkerSize_mm,  halfMarkerSize_mm, -halfTargetFace_mm},
  });
  
  auto GetCoordsForFace = [&originsLeftFace, &originsFrontFace, &originsBottomFace]
    (bool isFrontFace,
     int numCubesRightOfOrigin,
     int numCubesAwayRobotFromOrigin,
     int numCubesAboveOrigin,
     bool isBottomFace = false)
  {
    
    Quad3f whichFace = (isFrontFace ? originsFrontFace : originsLeftFace);
    whichFace = (isBottomFace ? originsBottomFace : whichFace);
    
    Pose3d p;
    p.SetTranslation({INVERTEDBOX_CALIB_TARGET_FACE_SIZE_MM * numCubesRightOfOrigin,
                      INVERTEDBOX_CALIB_TARGET_FACE_SIZE_MM * numCubesAwayRobotFromOrigin,
                      INVERTEDBOX_CALIB_TARGET_FACE_SIZE_MM * numCubesAboveOrigin});
    
    p.ApplyTo(whichFace, whichFace);
    return whichFace;
  };

  // Left face
  // Bottom row
  markersTo3dCoords[Vision::MARKER_LIGHTCUBEK_LEFT]   = GetCoordsForFace(true, 0, 0, 0);
  markersTo3dCoords[Vision::MARKER_LIGHTCUBEK_RIGHT]  = GetCoordsForFace(true, 1, 0, 0);
  markersTo3dCoords[Vision::MARKER_LIGHTCUBEK_TOP]    = GetCoordsForFace(true, 2, 0, 0);
  
  // Middle row
  markersTo3dCoords[Vision::MARKER_SDK_3CIRCLES]      = GetCoordsForFace(true, -1, 0, 1);
  markersTo3dCoords[Vision::MARKER_LIGHTCUBEJ_TOP]    = GetCoordsForFace(true, 0, 0, 1);
  markersTo3dCoords[Vision::MARKER_LIGHTCUBEK_BACK]   = GetCoordsForFace(true, 1, 0, 1);
  markersTo3dCoords[Vision::MARKER_LIGHTCUBEK_BOTTOM] = GetCoordsForFace(true, 2, 0, 1);
  
  // Top row
  markersTo3dCoords[Vision::MARKER_SDK_2CIRCLES]      = GetCoordsForFace(true, -1, 0, 2);
  markersTo3dCoords[Vision::MARKER_SDK_2DIAMONDS]     = GetCoordsForFace(true, 0, 0, 2);
  markersTo3dCoords[Vision::MARKER_SDK_2HEXAGONS]     = GetCoordsForFace(true, 1, 0, 2);
  markersTo3dCoords[Vision::MARKER_SDK_2TRIANGLES]    = GetCoordsForFace(true, 2, 0, 2);
  
  // Right face
  // Bottom row
  markersTo3dCoords[Vision::MARKER_LIGHTCUBEJ_BOTTOM] = GetCoordsForFace(false, 3, -1, 0);
  markersTo3dCoords[Vision::MARKER_LIGHTCUBEJ_FRONT]  = GetCoordsForFace(false, 3, -2, 0);
  markersTo3dCoords[Vision::MARKER_LIGHTCUBEJ_LEFT]   = GetCoordsForFace(false, 3, -3, 0);
  
  // Middle row
  markersTo3dCoords[Vision::MARKER_LIGHTCUBEI_LEFT]   = GetCoordsForFace(false, 3, -1, 1);
  markersTo3dCoords[Vision::MARKER_LIGHTCUBEI_RIGHT]  = GetCoordsForFace(false, 3, -2, 1);
  markersTo3dCoords[Vision::MARKER_LIGHTCUBEI_TOP]    = GetCoordsForFace(false, 3, -3, 1);
  markersTo3dCoords[Vision::MARKER_LIGHTCUBEJ_BACK]   = GetCoordsForFace(false, 3, -4, 1);
  
  // Top row
  markersTo3dCoords[Vision::MARKER_ARROW]             = GetCoordsForFace(false, 3, -1, 2);
  markersTo3dCoords[Vision::MARKER_LIGHTCUBEI_BACK]   = GetCoordsForFace(false, 3, -2, 2);
  markersTo3dCoords[Vision::MARKER_LIGHTCUBEI_BOTTOM] = GetCoordsForFace(false, 3, -3, 2);
  markersTo3dCoords[Vision::MARKER_LIGHTCUBEI_FRONT]  = GetCoordsForFace(false, 3, -4, 2);
  
  // Top face
  markersTo3dCoords[Vision::MARKER_BULLSEYE2]         = GetCoordsForFace(false, -1, -1, 3, true);
  markersTo3dCoords[Vision::MARKER_SDK_5TRIANGLES]    = GetCoordsForFace(false, 0, -1, 3, true);
  markersTo3dCoords[Vision::MARKER_SDK_4TRIANGLES]    = GetCoordsForFace(false, 1, -1, 3, true);
  markersTo3dCoords[Vision::MARKER_SDK_5HEXAGONS]     =  GetCoordsForFace(false, 2, -1, 3, true);
  
  markersTo3dCoords[Vision::MARKER_SDK_4DIAMONDS]     = GetCoordsForFace(false, 0, -2, 3, true);
  markersTo3dCoords[Vision::MARKER_SDK_4CIRCLES]      = GetCoordsForFace(false, 1, -2, 3, true);
  markersTo3dCoords[Vision::MARKER_SDK_4HEXAGONS]     = GetCoordsForFace(false, 2, -2, 3, true);
  
  markersTo3dCoords[Vision::MARKER_SDK_3HEXAGONS]     = GetCoordsForFace(false, 1, -3, 3, true);
  markersTo3dCoords[Vision::MARKER_SDK_3TRIANGLES]    = GetCoordsForFace(false, 2, -3, 3, true);
  
  markersTo3dCoords[Vision::MARKER_SDK_3DIAMONDS]     = GetCoordsForFace(false, 2, -4, 3, true);
#else
  PRINT_NAMED_ERROR("CameraCalibrator.GetCalibTargetMarkersTo3dCoords_InvertedBox.NotInFactoryTest",
                    "Markers have diverged from factory test build");
#endif
}

}
}
