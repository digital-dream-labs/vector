/**
 * File: visionSystem.cpp [Basestation]
 *
 * Author: Andrew Stein
 * Date:   (various)
 *
 * Description: High-level module that controls the basestation vision system
 *              Runs on its own thread inside VisionComponent.
 *
 * Copyright: Anki, Inc. 2014
 **/

#include "visionSystem.h"

#include "coretech/common/engine/jsonTools.h"
#include "coretech/common/engine/math/linearAlgebra.h"
#include "coretech/common/engine/math/linearClassifier.h"
#include "coretech/common/engine/math/quad.h"
#include "coretech/common/shared/math/rect.h"
#include "coretech/common/engine/utils/data/dataPlatform.h"
#include "coretech/vision/engine/imageCompositor.h"

#include "engine/cozmoContext.h"
#include "engine/robot.h"
#include "engine/vision/cropScheduler.h"
#include "engine/vision/groundPlaneClassifier.h"
#include "engine/vision/illuminationDetector.h"
#include "engine/vision/imageSaver.h"
#include "engine/vision/laserPointDetector.h"
#include "engine/vision/mirrorModeManager.h"
#include "engine/vision/motionDetector.h"
#include "engine/vision/overheadEdgesDetector.h"
#include "engine/vision/overheadMap.h"
#include "engine/vision/visionModesHelpers.h"
#include "engine/utils/cozmoFeatureGate.h"

#include "coretech/neuralnets/iNeuralNetMain.h"
#include "coretech/neuralnets/neuralNetJsonKeys.h"
#include "coretech/neuralnets/neuralNetRunner.h"
#include "coretech/vision/engine/benchmark.h"
#include "coretech/vision/engine/cameraParamsController.h"
#include "coretech/vision/engine/faceTracker.h"
#include "coretech/vision/engine/image.h"
#include "coretech/vision/engine/imageCache.h"
#include "coretech/vision/engine/markerDetector.h"
#include "coretech/vision/engine/petTracker.h"

#include "clad/vizInterface/messageViz.h"
#include "clad/robotInterface/messageEngineToRobot.h"
#include "clad/types/robotStatusAndActions.h"

#include "util/console/consoleInterface.h"
#include "util/fileUtils/fileUtils.h"
#include "util/helpers/cleanupHelper.h"
#include "util/helpers/templateHelpers.h"
#include "util/helpers/fullEnumToValueArrayChecker.h"
#include "util/random/randomGenerator.h" // DEBUG

#include <thread>
#include <fstream>

#include "opencv2/calib3d/calib3d.hpp"

// Cozmo-Specific Library Includes
#include "anki/cozmo/shared/cozmoConfig.h"

#define DEBUG_MOTION_DETECTION    0
#define DEBUG_FACE_DETECTION      0
#define DEBUG_DISPLAY_CLAHE_IMAGE 0

#define DRAW_TOOL_CODE_DEBUG 0

namespace Anki {
namespace Vector {

namespace {
  const char* kImageCompositorReadyPeriodKey = "imageReadyPeriod";
  const char* kImageCompositorReadyCycleResetKey = "numImageReadyCyclesBeforeReset";
}
  
CONSOLE_VAR_RANGED(u8,  kUseCLAHE_u8,     "Vision.PreProcessing", 0, 0, 4);  // One of MarkerDetectionCLAHE enum
CONSOLE_VAR(s32, kClaheClipLimit,         "Vision.PreProcessing", 32);
CONSOLE_VAR(s32, kClaheTileSize,          "Vision.PreProcessing", 4);
CONSOLE_VAR(u8,  kClaheWhenDarkThreshold, "Vision.PreProcessing", 80); // In MarkerDetectionCLAHE::WhenDark mode, only use CLAHE when img avg < this
CONSOLE_VAR(s32, kPostClaheSmooth,        "Vision.PreProcessing", -3); // 0: off, +ve: Gaussian sigma, -ve (& odd): Box filter size
CONSOLE_VAR(s32, kMarkerDetector_ScaleMultiplier, "Vision.MarkerDetection", 2);
CONSOLE_VAR(f32, kHeadTurnSpeedThreshBlock_degs, "Vision.MarkerDetection",   10.f);
CONSOLE_VAR(f32, kBodyTurnSpeedThreshBlock_degs, "Vision.MarkerDetection",   30.f);

  
// This is fraction of full width we use with the CropScheduler to crop the image for marker detection.
CONSOLE_VAR_RANGED(f32, kMarkerDetector_CropWidthFraction, "Vision.MarkerDetection", 0.65f, 0.5f, 1.f);
  
// Show the crops being used for MarkerDetection. Need to increase Viz debug windows if enabled.
CONSOLE_VAR(bool, kMarkerDetector_VizCropScheduler, "Vision.MarkerDetection", false);
  
// How long to disable auto exposure after using detections to meter
CONSOLE_VAR(u32, kMeteringHoldTime_ms,    "Vision.PreProcessing", 2000);
  
// Loose constraints on how fast Cozmo can move and still trust tracker (which has no
// knowledge of or access to camera movement). Rough means of deciding these angles:
// look at angle created by distance between two faces seen close together at the max
// distance we care about seeing them from. If robot turns by that angle between two
// consecutve frames, it is possible the tracker will be confused and jump from one
// to the other.
CONSOLE_VAR(f32,  kFaceTrackingMaxHeadAngleChange_deg, "Vision.FaceDetection", 8.f);
CONSOLE_VAR(f32,  kFaceTrackingMaxBodyAngleChange_deg, "Vision.FaceDetection", 8.f);
CONSOLE_VAR(f32,  kFaceTrackingMaxPoseChange_mm,       "Vision.FaceDetection", 10.f);

// Sample rate for estimating the mean of an image (increment in both X and Y)
CONSOLE_VAR_RANGED(s32, kImageMeanSampleInc, "VisionSystem.Statistics", 10, 1, 32);

// For testing artificial slowdowns of the vision thread
CONSOLE_VAR(u32, kVisionSystemSimulatedDelay_ms, "Vision.General", 0);

CONSOLE_VAR(u32, kCalibTargetType, "Vision.Calibration", (u32)CameraCalibrator::CalibTargetType::CHECKERBOARD);

// The percentage of the width of the image that will remain after cropping
CONSOLE_VAR_RANGED(f32, kFaceTrackingCropWidthFraction, "Vision.FaceDetection", 2.f / 3.f, 0.f, 1.f);

// Fake hand and pet detections for testing behaviors while we don't have reliable neural net models
CONSOLE_VAR_RANGED(f32, kFakeHandDetectionProbability, "Vision.NeuralNets", 0.f, 0.f, 1.f);
CONSOLE_VAR_RANGED(f32, kFakeCatDetectionProbability,  "Vision.NeuralNets", 0.f, 0.f, 1.f);
CONSOLE_VAR_RANGED(f32, kFakeDogDetectionProbability,  "Vision.NeuralNets", 0.f, 0.f, 1.f);

CONSOLE_VAR(bool, kDisplayUndistortedImages,"Vision.General", false);
  
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
namespace {
  // These are initialized from Json config:
  u8 kTooDarkValue   = 15;
  u8 kTooBrightValue = 230;
  f32 kLowPercentile = 0.10f;
  f32 kTargetPercentile = 0.50f;
  f32 kHighPercentile = 0.90f;
  bool kMeterFromDetections = true;
}

static const char * const kLogChannelName = "VisionSystem";

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
VisionSystem::VisionSystem(const CozmoContext* context)
: _rollingShutterCorrector()
, _lastRollingShutterCorrectionTime(0)
, _imageCache(new Vision::ImageCache())
, _context(context)
, _currentCameraParams{31, 1.0, 2.0, 1.0, 2.0}
, _nextCameraParams{false, _currentCameraParams}
, _cameraParamsController(new Vision::CameraParamsController(MIN_CAMERA_EXPOSURE_TIME_MS,
                                                             MAX_CAMERA_EXPOSURE_TIME_MS,
                                                             MIN_CAMERA_GAIN,
                                                             MAX_CAMERA_GAIN,
                                                             _currentCameraParams))
, _poseOrigin("VisionSystemOrigin")
, _vizManager(context == nullptr ? nullptr : context->GetVizManager())
, _petTracker(new Vision::PetTracker())
, _markerDetector(new Vision::MarkerDetector(_camera))
, _laserPointDetector(new LaserPointDetector(_vizManager))
, _overheadEdgeDetector(new OverheadEdgesDetector(_camera, _vizManager, *this))
, _cameraCalibrator(new CameraCalibrator())
, _illuminationDetector(new IlluminationDetector())
, _imageSaver(new ImageSaver())
, _mirrorModeManager(new MirrorModeManager())
, _benchmark(new Vision::Benchmark())
, _clahe(cv::createCLAHE())
{
  DEV_ASSERT(_context != nullptr, "VisionSystem.Constructor.NullContext");
} // VisionSystem()

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
Result VisionSystem::Init(const Json::Value& config)
{
  _isInitialized = false;
  
  std::string dataPath("");
  std::string cachePath("");
  if(_context->GetDataPlatform() != nullptr) {
    dataPath = _context->GetDataPlatform()->pathToResource(Util::Data::Scope::Resources,
                                                           Util::FileUtils::FullFilePath({"config", "engine", "vision"}));
    cachePath = _context->GetDataPlatform()->pathToResource(Util::Data::Scope::Cache, "vision");
  } else {
    PRINT_NAMED_WARNING("VisionSystem.Init.NullDataPlatform",
                        "Initializing VisionSystem with no data platform.");
  }
  if(!config.isMember("ImageQuality"))
  {
    PRINT_NAMED_ERROR("VisionSystem.Init.MissingImageQualityConfigField", "");
    return RESULT_FAIL;
  }
    
  
  // Helper macro to try to get the specified field and store it in the given variable
  // and return RESULT_FAIL if that doesn't work
#   define GET_JSON_PARAMETER(__json__, __fieldName__, __variable__) \
  do { \
  if(!JsonTools::GetValueOptional(__json__, __fieldName__, __variable__)) { \
    PRINT_NAMED_ERROR("VisionSystem.Init.MissingJsonParameter", "%s", __fieldName__); \
    return RESULT_FAIL; \
  }} while(0)

  {
    // Set up auto-exposure
    const Json::Value& imageQualityConfig = config["ImageQuality"];
    GET_JSON_PARAMETER(imageQualityConfig, "TooBrightValue",      kTooBrightValue);
    GET_JSON_PARAMETER(imageQualityConfig, "TooDarkValue",        kTooDarkValue);
    GET_JSON_PARAMETER(imageQualityConfig, "MeterFromDetections", kMeterFromDetections);
    GET_JSON_PARAMETER(imageQualityConfig, "LowPercentile",       kLowPercentile);
    GET_JSON_PARAMETER(imageQualityConfig, "HighPercentile",      kHighPercentile);
    
    u8  targetValue=0;
    f32 maxChangeFraction = -1.f;
    s32 subSample = 0;
    
    GET_JSON_PARAMETER(imageQualityConfig, "TargetPercentile",    kTargetPercentile);
    GET_JSON_PARAMETER(imageQualityConfig, "TargetValue",         targetValue);
    GET_JSON_PARAMETER(imageQualityConfig, "MaxChangeFraction",   maxChangeFraction);
    GET_JSON_PARAMETER(imageQualityConfig, "SubSample",           subSample);
    
    std::vector<u8> cyclingTargetValues;
    if(!JsonTools::GetVectorOptional(imageQualityConfig, "CyclingTargetValues", cyclingTargetValues))
    {
      PRINT_NAMED_ERROR("VisionSystem.Init.MissingJsonParameter", "%s", "CyclingTargetValues");
      return RESULT_FAIL;
    }
    
    Result result = _cameraParamsController->SetExposureParameters(targetValue,
                                                                   cyclingTargetValues,
                                                                   kTargetPercentile,
                                                                   maxChangeFraction,
                                                                   subSample);
    if(RESULT_OK == result)
    {
      PRINT_CH_INFO(kLogChannelName, "VisionSystem.Init.SetAutoExposureParams",
                    "subSample:%d tarVal:%d tarPerc:%.3f changeFrac:%.3f",
                    subSample, targetValue, kTargetPercentile, maxChangeFraction);
    }
    else
    {
      PRINT_NAMED_ERROR("VisionSystem.Init.SetExposureParametersFailed", "");
      return result;
    }
    
    result = _cameraParamsController->SetImageQualityParameters(kTooDarkValue,   kHighPercentile,
                                                                kTooBrightValue, kLowPercentile);
    if(RESULT_OK != result)
    {
      PRINT_NAMED_ERROR("VisionSystem.Init.SetImageQualityParametersFailed", "");
      return result;
    }
  }
  
  {
    // Set up profiler logging frequencies
    f32 timeBetweenProfilerInfoPrints_sec = 5.f;
    f32 timeBetweenProfilerDasLogs_sec = 60.f;
    
    const Json::Value& performanceConfig = config["PerformanceLogging"];
    GET_JSON_PARAMETER(performanceConfig, "TimeBetweenProfilerInfoPrints_sec", timeBetweenProfilerInfoPrints_sec);
    GET_JSON_PARAMETER(performanceConfig, "TimeBetweenProfilerDasLogs_sec",    timeBetweenProfilerDasLogs_sec);
    
    Profiler::SetProfileGroupName("VisionSystem.Profiler");
    Profiler::SetPrintChannelName(kLogChannelName);
    Profiler::SetPrintFrequency(Util::SecToMilliSec(timeBetweenProfilerInfoPrints_sec));
    Profiler::SetDasLogFrequency(Util::SecToMilliSec(timeBetweenProfilerDasLogs_sec));
  }
  
  PRINT_CH_INFO(kLogChannelName, "VisionSystem.Init.InstantiatingFaceTracker",
                "With model path %s.", dataPath.c_str());
  _faceTracker.reset(new Vision::FaceTracker(_camera, dataPath, config));
  PRINT_CH_INFO(kLogChannelName, "VisionSystem.Init.DoneInstantiatingFaceTracker", "");

  _motionDetector.reset(new MotionDetector(_camera, _vizManager, config));

  if (!config.isMember("OverheadMap")) {
    PRINT_NAMED_ERROR("VisionSystem.Init.MissingJsonParameter", "OverheadMap");
    return RESULT_FAIL;
  }
  _overheadMap.reset(new OverheadMap(config["OverheadMap"], _context));

  const Json::Value& imageCompositeCfg = config["ImageCompositing"];
  {
    _imageCompositorReadyPeriod = JsonTools::ParseUInt32(imageCompositeCfg, 
                                    kImageCompositorReadyPeriodKey, 
                                    "VisionSystem.Ctor");

    // The Reset Period is an integer multiple of the Ready Period
    _imageCompositorResetPeriod = _imageCompositorReadyPeriod * 
                                    JsonTools::ParseUInt32(imageCompositeCfg, 
                                    kImageCompositorReadyCycleResetKey, 
                                    "VisionSystem.Ctor");
  }
  _imageCompositor.reset(new Vision::ImageCompositor(imageCompositeCfg));

  // TODO check config entry here
  _groundPlaneClassifier.reset(new GroundPlaneClassifier(config["GroundPlaneClassifier"], _context));

  const Result petTrackerInitResult = _petTracker->Init(config);
  if(RESULT_OK != petTrackerInitResult) {
    PRINT_NAMED_ERROR("VisionSystem.Init.PetTrackerInitFailed", "");
    return petTrackerInitResult;
  }
  
  if(!config.isMember(NeuralNets::JsonKeys::NeuralNets))
  {
    PRINT_NAMED_ERROR("VisionSystem.Init.MissingNeuralNetsConfigField", "");
    return RESULT_FAIL;
  }
  
  const std::string modelPath = Util::FileUtils::FullFilePath({dataPath, "dnn_models"});
  if(Util::FileUtils::DirectoryExists(modelPath)) // TODO: Remove once DNN models are checked in somewhere (VIC-1071)
  {
    const Json::Value& neuralNetConfig = config[NeuralNets::JsonKeys::NeuralNets];
    
    if(!neuralNetConfig.isMember(NeuralNets::JsonKeys::Models))
    {
      PRINT_NAMED_ERROR("VisionSystem.Init.MissingNeuralNetsModelsConfigField", "");
      return RESULT_FAIL;
    }
    
    const Json::Value& modelsConfig = neuralNetConfig[NeuralNets::JsonKeys::Models];
    
    if(!modelsConfig.isArray())
    {
      PRINT_NAMED_ERROR("VisionSystem.Init.NeuralNetsModelsConfigNotArray", "");
      return RESULT_FAIL;
    }
    
#   ifdef VICOS
    // Use faster tmpfs partition for the cache, to make I/O less of a bottleneck
    const std::string dnnCachePath = "/tmp/vision/neural_nets";
#   else
    const std::string dnnCachePath = Util::FileUtils::FullFilePath({cachePath, "neural_nets"});
#   endif
    
    for(const auto& modelConfig : modelsConfig)
    {
      if(!modelConfig.isMember(NeuralNets::JsonKeys::NetworkName))
      {
        PRINT_NAMED_ERROR("VisionSystem.Init.MissingNeuralNetModelName", "");
        continue;
      }
      
      const std::string& name = modelConfig[NeuralNets::JsonKeys::NetworkName].asString();
      auto addModelResult = _neuralNetRunners.emplace(name, new NeuralNets::NeuralNetRunner(modelPath));
      if(!addModelResult.second)
      {
        PRINT_NAMED_ERROR("VisionSystem.Init.DuplicateNeuralNetModelName", "%s", name.c_str());
        continue;
      }
      std::unique_ptr<NeuralNets::NeuralNetRunner>& neuralNetRunner = addModelResult.first->second;
      Result neuralNetResult = neuralNetRunner->Init(dnnCachePath, modelConfig);
      if(RESULT_OK != neuralNetResult)
      {
        PRINT_NAMED_ERROR("VisionSystem.Init.NeuralNetInitFailed", "Name: %s", name.c_str());
        continue;
      }
    }
  }
   
  if(!config.isMember("IlluminationDetector"))
  {
    PRINT_NAMED_ERROR("VisionSystem.Init.MissingIlluminationDetectorConfigField", "");
    return RESULT_FAIL;
  }
  Result illuminationResult = _illuminationDetector->Init(config["IlluminationDetector"], _context);
  if( illuminationResult != RESULT_OK )
  {
    PRINT_NAMED_ERROR("VisionSystem.Init.IlluminationDetectorInitFailed", "");
    return RESULT_FAIL;
  }

  _modes.Clear();
  
  _clahe->setClipLimit(kClaheClipLimit);
  _clahe->setTilesGridSize(cv::Size(kClaheTileSize, kClaheTileSize));
  _lastClaheTileSize = kClaheTileSize;
  _lastClaheClipLimit = kClaheClipLimit;
  
  _isInitialized = true;
  return RESULT_OK;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
Result VisionSystem::UpdateCameraCalibration(std::shared_ptr<Vision::CameraCalibration> camCalib)
{
  Result result = RESULT_OK;
  const bool updatedCalibration = _camera.SetCalibration(camCalib);
  if(!updatedCalibration)
  {
    // Camera already calibrated with same settings, no need to do anything
    return result;
  }
  
  // Re-initialize the marker detector for the new image size
  _markerDetector->Init(camCalib->GetNrows(), camCalib->GetNcols());

  // Provide the ImageSaver with the camera calibration so that it can remove distortion if needed
  // Also pre-cache distortion maps for Sensor resolution, since we use that for photos
  _imageSaver->SetCalibration(camCalib);
  _imageSaver->CacheUndistortionMaps(CAMERA_SENSOR_RESOLUTION_HEIGHT, CAMERA_SENSOR_RESOLUTION_WIDTH);
  
  return result;
} // Init()

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
VisionSystem::~VisionSystem()
{
  
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
Result VisionSystem::SetNextCameraParams(const Vision::CameraParams& params)
{
  if(!_cameraParamsController->AreCameraParamsValid(params))
  {
    PRINT_PERIODIC_CH_INFO(100, kLogChannelName, "VisionSystem.SetNextCameraParams.InvalidParams",
                           "ExpTime:%dms, ExpGain=%f, WBGains RGB=(%f,%f,%f)",
                           params.exposureTime_ms, params.gain,
                           params.whiteBalanceGainR, params.whiteBalanceGainG, params.whiteBalanceGainB);
    return RESULT_FAIL;
  }
  
  bool& nextParamsSet = _nextCameraParams.first;
  if(nextParamsSet)
  {
    PRINT_NAMED_WARNING("VisionSystem.SetNextCameraParams.OverwritingPreviousParams",
                        "Params already requested AE:(%dms,%.2f) WB:(%.2f,%.2f) but not sent. "
                        "Replacing with AE:(%dms,%.2f) WB:(%.2f,%.2f)",
                        _nextCameraParams.second.exposureTime_ms, _nextCameraParams.second.gain,
                        _nextCameraParams.second.whiteBalanceGainR, _nextCameraParams.second.whiteBalanceGainB,
                        params.exposureTime_ms, params.gain,
                        params.whiteBalanceGainR, params.whiteBalanceGainB);
  }
  
  _nextCameraParams.second = params;
  nextParamsSet = true;
  
  return RESULT_OK;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void VisionSystem::SetSaveParameters(const ImageSaverParams& params)
{
  const Result result = _imageSaver->SetParams(params);
  
  if(RESULT_OK != result)
  {
    PRINT_NAMED_ERROR("VisionSystem.SetSaveParameters.BadParams", "");
  }
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
Result VisionSystem::UpdatePoseData(const VisionPoseData& poseData)
{
  std::swap(_prevPoseData, _poseData);
  _poseData = poseData;
  
  // Update cameraPose and historical state's pose to use the vision system's pose origin
  {
    // We expect the passed-in historical pose to be w.r.t. to an origin and have its parent removed
    // so that we can set its parent to be our poseOrigin on this thread. The cameraPose
    // should use the histState's pose as its parent.
    DEV_ASSERT(!poseData.histState.GetPose().HasParent(), "VisionSystem.UpdatePoseData.HistStatePoseHasParent");
    DEV_ASSERT(poseData.cameraPose.IsChildOf(poseData.histState.GetPose()),
               "VisionSystem.UpdatePoseData.BadPoseDataCameraPose");
    _poseData.histState.SetPoseParent(_poseOrigin);
  }
  
  if(_wasCalledOnce) {
    _havePrevPoseData = true;
  } else {
    _wasCalledOnce = true;
  }
  
  return RESULT_OK;
} // UpdateRobotState()

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
Radians VisionSystem::GetCurrentHeadAngle()
{
  return _poseData.histState.GetHeadAngle_rad();
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
Radians VisionSystem::GetPreviousHeadAngle()
{
  return _prevPoseData.histState.GetHeadAngle_rad();
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
bool VisionSystem::CheckMailbox(VisionProcessingResult& result)
{
  std::lock_guard<std::mutex> lock(_mutex);
  if(_results.empty()) {
    return false;
  } else {
    std::swap(result, _results.front());
    _results.pop();
    return true;
  }
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
bool VisionSystem::IsInitialized() const
{
  return _isInitialized;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
u8 VisionSystem::ComputeMean(Vision::ImageCache& imageCache, const s32 sampleInc)
{
  DEV_ASSERT(sampleInc >= 1, "VisionSystem.ComputeMean.BadIncrement");
  
  const Vision::Image& inputImageGray = imageCache.GetGray();
  s32 sum=0;
  const s32 numRows = inputImageGray.GetNumRows();
  const s32 numCols = inputImageGray.GetNumCols();
  for(s32 i=0; i<numRows; i+=sampleInc)
  {
    const u8* image_i = inputImageGray.GetRow(i);
    for(s32 j=0; j<numCols; j+=sampleInc)
    {
      sum += image_i[j];
    }
  }
  // Consider that in the loop above, we always start at row 0, and we always start at column 0
  const s32 count = ((numRows + sampleInc - 1) / sampleInc) *
                    ((numCols + sampleInc - 1) / sampleInc);

  const u8 mean = Util::numeric_cast_clamped<u8>(sum/count);
  return mean;
}
  
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void VisionSystem::UpdateMeteringRegions(TimeStamp_t currentTime_ms, DetectionRectsByMode&& detectionsByMode)
{
  const bool meterFromChargerOnly = IsModeEnabled(VisionMode::Markers_ChargerOnly);
  
  // Before we do image quality / auto exposure, swap in the detections for any mode that actually ran,
  // in case we need them for metering
  // - if a mode ran but detected nothing, then an empty vector of rects will be swapped in
  // - if a mode did not run, the previous detections for that mode will persist until it runs again
  for(auto & current : detectionsByMode)
  {
    const VisionMode mode = current.first;
    if(meterFromChargerOnly && (mode != VisionMode::Markers))
    {
      continue;
    }
    std::swap(_meteringRegions[mode], current.second);
  }
  
  // Clear out any stale "previous" detections for modes that are completely disabled by the current schedule
  // Also remove empty vectors of rectangles that got swapped in above
  auto iter = _meteringRegions.begin();
  while(iter != _meteringRegions.end())
  {
    const VisionMode mode = iter->first;
    if(iter->second.empty() || !_futureModes.Contains(mode)) {
      iter = _meteringRegions.erase(iter);
    }
    else if(meterFromChargerOnly && (mode != VisionMode::Markers)) {
      iter = _meteringRegions.erase(iter);
    }
    else {
      ++iter;
    }
  }
}
  
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
Result VisionSystem::UpdateCameraParams(Vision::ImageCache& imageCache)
{
# define DEBUG_IMAGE_HISTOGRAM 0
  
  Vision::CameraParamsController::AutoExpMode aeMode = Vision::CameraParamsController::AutoExpMode::Off;
  if(IsModeEnabled(VisionMode::AutoExp))
  {
    if(IsModeEnabled(VisionMode::AutoExp_MinGain))
    {
      aeMode = Vision::CameraParamsController::AutoExpMode::MinGain;
    }
    else {
      aeMode = Vision::CameraParamsController::AutoExpMode::MinTime;
    }
  }
  
  Vision::CameraParamsController::WhiteBalanceMode wbMode = Vision::CameraParamsController::WhiteBalanceMode::Off;
  if(IsModeEnabled(VisionMode::WhiteBalance))
  {
    wbMode = Vision::CameraParamsController::WhiteBalanceMode::GrayWorld;
  }
  
  _cameraParamsController->ClearMeteringRegions();
  
  bool useCycling = false;
  if(!kMeterFromDetections || _meteringRegions.empty())
  {
    if((_lastMeteringTimestamp_ms > 0) && // if we've ever metered
       imageCache.GetTimeStamp() <= (_lastMeteringTimestamp_ms + kMeteringHoldTime_ms))
    {
      // Don't update auto exposure for a little while after we lose metered regions
      PRINT_CH_INFO("VisionSystem", "VisionSystem.UpdateCameraParams.HoldingExposureAfterRecentMeteredRegions", "");
      return RESULT_OK;
    }
    
    useCycling = IsModeEnabled(VisionMode::AutoExp_Cycling);
  }
  else
  {   
    // When we have detections to weight, don't use cycling exposures. Just let the detections drive the exposure.
    useCycling = false;
    
    _lastMeteringTimestamp_ms = imageCache.GetTimeStamp();
    
    for(auto const& entry : _meteringRegions)
    {
      for(auto const& rect : entry.second)
      {
        _cameraParamsController->AddMeteringRegion(rect);
      }
    }
  }
  
  Vision::CameraParams nextParams;
  Result expResult = RESULT_FAIL;
  if(imageCache.HasColor())
  {
    const Vision::ImageRGB& inputImage = imageCache.GetRGB();
    expResult = _cameraParamsController->ComputeNextCameraParams(inputImage, aeMode, wbMode, useCycling, nextParams);
  }
  else
  {
    const Vision::Image& inputImage = imageCache.GetGray();
    expResult = _cameraParamsController->ComputeNextCameraParams(inputImage, aeMode, useCycling, nextParams);
  }
  
  if(RESULT_OK != expResult)
  {
    PRINT_NAMED_WARNING("VisionSystem.UpdateCameraParams.ComputeNewExposureFailed", "");
    return expResult;
  }
  
  if(DEBUG_IMAGE_HISTOGRAM)
  {
    const Vision::ImageBrightnessHistogram& hist = _cameraParamsController->GetHistogram();
    std::vector<u8> values = hist.ComputePercentiles({kLowPercentile, kTargetPercentile, kHighPercentile});
    auto valueIter = values.begin();
    
    Vision::ImageRGB histImg(hist.GetDisplayImage(128));
    histImg.DrawText(Anki::Point2f((s32)hist.GetCounts().size()/3, 12),
                     std::string("L:")  + std::to_string(*valueIter++) +
                     std::string(" M:") + std::to_string(*valueIter++) +
                     std::string(" H:") + std::to_string(*valueIter++),
                     NamedColors::RED, 0.45f);
    _currentResult.debugImages.emplace_back("ImageHist", histImg);
    
  } // if(DEBUG_IMAGE_HISTOGRAM)
  
  // Put the new values in the output result:
  std::swap(_currentResult.cameraParams, nextParams);
  _currentResult.imageQuality = _cameraParamsController->GetImageQuality();
  const bool _isMeteringForDetection = (!_meteringRegions.empty());
  const bool completedExposureCycling = _cameraParamsController->IsExposureCyclingComplete();
  if(IsModeEnabled(VisionMode::AutoExp_Cycling) && (_isMeteringForDetection || completedExposureCycling))
  {
    // We have completed one full pass through the list of exposures to cycle
    //  or we have detected something (which triggers metering that locks the
    //  exposure settings, during this mode).
    _currentResult.modesProcessed.Insert(VisionMode::AutoExp_Cycling);
  }
  else if(!IsModeEnabled(VisionMode::AutoExp_Cycling))
  {
    // Whenever we are not cycling the exposure, make sure the
    //  iterator starts with the first value in the cycle.
    // This is necessary because of the asynchronous nature of
    //  VisionSystem, we cannot predict whether we ticked an 
    //  extra frame of processing after the mode is turned off 
    //  by the VisionSchedule.
    // Because of this potential extra-tick, and because of how
    //  we track the current exposure as state-var, it could
    //  potentially throw off our count of how many full loops
    //  through the exposure values we have gone through.
    _cameraParamsController->ResetTargetAutoExposure_Cycling();
  }
  
  _currentResult.modesProcessed.Enable(VisionMode::AutoExp_MinGain,
                                       (aeMode == Vision::CameraParamsController::AutoExpMode::MinGain));
  
  return RESULT_OK;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
bool VisionSystem::CanAddNamedFace() const
{
  return _faceTracker->CanAddNamedFace();
}
  
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
Result VisionSystem::AssignNameToFace(Vision::FaceID_t faceID, const std::string& name, Vision::FaceID_t mergeWithID)
{
  if(!_isInitialized) {
    PRINT_NAMED_WARNING("VisionSystem.AssignNameToFace.NotInitialized",
                        "Cannot assign name '%s' to face ID %d before being initialized",
                        name.c_str(), faceID);
    return RESULT_FAIL;
  }
  
  DEV_ASSERT(_faceTracker != nullptr, "VisionSystem.AssignNameToFace.NullFaceTracker");
  
  return _faceTracker->AssignNameToID(faceID, name, mergeWithID);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
Result VisionSystem::EraseFace(Vision::FaceID_t faceID)
{
  return _faceTracker->EraseFace(faceID);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void VisionSystem::SetFaceEnrollmentMode(Vision::FaceID_t forFaceID,  s32 numEnrollments, bool forceNewID)
{
  _faceTracker->SetFaceEnrollmentMode(forFaceID, numEnrollments, forceNewID);
}

#if ANKI_DEV_CHEATS
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void VisionSystem::SaveAllRecognitionImages(const std::string& imagePathPrefix)
{
  _faceTracker->SaveAllRecognitionImages(imagePathPrefix);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void VisionSystem::DeleteAllRecognitionImages()
{
  _faceTracker->DeleteAllRecognitionImages();
}
#endif

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void VisionSystem::EraseAllFaces()
{
  _faceTracker->EraseAllFaces();
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
std::vector<Vision::LoadedKnownFace> VisionSystem::GetEnrolledNames() const
{
  return _faceTracker->GetEnrolledNames();
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
Result VisionSystem::RenameFace(Vision::FaceID_t faceID, const std::string& oldName, const std::string& newName,
                                Vision::RobotRenamedEnrolledFace& renamedFace)
{
  return _faceTracker->RenameFace(faceID, oldName, newName, renamedFace);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
Vision::Image BlackOutRects(const Vision::Image& img, const std::vector<Anki::Rectangle<s32>>& rects)
{
  // Black out detected markers so we don't find faces in them
  Vision::Image maskedImage;
  img.CopyTo(maskedImage);
  
  DEV_ASSERT(maskedImage.GetTimestamp() == img.GetTimestamp(), "VisionSystem.DetectFaces.BadImageTimestamp");
  
  for(auto rect : rects) // Deliberate copy because GetROI can modify 'rect'
  {
    Vision::Image roi = maskedImage.GetROI(rect);
    
    if(!roi.IsEmpty())
    {
      roi.FillWith(0);
    }
  }
  
  return maskedImage;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
Result VisionSystem::DetectFaces(Vision::ImageCache& imageCache, std::vector<Anki::Rectangle<s32>>& detectionRects,
                                 const bool useCropping)
{
  DEV_ASSERT(_faceTracker != nullptr, "VisionSystem.DetectFaces.NullFaceTracker");
 
  const Vision::Image& grayImage = imageCache.GetGray();

  /*
  // Periodic printouts of face tracker timings
  static TimeStamp_t lastProfilePrint = 0;
  if(grayImage.GetTimestamp() - lastProfilePrint > 2000) {
    _faceTracker->PrintTiming();
    lastProfilePrint = grayImage.GetTimestamp();
  }
   */
  
  if(_faceTracker == nullptr) {
    PRINT_NAMED_ERROR("VisionSystem.Update.NullFaceTracker",
                      "In detecting faces mode, but face tracker is null.");
    return RESULT_FAIL;
  }
  
  // If we've moved too much, reset the tracker so we don't accidentally mistake
  // one face for another. (If one face it was tracking from the last image is
  // now on top of a nearby face in the image, the tracker can't tell if that's
  // because the face moved or the camera moved.)
  const bool hasHeadMoved = !_poseData.IsHeadAngleSame(_prevPoseData, DEG_TO_RAD(kFaceTrackingMaxHeadAngleChange_deg));
  const bool hasBodyMoved = !_poseData.IsBodyPoseSame(_prevPoseData,
                                                      DEG_TO_RAD(kFaceTrackingMaxBodyAngleChange_deg),
                                                      kFaceTrackingMaxPoseChange_mm);
  if(hasHeadMoved || hasBodyMoved)
  {
    PRINT_CH_DEBUG(kLogChannelName, "VisionSystem.Update.ResetFaceTracker",
                      "HeadMoved:%d BodyMoved:%d", hasHeadMoved, hasBodyMoved);
    _faceTracker->AccountForRobotMove();
  }

  const f32 cropFactor = (useCropping ? kFaceTrackingCropWidthFraction : 1.f);

  if(!detectionRects.empty())
  {
    // Black out previous detections so we don't find faces in them
    Vision::Image maskedImage = BlackOutRects(grayImage, detectionRects);
    
#     if DEBUG_FACE_DETECTION
    //_currentResult.debugImages.push_back({"MaskedFaceImage", maskedImage});
#     endif
    
    _faceTracker->Update(maskedImage, cropFactor, _currentResult.faces, _currentResult.updatedFaceIDs,
                         _currentResult.debugImages);
  }
  else
  {
    // Nothing already detected, so nothing to black out before looking for faces
    _faceTracker->Update(grayImage, cropFactor, _currentResult.faces, _currentResult.updatedFaceIDs, _currentResult.debugImages);
  }
  
  for(auto faceIter = _currentResult.faces.begin(); faceIter != _currentResult.faces.end(); ++faceIter)
  {
    auto & currentFace = *faceIter;
    
    DEV_ASSERT(currentFace.GetTimeStamp() == grayImage.GetTimestamp(), "VisionSystem.DetectFaces.BadFaceTimestamp");

    detectionRects.emplace_back((s32)std::round(faceIter->GetRect().GetX()),
                                (s32)std::round(faceIter->GetRect().GetY()),
                                (s32)std::round(faceIter->GetRect().GetWidth()),
                                (s32)std::round(faceIter->GetRect().GetHeight()));
    
    // Make head pose w.r.t. the historical world origin
    Pose3d headPose = currentFace.GetHeadPose();
    headPose.SetParent(_poseData.cameraPose);
    headPose = headPose.GetWithRespectToRoot();

    // Make eye pose w.r.t. the historical world origin
    Pose3d eyePose = currentFace.GetEyePose();
    eyePose.SetParent(_poseData.cameraPose);
    eyePose = eyePose.GetWithRespectToRoot();

    DEV_ASSERT(headPose.IsChildOf(_poseOrigin), "VisionSystem.DetectFaces.BadHeadPoseParent");
    DEV_ASSERT(eyePose.IsChildOf(_poseOrigin), "VisionSystem.DetectFaces.BadEyePoseParent");
    
    // Leave faces in the output result with no parent pose (b/c we will assume they are w.r.t. the origin)
    headPose.ClearParent();
    eyePose.ClearParent();
    
    currentFace.SetHeadPose(headPose);
    currentFace.SetEyePose(eyePose);
  }
  
  return RESULT_OK;
} // DetectFaces()

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
Result VisionSystem::DetectPets(Vision::ImageCache& imageCache,
                                std::vector<Anki::Rectangle<s32>>& detections)
{
  const Vision::Image& grayImage = imageCache.GetGray();
  Result result = RESULT_FAIL;
  
  if(detections.empty())
  {
    result = _petTracker->Update(grayImage, _currentResult.pets);
  }
  else
  {
    // Don't look for pets where we've already found something else
    Vision::Image maskedImage = BlackOutRects(grayImage, detections);
    result = _petTracker->Update(maskedImage, _currentResult.pets);
  }
  
  if(RESULT_OK != result) {
    PRINT_NAMED_WARNING("VisionSystem.DetectPets.PetTrackerUpdateFailed", "");
  }
  
  for(auto const& pet : _currentResult.pets)
  {
    detections.emplace_back((s32)std::round(pet.GetRect().GetX()),
                            (s32)std::round(pet.GetRect().GetY()),
                            (s32)std::round(pet.GetRect().GetWidth()),
                            (s32)std::round(pet.GetRect().GetHeight()));
  }
  return result;
  
} // DetectPets()
  
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
Result VisionSystem::DetectMotion(Vision::ImageCache& imageCache)
{

  Result result = RESULT_OK;
  
  _motionDetector->Detect(imageCache, _poseData, _prevPoseData,
                          _currentResult.observedMotions, _currentResult.debugImages);
  
  return result;
  
} // DetectMotion()

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
Result VisionSystem::DetectBrightColors(Vision::ImageCache& imageCache)
{
  DEV_ASSERT(imageCache.HasColor(), "VisionSystem.DetectBrightColors.NoColor");
  const Vision::ImageRGB& image = imageCache.GetRGB();
  Result result = _brightColorDetector->Detect(image, _currentResult.salientPoints);
  return result;
} // DetectBrightColors()

Result VisionSystem::UpdateOverheadMap(Vision::ImageCache& imageCache)
{
  DEV_ASSERT(imageCache.HasColor(), "VisionSystem.UpdateOverheadMap.NoColor");
  const Vision::ImageRGB& image = imageCache.GetRGB();
  Result result = _overheadMap->Update(image, _poseData, _currentResult.debugImages);
  return result;
}

Result VisionSystem::UpdateGroundPlaneClassifier(Vision::ImageCache& imageCache)
{
  DEV_ASSERT(imageCache.HasColor(), "VisionSystem.UpdateGroundPlaneClassifier.NoColor");
  const Vision::ImageRGB& image = imageCache.GetRGB();
  Result result = _groundPlaneClassifier->Update(image, _poseData, _currentResult.debugImages,
                                                 _currentResult.visualObstacles);
  return result;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
Result VisionSystem::DetectLaserPoints(Vision::ImageCache& imageCache)
{
  const bool isDarkExposure = (Util::IsNear(_currentCameraParams.exposureTime_ms, GetMinCameraExposureTime_ms()) &&
                               Util::IsNear(_currentCameraParams.gain, GetMinCameraGain()));
  
  Result result = _laserPointDetector->Detect(imageCache, _poseData, isDarkExposure,
                                              _currentResult.laserPoints,
                                              _currentResult.debugImages);
  
  return result;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
Result VisionSystem::DetectIllumination(Vision::ImageCache& imageCache)
{
  Result result = _illuminationDetector->Detect(imageCache, 
                                                _poseData, 
                                                _currentResult.illumination);
  return result;
}

#if 0
#pragma mark --- Public VisionSystem API Implementations ---
#endif

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
std::string VisionSystem::GetCurrentModeName() const {
  return _modes.ToString();
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
VisionMode VisionSystem::GetModeFromString(const std::string& str) const
{
  return VisionModeFromString(str.c_str());
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
Result VisionSystem::ApplyCLAHE(Vision::ImageCache& imageCache,
                                const MarkerDetectionCLAHE useCLAHE,
                                Vision::Image& claheImage)
{
  const Vision::ImageCacheSize whichSize = imageCache.GetSize(kMarkerDetector_ScaleMultiplier);
  
  switch(useCLAHE)
  {
    case MarkerDetectionCLAHE::Off:
      _currentUseCLAHE = false;
      break;
      
    case MarkerDetectionCLAHE::On:
    case MarkerDetectionCLAHE::Both:
      _currentUseCLAHE = true;
      break;
      
    case MarkerDetectionCLAHE::Alternating:
      _currentUseCLAHE = !_currentUseCLAHE;
      break;
      
    case MarkerDetectionCLAHE::WhenDark:
    {
      const Vision::Image& inputImageGray = imageCache.GetGray(whichSize);
        
      // Use CLAHE on the current image if it is dark enough
      static const s32 subSample = 3;
      const s32 numRows = inputImageGray.GetNumRows();
      const s32 numCols = inputImageGray.GetNumCols();
      // Consider that in the loop below, we always start at row 0, and we always start at column 0
      const s32 count = ((numRows + subSample - 1) / subSample) *
                        ((numCols + subSample - 1) / subSample);
      const s32 threshold = kClaheWhenDarkThreshold * count;

      _currentUseCLAHE = true;
      s32 meanValue = 0;
      for(s32 i=0; i<numRows; i+=subSample)
      {
        const u8* img_i = inputImageGray.GetRow(i);
        for(s32 j=0; j<numCols; j+=subSample)
        {
          meanValue += img_i[j];
        }
        if (meanValue >= threshold)
        {
          // Image is not dark enough; early out
          _currentUseCLAHE = false;
          break;
        }
      }
      break;
    }
      
    case MarkerDetectionCLAHE::Count:
      assert(false);
      break;
  }
  
  if(!_currentUseCLAHE)
  {
    // Nothing to do: not currently using CLAHE
    return RESULT_OK;
  }
  
  if(_lastClaheTileSize != kClaheTileSize) {
    PRINT_CH_DEBUG(kLogChannelName, "VisionSystem.Update.ClaheTileSizeUpdated",
                      "%d -> %d", _lastClaheTileSize, kClaheTileSize);
    
    _clahe->setTilesGridSize(cv::Size(kClaheTileSize, kClaheTileSize));
    _lastClaheTileSize = kClaheTileSize;
  }
  
  if(_lastClaheClipLimit != kClaheClipLimit) {
    PRINT_CH_DEBUG(kLogChannelName, "VisionSystem.Update.ClaheClipLimitUpdated",
                      "%d -> %d", _lastClaheClipLimit, kClaheClipLimit);
    
    _clahe->setClipLimit(kClaheClipLimit);
    _lastClaheClipLimit = kClaheClipLimit;
  }

  const Vision::Image& inputImageGray = imageCache.GetGray(whichSize);
    
  Tic("CLAHE");
  _clahe->apply(inputImageGray.get_CvMat_(), claheImage.get_CvMat_());
  
  if(kPostClaheSmooth > 0)
  {
    s32 kSize = 3*kPostClaheSmooth;
    if(kSize % 2 == 0) {
      ++kSize; // Make sure it's odd
    }
    cv::GaussianBlur(claheImage.get_CvMat_(), claheImage.get_CvMat_(),
                     cv::Size(kSize,kSize), kPostClaheSmooth);
  }
  else if(kPostClaheSmooth < 0)
  {
    static Vision::Image temp(claheImage.GetNumRows(), claheImage.GetNumCols());
    claheImage.BoxFilter(temp, -kPostClaheSmooth);
    std::swap(claheImage, temp);
  }
  Toc("CLAHE");
  
  if(DEBUG_DISPLAY_CLAHE_IMAGE) {
    _currentResult.debugImages.emplace_back("ImageCLAHE", claheImage);
  }
  
  claheImage.SetTimestamp(inputImageGray.GetTimestamp()); // make sure to preserve timestamp!
  
  return RESULT_OK;
  
} // ApplyCLAHE()

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
Result VisionSystem::DetectMarkers(Vision::ImageCache& imageCache,
                                  const Vision::Image& claheImage,
                                  std::vector<Anki::Rectangle<s32>>& detectionRects,
                                  MarkerDetectionCLAHE useCLAHE,
                                  const VisionPoseData& poseData)
{
  // Currently assuming we detect markers first, so we won't make use of anything already detected
  DEV_ASSERT(detectionRects.empty(), "VisionSystem.DetectMarkersWithCLAHE.ExpectingEmptyDetectionRects");
  
  const auto whichSize = imageCache.GetSize(kMarkerDetector_ScaleMultiplier);
  
  std::vector<const Vision::Image*> imagePtrs;
  
  switch(useCLAHE)
  {
    case MarkerDetectionCLAHE::Off:
    {
      imagePtrs.push_back(&imageCache.GetGray(whichSize));
      break;
    }
      
    case MarkerDetectionCLAHE::On:
    {
      DEV_ASSERT(!claheImage.IsEmpty(), "VisionSystem.DetectMarkersWithCLAHE.useOn.ImageIsEmpty");
      imagePtrs.push_back(&claheImage);
      break;
    }
      
    case MarkerDetectionCLAHE::Both:
    {
      DEV_ASSERT(!claheImage.IsEmpty(), "VisionSystem.DetectMarkersWithCLAHE.useBoth.ImageIsEmpty");
      
      // First run will put quads into detectionRects
      imagePtrs.push_back(&imageCache.GetGray(whichSize));
      
      // Second run will white out existing markerQuads (so we don't
      // re-detect) and also add new ones
      imagePtrs.push_back(&claheImage);
      break;
    }
      
    case MarkerDetectionCLAHE::Alternating:
    {
      if(_currentUseCLAHE) {
        DEV_ASSERT(!claheImage.IsEmpty(), "VisionSystem.DetectMarkersWithCLAHE.useAlternating.ImageIsEmpty");
        imagePtrs.push_back(&claheImage);
      }
      else {
        imagePtrs.push_back(&imageCache.GetGray(whichSize));
      }
      break;
    }
      
    case MarkerDetectionCLAHE::WhenDark:
    {
      // NOTE: _currentUseCLAHE should have been set based on image brightness already
      
      if(_currentUseCLAHE)
      {
        DEV_ASSERT(!claheImage.IsEmpty(), "VisionSystem.DetectMarkersWithCLAHE.useWhenDark.ImageIsEmpty");
        imagePtrs.push_back(&claheImage);
      }
      else {
        imagePtrs.push_back(&imageCache.GetGray(whichSize));
      }
      break;
    }
      
    case MarkerDetectionCLAHE::Count:
      assert(false); // should never get here
      break;
  }

  #define DEBUG_IMAGE_COMPOSITING 0
  #if(DEBUG_IMAGE_COMPOSITING)
  static Vision::Image dispCompositeImg;
  #endif
  
  Vision::Image compositeImage;
  if(IsModeEnabled(VisionMode::Markers_Composite)) {
    _imageCompositor->ComposeWith(imageCache.GetGray(whichSize));
    const size_t numImagesComposited = _imageCompositor->GetNumImagesComposited();

    const bool shouldRunOnComposite = (numImagesComposited % _imageCompositorReadyPeriod) == 0;
    if(shouldRunOnComposite) {
      _imageCompositor->GetCompositeImage(compositeImage);
      imagePtrs.push_back(&compositeImage);
      #if(DEBUG_IMAGE_COMPOSITING)
      dispCompositeImg.Allocate(compositeImage.GetNumRows(), compositeImage.GetNumCols());
      compositeImage.CopyTo(dispCompositeImg);
      #endif
    }

    const bool shouldReset = (numImagesComposited == _imageCompositorResetPeriod);
    if(shouldReset) {
      _imageCompositor->Reset();

      // This mode is considered processed iff:
      // - a composite image was produced for marker detection
      // - a reset occurs simultaneously
      // NOTE: by definition of the Ready and Reset periods, we're guaranteed
      //  to have run MarkerDetection in the same frame we trigger a Reset
      DEV_ASSERT_MSG(shouldRunOnComposite, "VisionSystem.DetectMarkers.InvalidResetCallBeforeImageUsed","");
      _currentResult.modesProcessed.Insert(VisionMode::Markers_Composite);
    }
  }

  #if(DEBUG_IMAGE_COMPOSITING)
  if(!dispCompositeImg.IsEmpty()) {
    _currentResult.debugImages.emplace_back("ImageCompositing", dispCompositeImg);
  }
  #endif
  
  // Set up cropping rectangles to cycle through each time DetectMarkers is called
  DEV_ASSERT(!imagePtrs.empty(), "VisionSystem.DetectMarkersWithCLAHE.NoImagePointers");
  static CropScheduler cropScheduler(_camera);
  if(!Util::IsNear(cropScheduler.GetCropWidthFraction(), kMarkerDetector_CropWidthFraction))
  {
    cropScheduler.Reset(kMarkerDetector_CropWidthFraction, CropScheduler::CyclingMode::Middle_Left_Middle_Right);
  }
  
  Rectangle<s32> cropRect;
  if(IsModeEnabled(VisionMode::Markers_FullFrame))
  {
    cropRect = Rectangle<s32>(0,0,imagePtrs.front()->GetNumCols(), imagePtrs.front()->GetNumRows());
    _currentResult.modesProcessed.Insert(VisionMode::Markers_FullFrame);
  }
  else
  {
    const bool useHorizontalCycling = !IsModeEnabled(VisionMode::Markers_FullWidth);
    const bool useVariableHeight = !IsModeEnabled(VisionMode::Markers_FullHeight);
    const bool cropInBounds = cropScheduler.GetCropRect(imagePtrs.front()->GetNumRows(),
                                                        imagePtrs.front()->GetNumCols(),
                                                        useHorizontalCycling,
                                                        useVariableHeight,
                                                        poseData, cropRect);

    if(!cropInBounds)
    {
      PRINT_CH_DEBUG(kLogChannelName, "VisionSystem.DetectMarkersWithCLAHE.CropRectOOB", "");
      return RESULT_OK;
    }
    
    DEV_ASSERT(cropRect.Area() > 0, "VisionSystem.DetectMarkersWithCLAHE.EmptyCrop");
    
    _currentResult.modesProcessed.Enable(VisionMode::Markers_FullWidth, !useHorizontalCycling);
    _currentResult.modesProcessed.Enable(VisionMode::Markers_FullHeight, !useVariableHeight);
  }
  
  Result lastResult = RESULT_OK;
  for(auto imgPtr : imagePtrs)
  {
    DEV_ASSERT(imgPtr->GetNumRows() == imagePtrs.front()->GetNumRows() &&
               imgPtr->GetNumCols() == imagePtrs.front()->GetNumCols(),
               "VisionSystem.DetectMarkersWithCLAHE.DifferingImageSizes");
    
    const Vision::Image& imgROI = imgPtr->GetROI(cropRect);
    lastResult = _markerDetector->Detect(imgROI, _currentResult.observedMarkers);
    if(RESULT_OK != lastResult) {
      break;
    }
    
    // Debug crop windows
    if(kMarkerDetector_VizCropScheduler)
    {
      Vision::ImageRGB dispImg;
      dispImg.SetFromGray(imgROI);
      for(auto const& marker : _currentResult.observedMarkers)
      {
        dispImg.DrawQuad(marker.GetImageCorners(), NamedColors::RED);
      }
      dispImg.DrawRect(Rectangle<s32>{0,0,cropRect.GetWidth(),cropRect.GetHeight()}, NamedColors::RED);
      _currentResult.debugImages.emplace_back("CroppedMarkers", dispImg);
    }
  }

  const bool meterFromChargerOnly = IsModeEnabled(VisionMode::Markers_ChargerOnly);
  _currentResult.modesProcessed.Enable(VisionMode::Markers_ChargerOnly, meterFromChargerOnly);
  
  auto markerIter = _currentResult.observedMarkers.begin();
  while(markerIter != _currentResult.observedMarkers.end())
  {
    auto & marker = *markerIter;
    
    if(meterFromChargerOnly && (marker.GetCode() != Vision::MARKER_CHARGER_HOME))
    {
      markerIter = _currentResult.observedMarkers.erase(markerIter);
      continue;
    }
    
    // Adjust the marker for the crop rectangle / processing resolution, to put it back in original image coordinates
    Quad2f scaledCorners(marker.GetImageCorners());
    if(cropRect.GetX() > 0 || cropRect.GetY() > 0 || kMarkerDetector_ScaleMultiplier != 1)
    {
      for(auto & corner : scaledCorners)
      {
        corner.x() += cropRect.GetX();
        corner.y() += cropRect.GetY();

        // By default we display images at the default image cache size so we need to scale the marker
        // corners to that size
        const f32 scaleMultiplier = ImageCacheSizeToScaleFactor(Vision::ImageCache::GetSize(kMarkerDetector_ScaleMultiplier));
        const f32 defaultScaleMultiplier = ImageCacheSizeToScaleFactor(Vision::ImageCache::GetDefaultImageCacheSize());
        corner *= (defaultScaleMultiplier / scaleMultiplier);
      }
      
      marker.SetImageCorners(scaledCorners);
    }
    
    // Add the bounding rect of the (unwarped) marker to the detection rectangles
    // Note that the scaled corners might get changed slightly by rolling shutter warping
    // below, making the detection rect slightly inaccurate, but these rectangles don't need
    // to be super accurate. And we'd rather still report something being detected here
    // even if the warping pushes a corner OOB, thereby removing the marker from the
    // actual reported list.
    detectionRects.emplace_back(scaledCorners);
    
    // Instead of correcting the entire image only correct the quads
    // Apply the appropriate shift to each of the corners of the quad to get a shifted quad
    if(_doRollingShutterCorrection)
    {
      bool allCornersInBounds = true;
      for(auto & corner : scaledCorners)
      {
        const s32 fullNumRows = imageCache.GetNumRows(Vision::ImageCacheSize::Half);
        const s32 fullNumCols = imageCache.GetNumCols(Vision::ImageCacheSize::Half);
        const int warpIndex = std::floor(corner.y() / (fullNumRows / _rollingShutterCorrector.GetNumDivisions()));
        DEV_ASSERT_MSG(warpIndex >= 0 && warpIndex < _rollingShutterCorrector.GetPixelShifts().size(),
                       "VisionSystem.DetectMarkersWithCLAHE.WarpIndexOOB", "Index:%d Corner y:%f",
                       warpIndex, corner.y());
        
        const Vec2f& pixelShift = _rollingShutterCorrector.GetPixelShifts().at(warpIndex);
        corner -= pixelShift;

        if(Util::IsFltLTZero(corner.x()) ||
           Util::IsFltLTZero(corner.y()) ||
           Util::IsFltGE(corner.x(), (f32)fullNumCols) ||
           Util::IsFltGE(corner.y(), (f32)fullNumRows))
        {
          // Warped corner is outside image bounds. Just drop this entire marker. Technically, we could still
          // probably estimate its pose just fine, but other things later may expect all corners to be in bounds
          // so easier just not to risk it.
          allCornersInBounds = false;
          break; // no need to warp remaining corners once any one is OOB
        }
      }
      
      if(!allCornersInBounds)
      {
        // Remove this OOB marker from the list entirely
        PRINT_CH_DEBUG(kLogChannelName, "VisionSystem.DetectMarkersWithCLAHE.RemovingMarkerOOB",
                       "%s", Vision::MarkerTypeStrings[marker.GetCode()]);
        markerIter = _currentResult.observedMarkers.erase(markerIter);
        continue;
      }
      
      marker.SetImageCorners(scaledCorners); // "scaled" corners are now the warped corners
    }
    
    ++markerIter;
  }
  
  return lastResult;
  
} // DetectMarkers
  
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void VisionSystem::CheckForNeuralNetResults()
{
  for(const auto& neuralNetRunnerEntry : _neuralNetRunners)
  {
    const std::string& networkName = neuralNetRunnerEntry.first;
    const auto& neuralNetRunner = neuralNetRunnerEntry.second;
    
    const bool resultReady = neuralNetRunner->GetDetections(_currentResult.salientPoints);
    if(resultReady)
    {
      PRINT_CH_DEBUG(kLogChannelName, "VisionSystem.CheckForNeuralNetResults.GotDetections",
                     "Network:%s NumSalientPoints:%zu",
                     networkName.c_str(), _currentResult.salientPoints.size());
      
      std::set<VisionMode> modes;
      const bool success = GetVisionModesForNeuralNet(networkName, modes);
      if(ANKI_VERIFY(success, "VisionSystem.CheckForNeuralNetResults.NoModeForNetworkName", "Name: %s",
                     networkName.c_str()))
      {
          
        for(auto & mode : modes)
        {
          _currentResult.modesProcessed.Insert(mode);
        }
        
        if(IsModeEnabled(VisionMode::SaveImages))
        {
          const Vision::ImageRGB& neuralNetRunnerImage = neuralNetRunner->GetOrigImg();
          
          if(!neuralNetRunnerImage.IsEmpty() &&
             _imageSaver->WantsToSave(_currentResult, neuralNetRunnerImage.GetTimestamp()))
          {
            const Result saveResult = _imageSaver->Save(neuralNetRunnerImage, _frameNumber);
            if(RESULT_OK == saveResult)
            {
              _currentResult.modesProcessed.Insert(VisionMode::SaveImages);
            }
            
            const std::string jsonFilename = _imageSaver->GetFullFilename(_frameNumber, "json");
            Json::Value jsonSalientPoints;
            NeuralNets::INeuralNetMain::ConvertSalientPointsToJson(_currentResult.salientPoints, false,
                                                                   jsonSalientPoints);
            const bool success = NeuralNets::INeuralNetMain::WriteResults(jsonFilename, jsonSalientPoints);
            if(!success)
            {
              LOG_WARNING("VisionSystem.CheckForNeuralNets.WriteJsonSalientPointsFailed",
                          "Writing %zu salient points to %s",
                          _currentResult.salientPoints.size(), jsonFilename.c_str());
            }
          }
        }
          
        if(ANKI_DEV_CHEATS)
        {
          const Vision::ImageRGB& neuralNetRunnerImage = neuralNetRunner->GetOrigImg();
          if(!neuralNetRunnerImage.IsEmpty())
          {
            AddFakeDetections(neuralNetRunnerImage.GetTimestamp(), modes);
          }
        }
      }
    } // if(resultReady)
    
  }
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void VisionSystem::AddFakeDetections(const TimeStamp_t atTimestamp, const std::set<VisionMode>& modes)
{
  // DEBUG: Randomly fake detections of hands and pets if this network was registered to those modes
  if(Util::IsFltGTZero(kFakeHandDetectionProbability) ||
     Util::IsFltGTZero(kFakeCatDetectionProbability) ||
     Util::IsFltGTZero(kFakeDogDetectionProbability))
  {
    std::vector<Vision::SalientPointType> fakeDetectionsToAdd;
    for(auto & mode : modes)
    {
      _currentResult.modesProcessed.Insert(mode);
      
      static Util::RandomGenerator rng;
      if((VisionMode::Hands == mode) && (rng.RandDbl() < kFakeHandDetectionProbability))
      {
        fakeDetectionsToAdd.emplace_back(Vision::SalientPointType::Hand);
      }
      if((VisionMode::Pets == mode) && (rng.RandDbl() < kFakeCatDetectionProbability))
      {
        fakeDetectionsToAdd.emplace_back(Vision::SalientPointType::Cat);
      }
      if((VisionMode::Pets == mode) && (rng.RandDbl() < kFakeDogDetectionProbability))
      {
        fakeDetectionsToAdd.emplace_back(Vision::SalientPointType::Dog);
      }
    }
    for(const auto& type : fakeDetectionsToAdd)
    {
      // Simple full-image "classification" SalientPoint
      Vision::SalientPoint salientPoint(atTimestamp,
                                        0.5f, 0.5f, 1.f, 1.f,
                                        type, EnumToString(type),
                                        {CladPoint2d{0.f,0.f}, CladPoint2d{0.f,1.f}, CladPoint2d{1.f,1.f}, CladPoint2d{1.f,0.f}},
                                        0);
      _currentResult.salientPoints.emplace_back(std::move(salientPoint));
    }
  }
}
  
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void VisionSystem::UpdateRollingShutter(const VisionPoseData& poseData, const Vision::ImageCache& imageCache)
{
  // If we've already updated the corrector at this timestamp, don't have to do it again
  if(!_doRollingShutterCorrection || (imageCache.GetTimeStamp() <= _lastRollingShutterCorrectionTime))
  {
    return;
  }

  Tic("RollingShutterComputePixelShifts");
  s32 numRows = imageCache.GetNumRows(Vision::ImageCacheSize::Half);
  _rollingShutterCorrector.ComputePixelShifts(poseData, _prevPoseData, numRows);
  Toc("RollingShutterComputePixelShifts");
  _lastRollingShutterCorrectionTime = imageCache.GetTimeStamp();
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
Result VisionSystem::Update(const VisionSystemInput& input)
{
  _imageCache->Reset(input.imageBuffer);

  _modes = input.modesToProcess;
  _futureModes = input.futureModesToProcess;
  _imageCompressQuality = input.imageCompressQuality;
  _vizImageBroadcastSize = input.vizImageBroadcastSize;
  
  return Update(input.poseData, *_imageCache);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// This is the regular Update() call
Result VisionSystem::Update(const VisionPoseData& poseData, Vision::ImageCache& imageCache)
{
  Result lastResult = RESULT_OK;
  
  if(!_isInitialized || !_camera.IsCalibrated())
  {
    PRINT_NAMED_WARNING("VisionSystem.Update.NotReady",
                        "Must be initialized and have calibrated camera to Update");
    return RESULT_FAIL;
  }
  
  _frameNumber++;
  
  // Set up the results for this frame:
  VisionProcessingResult result;
  result.timestamp = imageCache.GetTimeStamp();
  result.imageQuality = Vision::ImageQuality::Unchecked;
  result.cameraParams.exposureTime_ms = -1;
  std::swap(result, _currentResult);
  
  auto& visionModesProcessed = _currentResult.modesProcessed;
  visionModesProcessed.Clear();
  
  // Store the new robot state and keep a copy of the previous one
  UpdatePoseData(poseData);
  
  bool& cameraParamsRequested = _nextCameraParams.first;
  if(cameraParamsRequested)
  {
    _currentCameraParams = _nextCameraParams.second;
    cameraParamsRequested = false;
    
    _cameraParamsController->UpdateCurrentCameraParams(_currentCameraParams);
  }
  
  if(_modes.IsEmpty())
  {
    // Push the empty result and bail
    _mutex.lock();
    _results.push(_currentResult);
    _mutex.unlock();
    return RESULT_OK;
  }
  
  if(kVisionSystemSimulatedDelay_ms > 0)
  {
    std::this_thread::sleep_for(std::chrono::milliseconds(kVisionSystemSimulatedDelay_ms));
  }
  
  // Begin image processing
  // Apply CLAHE if enabled:
  DEV_ASSERT(kUseCLAHE_u8 < Util::EnumToUnderlying(MarkerDetectionCLAHE::Count),
             "VisionSystem.ApplyCLAHE.BadUseClaheVal");
  MarkerDetectionCLAHE useCLAHE = static_cast<MarkerDetectionCLAHE>(kUseCLAHE_u8);
  Vision::Image claheImage;
  
  // Note: this will do nothing and leave claheImage empty if CLAHE is disabled
  // entirely or for this frame.
  lastResult = ApplyCLAHE(imageCache, useCLAHE, claheImage);
  ANKI_VERIFY(RESULT_OK == lastResult, "VisionSystem.Update.FailedCLAHE", "ApplyCLAHE supposedly has no failure mode");
  
  if(IsModeEnabled(VisionMode::Stats))
  {
    Tic("TotalStats");
    _currentResult.imageMean = ComputeMean(imageCache, kImageMeanSampleInc);
    visionModesProcessed.Insert(VisionMode::Stats);
    Toc("TotalStats");
  }

  DetectionRectsByMode detectionsByMode;

  bool anyModeFailures = false;
  
  
  if(IsModeEnabled(VisionMode::Markers))
  {
    if(IsModeEnabled(VisionMode::Markers_Off))
    {
      // Marker detection is forcibly disabled (Gross, see VIC-6838)
      visionModesProcessed.Insert(VisionMode::Markers, VisionMode::Markers_Off);
    }
    else
    {
      const bool allowWhileRotatingFast = IsModeEnabled(VisionMode::Markers_FastRotation);
      const bool wasRotatingTooFast = ( allowWhileRotatingFast ? false :
                                       poseData.imuDataHistory.WasRotatingTooFast(imageCache.GetTimeStamp(),
                                                                                  DEG_TO_RAD(kBodyTurnSpeedThreshBlock_degs),
                                                                                  DEG_TO_RAD(kHeadTurnSpeedThreshBlock_degs)));
      if(!wasRotatingTooFast)
      {
        // Marker detection uses rolling shutter compensation
        UpdateRollingShutter(poseData, imageCache);
        
        Tic("TotalMarkers");
        lastResult = DetectMarkers(imageCache, claheImage, detectionsByMode[VisionMode::Markers], useCLAHE, poseData);
        
        if(RESULT_OK != lastResult) {
          PRINT_NAMED_ERROR("VisionSystem.Update.DetectMarkersFailed", "");
          anyModeFailures = true;
        } else {
          visionModesProcessed.Insert(VisionMode::Markers);
          visionModesProcessed.Enable(VisionMode::Markers_FastRotation, allowWhileRotatingFast);
        }
        Toc("TotalMarkers");
      }
    }
  }
  
  if(!IsModeEnabled(VisionMode::Markers_Composite) && 
     _imageCompositor->GetNumImagesComposited() > 0) {
    // Clears any leftover artifacts from prematurely cancelled ImageCompositing
    // Check this here to avoid gating it on whether or not DetectMarkers
    _imageCompositor->Reset();
  }
  
  if(IsModeEnabled(VisionMode::Faces))
  {
    const bool estimatingFacialExpression = IsModeEnabled(VisionMode::Faces_Expression);
    _faceTracker->EnableEmotionDetection(estimatingFacialExpression);

    const bool detectingSmile = IsModeEnabled(VisionMode::Faces_Smile);
    _faceTracker->EnableSmileDetection(detectingSmile);

    const bool detectingGaze = IsModeEnabled(VisionMode::Faces_Gaze);
    _faceTracker->EnableGazeDetection(detectingGaze);

    const bool detectingBlink = IsModeEnabled(VisionMode::Faces_Blink);
    _faceTracker->EnableBlinkDetection(detectingBlink);

    
    Tic("TotalFaces");
    // NOTE: To use rolling shutter in DetectFaces, call UpdateRollingShutterHere
    // See: VIC-1417 
    // UpdateRollingShutter(poseData, imageCache);
    const bool useCropping = IsModeEnabled(VisionMode::Faces_Crop);
    if((lastResult = DetectFaces(imageCache, detectionsByMode[VisionMode::Faces], useCropping)) != RESULT_OK) {
      PRINT_NAMED_ERROR("VisionSystem.Update.DetectFacesFailed", "");
      anyModeFailures = true;
    } else {
      visionModesProcessed.Insert(VisionMode::Faces);
      visionModesProcessed.Enable(VisionMode::Faces_Crop,          useCropping);
      visionModesProcessed.Enable(VisionMode::Faces_Expression,    estimatingFacialExpression);
      visionModesProcessed.Enable(VisionMode::Faces_Smile,         detectingSmile);
      visionModesProcessed.Enable(VisionMode::Faces_Gaze,          detectingGaze);
      visionModesProcessed.Enable(VisionMode::Faces_Blink,         detectingBlink);
    }
    Toc("TotalFaces");
  }
  
  if(IsModeEnabled(VisionMode::Pets))
  {
    Tic("TotalPets");
    if((lastResult = DetectPets(imageCache, detectionsByMode[VisionMode::Pets])) != RESULT_OK) {
      PRINT_NAMED_ERROR("VisionSystem.Update.DetectPetsFailed", "");
      anyModeFailures = true;
    } else {
      visionModesProcessed.Insert(VisionMode::Pets);
    }
    Toc("TotalPets");
  }
  
  if(IsModeEnabled(VisionMode::Motion))
  {
    Tic("TotalMotion");
    if((lastResult = DetectMotion(imageCache)) != RESULT_OK) {
      PRINT_NAMED_ERROR("VisionSystem.Update.DetectMotionFailed", "");
      anyModeFailures = true;
    } else {
      visionModesProcessed.Insert(VisionMode::Motion);
    }
    Toc("TotalMotion");
  }

  if(IsModeEnabled(VisionMode::BrightColors)){
    if (imageCache.HasColor()){
      Tic("TotalBrightColors");
      lastResult = DetectBrightColors(imageCache);
      Toc("TotalBrightColors");
      if (lastResult != RESULT_OK){
        PRINT_NAMED_ERROR("VisionSystem.Update.DetectBrightColorsFailed","");
        anyModeFailures = true;
      } else {
        visionModesProcessed.Insert(VisionMode::BrightColors);
      }
    } else {
      PRINT_NAMED_WARNING("VisionSystem.Update.NoColorImage", "Could not process bright colors. No color image!");
    }
  }
  // Disabling this while VisionMode::OverheadMap is disabled
  if (IsModeEnabled(VisionMode::OverheadMap))
  {
    if (imageCache.HasColor()) {
      Tic("UpdateOverheadMap");
      lastResult = UpdateOverheadMap(imageCache);
      Toc("UpdateOverheadMap");
      if (lastResult != RESULT_OK) {
        anyModeFailures = true;
      } else {
        visionModesProcessed.Insert(VisionMode::OverheadMap);
      }
    }
    else {
      PRINT_NAMED_WARNING("VisionSystem.Update.NoColorImage", "Could not process overhead map. No color image!");
    }
  }
  
  if (IsModeEnabled(VisionMode::Obstacles))
  {
    if (imageCache.HasColor()) {
      Tic("DetectVisualObstacles");
      lastResult = UpdateGroundPlaneClassifier(imageCache);
      Toc("DetectVisualObstacles");
      if (lastResult != RESULT_OK) {
        anyModeFailures = true;
      } else {
        visionModesProcessed.Insert(VisionMode::Obstacles);
      }
    }
    else {
      PRINT_NAMED_WARNING("VisionSystem.Update.NoColorImage", "Could not process visual obstacles. No color image!");
    }
  }

  if(IsModeEnabled(VisionMode::OverheadEdges))
  {
    Tic("TotalOverheadEdges");

    lastResult = _overheadEdgeDetector->Detect(imageCache, _poseData, _currentResult);
    
    if(lastResult != RESULT_OK) {
      PRINT_NAMED_ERROR("VisionSystem.Update.DetectOverheadEdgesFailed", "");
      anyModeFailures = true;
    } else {
      visionModesProcessed.Insert(VisionMode::OverheadEdges);
    }
    Toc("TotalOverheadEdges");
  }
  
  if(IsModeEnabled(VisionMode::Calibration))
  {
    switch(kCalibTargetType)
    {
      case CameraCalibrator::CalibTargetType::CHECKERBOARD:
      {
        lastResult = _cameraCalibrator->ComputeCalibrationFromCheckerboard(_currentResult.cameraCalibration,
                                                                           _currentResult.debugImages);
        break;
      }
      case CameraCalibrator::CalibTargetType::QBERT:
      case CameraCalibrator::CalibTargetType::INVERTED_BOX:
      {
        // Marker detection needs to have run before trying to do single target calibration
        DEV_ASSERT(visionModesProcessed.Contains(VisionMode::Markers),
                   "VisionSystem.Update.Calibration.MarkersNotDetected");
        
        CameraCalibrator::CalibTargetType targetType = static_cast<CameraCalibrator::CalibTargetType>(kCalibTargetType);
        lastResult = _cameraCalibrator->ComputeCalibrationFromSingleTarget(targetType,
                                                                           _currentResult.observedMarkers,
                                                                           _currentResult.cameraCalibration,
                                                                           _currentResult.debugImages);
        break;
      }
    }
    if(lastResult != RESULT_OK) {
      PRINT_NAMED_ERROR("VisionSystem.Update.ComputeCalibrationFailed", "");
      anyModeFailures = true;
    } else {
      visionModesProcessed.Insert(VisionMode::Calibration);
    }
  }
  
  if(IsModeEnabled(VisionMode::Lasers))
  {
    // Skip laser point detection if the Laser FeatureGate is disabled.
    // TODO: Remove this once laser feature is enabled (COZMO-11185)
    if(_context->GetFeatureGate()->IsFeatureEnabled(FeatureType::Laser))
    {
      Tic("TotalLasers");
      if((lastResult = DetectLaserPoints(imageCache)) != RESULT_OK) {
        PRINT_NAMED_ERROR("VisionSystem.Update.DetectlaserPointsFailed", "");
        anyModeFailures = true;
      } else {
        visionModesProcessed.Insert(VisionMode::Lasers);
      }
      Toc("TotalLasers");
    }
  }

  // Check for any results from any neural nets thave have been running (asynchronously).
  // Note that any SalientPoints found will be from a different image than the one in the cache and
  // will have their own timestamp which does not match the current VisionProcessingResult.
  CheckForNeuralNetResults();
  
  // Figure out if any modes requiring neural nets are enabled and keep up with the
  // set of networks needed to satisfy those modes (multiple modes could use the same
  // network!)
  std::set<std::string> networksToRun;
  for(const auto& mode : GetVisionModesUsingNeuralNets())
  {
    if(IsModeEnabled(mode))
    {
      std::set<std::string> networkNames;
      const bool success = GetNeuralNetsForVisionMode(mode, networkNames);
      if(ANKI_VERIFY(success, "VisionSystem.Update.NoNetworkForMode", "%s", EnumToString(mode)))
      {
        networksToRun.insert(networkNames.begin(), networkNames.end());
      }
    }
  }
  
  // Run the set of required networks
  for(const auto& networkName : networksToRun)
  {
    auto iter = _neuralNetRunners.find(networkName);
    if(iter == _neuralNetRunners.end())
    {
      // If the network does not exist, something has been configured / set up wrong in vision_config.json or
      // perhaps in registering network names and VisionModes in visionModeHelpers.cpp. Die immediately.
      // Don't waste time sifting through logs trying to figure out why the associated features isn't working.
      LOG_ERROR("VisionSystem.Update.MissingNeuralNet",
                "Requested to run network named %s but no runner for it exists",
                networkName.c_str());
      exit(-1);
    }
    
    const bool started = iter->second->StartProcessingIfIdle(imageCache);
    if(started)
    {
      PRINT_CH_DEBUG("NeuralNets", "VisionSystem.Update.StartedNeuralNet", "Running %s on image at time t:%u",
                     networkName.c_str(), imageCache.GetTimeStamp());
    }
  }
  
  // Check for illumination state
  if(IsModeEnabled(VisionMode::Illumination) &&
     !IsModeEnabled(VisionMode::AutoExp_Cycling)) // don't check for illumination if cycling exposure
  {
    Tic("Illumination");
    lastResult = DetectIllumination(imageCache);
    Toc("Illumination");
    if (lastResult != RESULT_OK) {
      PRINT_NAMED_ERROR("VisionSystem.Update.DetectIlluminationFailed", "");
      anyModeFailures = true;
    } else {
      visionModesProcessed.Insert(VisionMode::Illumination);
    }
  }

  UpdateMeteringRegions(imageCache.GetTimeStamp(), std::move(detectionsByMode));
  
  // NOTE: This should come after any detectors that add things to "detectionRects"
  //       since it meters exposure based on those.
  const bool isWhiteBalancing = IsModeEnabled(VisionMode::WhiteBalance);
  const bool isAutoExposing   = IsModeEnabled(VisionMode::AutoExp);
  if(isAutoExposing || isWhiteBalancing)
  {
    Tic("UpdateCameraParams");
    lastResult = UpdateCameraParams(imageCache);
    Toc("UpdateCameraParams");
    
    if(RESULT_OK != lastResult) {
      PRINT_NAMED_ERROR("VisionSystem.Update.UpdateCameraParamsFailed", "");
      anyModeFailures = true;
    } else {
      visionModesProcessed.Enable(VisionMode::AutoExp, isAutoExposing);
      visionModesProcessed.Enable(VisionMode::WhiteBalance, isWhiteBalancing);
    }
  }
  
  if(IsModeEnabled(VisionMode::Benchmark))
  {
    Tic("Benchmarking");
    const Result benchMarkResult = _benchmark->Update(imageCache);
    Toc("BenchMarking");
    
    if(RESULT_OK != benchMarkResult) {
      PRINT_NAMED_ERROR("VisionSystem.Update.BenchmarkFailed", "");
      // Continue processing, since this should be independent of other modes
    } else {
      visionModesProcessed.Insert(VisionMode::Benchmark);
    }
  }
  
  if(IsModeEnabled(VisionMode::SaveImages) && _imageSaver->WantsToSave(_currentResult, imageCache.GetTimeStamp()))
  {
    Tic("SaveImages");
    
    // Check this before calling Save(), since that can modify imageSaver's state
    const bool shouldSaveSensorData = _imageSaver->ShouldSaveSensorData();
    
    const Result saveResult = _imageSaver->Save(imageCache, _frameNumber);
    
    const Result saveSensorResult = (shouldSaveSensorData ? SaveSensorData() : RESULT_OK);
    
    Toc("SaveImages");

    if((RESULT_OK != saveResult) || (RESULT_OK != saveSensorResult)) // || (RESULT_OK != thumbnailResult))
    {
      PRINT_NAMED_ERROR("VisionSystem.Update.SaveImagesFailed", "Image:%s SensorData:%s",
                        (RESULT_OK == saveResult ? "OK" : "FAIL"),
                        (RESULT_OK == saveSensorResult ? "OK" : "FAIL"));
      // Continue processing, since this should be independent of other modes
    }
    else {
      visionModesProcessed.Insert(VisionMode::SaveImages);
    }
  }

  if(IsModeEnabled(VisionMode::Viz))
  {
    Tic("Viz");

    _currentResult.compressedDisplayImg.Compress(imageCache.GetRGB(_vizImageBroadcastSize), _imageCompressQuality);

    Toc("Viz");

    visionModesProcessed.Insert(VisionMode::Viz);
  }

  if(kDisplayUndistortedImages)
  {
    Vision::ImageRGB img = imageCache.GetRGB();
    Vision::ImageRGB imgUndistorted(img.GetNumRows(),img.GetNumCols());
    DEV_ASSERT(_camera.IsCalibrated(), "VisionComponent.GetCalibrationImageJpegData.NoCalibration");
    img.Undistort(*_camera.GetCalibration(), imgUndistorted);
    _currentResult.debugImages.emplace_back("undistorted", imgUndistorted);
  }

  // NOTE: This should come at the end because it relies on elements of the current VisionProcessingResult
  //       (i.e. _currentResult) to be populated for the purposes of drawing them.
  //       Note that any asynchronous results (e.g. from neural nets) get drawn on whatever image is current
  //       when they complete, for better or worse, so they are not actually in sync.
  if(IsModeEnabled(VisionMode::MirrorMode))
  {
    // TODO: Add an ImageCache::Size for MirrorMode directly
    const Result result = _mirrorModeManager->CreateMirrorModeImage(imageCache.GetRGB(), _currentResult);
    if(RESULT_OK != result)
    {
      PRINT_NAMED_ERROR("VisionSystem.Update.MirrorModeFailed", "");
    } else {
      visionModesProcessed.Insert(VisionMode::MirrorMode);
    }
  }
  
  // We've computed everything from this image that we're gonna compute.
  // Push the result, along with any neural net results, onto the queue of results all together.
  _mutex.lock();
  _results.push(_currentResult);
  _mutex.unlock();
  
  return (anyModeFailures ? RESULT_FAIL : RESULT_OK);
} // Update()

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
Result VisionSystem::SaveSensorData() const {

  const std::string fullFilename = _imageSaver->GetFullFilename(_frameNumber, "json");

  PRINT_CH_DEBUG(kLogChannelName, "VisionSystem.SaveSensorData.Filename", "Saving to %s",
                 fullFilename.c_str());
  
  std::ofstream outFile(fullFilename);
  if (! outFile.is_open()) {
    PRINT_NAMED_ERROR("VisionSystem.SaveSensorData.CantOpenFile", "Can't open file %s for writing",
                      fullFilename.c_str());
    return RESULT_FAIL;
  }

  Json::Value config;
  {

    const HistRobotState& state = _poseData.histState;
    // prox sensor
    const auto& proxData = state.GetProxSensorData();
    if (proxData.foundObject) {
      config["proxSensor"] = proxData.distance_mm;
    }
    else {
      config["proxSensor"] = -1;
    }

    // cliff sensor
    config["frontLeftCliff"]  = state.WasCliffDetected(CliffSensor::CLIFF_FL);
    config["frontRightCliff"] = state.WasCliffDetected(CliffSensor::CLIFF_FR);
    config["backLeftCliff"]   = state.WasCliffDetected(CliffSensor::CLIFF_BL);
    config["backRightCliff"]  = state.WasCliffDetected(CliffSensor::CLIFF_BR);

    // robot motion flags
    // We don't record "WasCameraMoving" since it's HeadMoving || WheelsMoving
    config["wasCarryingObject"] = state.WasCarryingObject();
    config["wasMoving"] = state.WasMoving();
    config["WasHeadMoving"] = state.WasHeadMoving();
    config["WasLiftMoving"] = state.WasLiftMoving();
    config["WereWheelsMoving"] = state.WereWheelsMoving();
    config["wasPickedUp"] = state.WasPickedUp();

    // head angle
    config["headAngle"] = state.GetHeadAngle_rad();
    // lift angle
    config["liftAngle"] = state.GetLiftAngle_rad();

    // camera exposure, gain, white balance
    // Make sure to get parameters for current image, not next image
    // NOTE: Due to latency between interface call and actual register writes,
    // the so-called current params may not actually be current
    config["requestedCamExposure"] = _currentCameraParams.exposureTime_ms;
    config["requestedCamGain"] = _currentCameraParams.gain;
    config["requestedCamWhiteBalanceRed"] = _currentCameraParams.whiteBalanceGainR;
    config["requestedCamWhiteBalanceGreen"] = _currentCameraParams.whiteBalanceGainG;
    config["requestedCamWhiteBalanceBlue"] = _currentCameraParams.whiteBalanceGainB;

    // image timestamp
    config["imageTimestamp"] = (TimeStamp_t)_currentResult.timestamp;
  }

  Json::StyledWriter writer;
  outFile << writer.write(config);
  outFile.close();

  return RESULT_OK;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
Vision::CameraParams VisionSystem::GetCurrentCameraParams() const
{
  // Return nextParams if they have not been set yet otherwise use currentParams
  return (_nextCameraParams.first ? _nextCameraParams.second : _currentCameraParams);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
Result VisionSystem::SetCameraExposureParams(const s32 currentExposureTime_ms,
                                             const f32 currentGain,
                                             const GammaCurve& gammaCurve)
{
  // TODO: Expose these x values ("knee locations") somewhere. These are specific to the camera.
  // (So I'm keeping them out of Vision::ImagingPipeline and defined in Cozmo namespace)
  static const std::vector<u8> kKneeLocations{
    0, 8, 16, 24, 32, 40, 48, 64, 80, 96, 112, 128, 144, 160, 192, 224, 255
  };
  
  std::vector<u8> gammaVector(gammaCurve.begin(), gammaCurve.end());
  
  Result result = _cameraParamsController->SetGammaTable(kKneeLocations, gammaVector);
  if(RESULT_OK != result)
  {
    PRINT_NAMED_WARNING("VisionSystem.SetCameraExposureParams.BadGammaCurve", "");
  }
  
  const Vision::CameraParams cameraParams(currentExposureTime_ms, currentGain,
                                          _currentCameraParams.whiteBalanceGainR,
                                          _currentCameraParams.whiteBalanceGainG,
                                          _currentCameraParams.whiteBalanceGainB);
  
  SetNextCameraParams(cameraParams);
  
  PRINT_CH_INFO(kLogChannelName, "VisionSystem.SetCameraExposureParams.Success",
                "Current Exposure Time:%dms, Gain:%.3f",
                currentExposureTime_ms, currentGain);

  return RESULT_OK;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
Result VisionSystem::GetSerializedFaceData(std::vector<u8>& albumData,
                                           std::vector<u8>& enrollData) const
{
  DEV_ASSERT(nullptr != _faceTracker, "VisionSystem.GetSerializedFaceData.NullFaceTracker");
  return _faceTracker->GetSerializedData(albumData, enrollData);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
Result VisionSystem::SetSerializedFaceData(const std::vector<u8>& albumData,
                                           const std::vector<u8>& enrollData,
                                           std::list<Vision::LoadedKnownFace>& loadedFaces)
{
  DEV_ASSERT(nullptr != _faceTracker, "VisionSystem.SetSerializedFaceData.NullFaceTracker");
  return _faceTracker->SetSerializedData(albumData, enrollData, loadedFaces);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
Result VisionSystem::LoadFaceAlbum(const std::string& albumName, std::list<Vision::LoadedKnownFace> &loadedFaces)
{
  DEV_ASSERT(nullptr != _faceTracker, "VisionSystem.LoadFaceAlbum.NullFaceTracker");
  return _faceTracker->LoadAlbum(albumName, loadedFaces);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
Result VisionSystem::SaveFaceAlbum(const std::string& albumName)
{
  DEV_ASSERT(nullptr != _faceTracker, "VisionSystem.SaveFaceAlbum.NullFaceTracker");
  return _faceTracker->SaveAlbum(albumName);
}
  
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void VisionSystem::SetFaceRecognitionIsSynchronous(bool isSynchronous)
{
  DEV_ASSERT(nullptr != _faceTracker, "VisionSystem.SetFaceRecognitionRunMode.NullFaceTracker");
  _faceTracker->SetRecognitionIsSynchronous(isSynchronous);
}

void VisionSystem::ClearImageCache()
{
  _imageCache->ReleaseMemory();
}

void VisionSystem::AddAllowedTrackedFace(const Vision::FaceID_t faceID)
{
  _faceTracker->AddAllowedTrackedFace(faceID);
}

void VisionSystem::ClearAllowedTrackedFaces()
{
  _faceTracker->ClearAllowedTrackedFaces();
}

f32 VisionSystem::GetBodyTurnSpeedThresh_degPerSec()
{
  return kBodyTurnSpeedThreshBlock_degs;
}

} // namespace Vector
} // namespace Anki
