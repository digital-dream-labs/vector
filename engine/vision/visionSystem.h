/**
 * File: visionSystem.h [Basestation]
 *
 * Author: Andrew Stein
 * Date:   (various)
 *
 * Description: High-level module that controls the basestation vision system
 *              Runs on its own thread inside VisionComponent.
 *
 * Copyright: Anki, Inc. 2014
 **/

#ifndef ANKI_COZMO_BASESTATION_VISIONSYSTEM_H
#define ANKI_COZMO_BASESTATION_VISIONSYSTEM_H

#include "coretech/common/engine/math/polygon_fwd.h"
#include "coretech/common/shared/types.h"

#include "anki/cozmo/shared/cozmoConfig.h"

#include "engine/overheadEdge.h"
#include "engine/robotStateHistory.h"
#include "engine/rollingShutterCorrector.h"
#include "engine/vision/cameraCalibrator.h"
#include "engine/vision/groundPlaneROI.h"
#include "engine/vision/visionModeSet.h"
#include "engine/vision/visionPoseData.h"
#include "engine/vision/visionProcessingResult.h"
#include "engine/vision/visionSystemInput.h"

#include "coretech/common/engine/matlabInterface.h"
#include "coretech/common/engine/robotTimeStamp.h"
#include "coretech/vision/engine/brightColorDetector.h"
#include "coretech/vision/engine/camera.h"
#include "coretech/vision/engine/cameraCalibration.h"
#include "coretech/vision/engine/compressedImage.h"
#include "coretech/vision/engine/debugImageList.h"
#include "coretech/vision/engine/imageCache.h"
#include "coretech/vision/engine/profiler.h"
#include "coretech/vision/engine/trackedFace.h"
#include "coretech/vision/engine/trackedPet.h"
#include "coretech/vision/engine/visionMarker.h"

#include "clad/vizInterface/messageViz.h"
#include "clad/robotInterface/messageEngineToRobot.h"
#include "clad/types/cameraParams.h"
#include "clad/types/imageTypes.h"
#include "clad/types/loadedKnownFace.h"
#include "clad/types/salientPointTypes.h"
#include "clad/types/visionModes.h"
#include "clad/externalInterface/messageEngineToGame.h"

#include "util/bitFlags/bitFlags.h"

#include <mutex>
#include <queue>

namespace Anki {
  
namespace NeuralNets {
  class NeuralNetRunner;
}
 
namespace Vision {
  class Benchmark;
  class BrightColorDetector;
  class CameraParamsController;
  class FaceTracker;
  class ImageCache;
  class MarkerDetector;
  class PetTracker;
  class ImageCompositor;
}
  
namespace Vector {
    
  // Forward declaration:
  class CameraCalibrator;
  class CozmoContext;
  class IlluminationDetector;
  class ImageSaver;
  struct ImageSaverParams;
  class LaserPointDetector;
  class MirrorModeManager;
  class MotionDetector;
  class OverheadEdgesDetector;
  class OverheadMap;
  class Robot;
  class VizManager;
  class GroundPlaneClassifier;
  
  class VisionSystem : public Vision::Profiler
  {
  public:

    VisionSystem(const CozmoContext* context);
    ~VisionSystem();
    
    //
    // Methods:
    //
    
    Result Init(const Json::Value& config);
    bool   IsInitialized() const;
    
    Result UpdateCameraCalibration(std::shared_ptr<Vision::CameraCalibration> camCalib);
    
    const VisionModeSet& GetEnabledModes() const { return _modes; }
    bool  IsModeEnabled(VisionMode whichMode) const { return _modes.Contains(whichMode); }
    
    // This is main Update() call to be called in a loop from above.
    Result Update(const VisionPoseData& robotState,
                  Vision::ImageCache& imageCache);
    
    Result Update(const VisionSystemInput& input);
    
    // Wrappers for camera calibration
    Result AddCalibrationImage(const Vision::Image& calibImg, const Anki::Rectangle<s32>& targetROI) { return _cameraCalibrator->AddCalibrationImage(calibImg, targetROI); }
    Result ClearCalibrationImages() { return _cameraCalibrator->ClearCalibrationImages(); }
    size_t GetNumStoredCalibrationImages() const { return _cameraCalibrator->GetNumStoredCalibrationImages(); }
    const std::vector<CameraCalibrator::CalibImage>& GetCalibrationImages() const {return _cameraCalibrator->GetCalibrationImages();}
    const std::vector<Pose3d>& GetCalibrationPoses() const { return _cameraCalibrator->GetCalibrationPoses();}

    // VisionMode <-> String Lookups
    std::string GetCurrentModeName() const;
    VisionMode  GetModeFromString(const std::string& str) const;
    
    bool CanAddNamedFace() const;
    Result AssignNameToFace(Vision::FaceID_t faceID, const std::string& name, Vision::FaceID_t mergeWithID);
    
    // Enable face enrollment mode and optionally specify the ID for which 
    // enrollment is allowed (use UnknownFaceID to indicate "any" ID).
    // Enrollment will automatically disable after numEnrollments. (Use 
    // a value < 0 to enable ongoing enrollments.)
    void SetFaceEnrollmentMode(Vision::FaceID_t forFaceID = Vision::UnknownFaceID,
                               s32 numEnrollments = -1,
                               bool forceNewID = false);

#if ANKI_DEV_CHEATS
    void SaveAllRecognitionImages(const std::string& imagePathPrefix);
    void DeleteAllRecognitionImages();
#endif
    
    void SetFaceRecognitionIsSynchronous(bool isSynchronous);
    
    Result LoadFaceAlbum(const std::string& albumName, std::list<Vision::LoadedKnownFace>& loadedFaces);
    
    Result SaveFaceAlbum(const std::string& albumName);
    
    Result GetSerializedFaceData(std::vector<u8>& albumData,
                                 std::vector<u8>& enrollData) const;
    
    Result SetSerializedFaceData(const std::vector<u8>& albumData,
                                 const std::vector<u8>& enrollData,
                                 std::list<Vision::LoadedKnownFace>& loadedFaces);

    Result EraseFace(Vision::FaceID_t faceID);
    void   EraseAllFaces();
    
    std::vector<Vision::LoadedKnownFace> GetEnrolledNames() const;
    
    Result RenameFace(Vision::FaceID_t faceID, const std::string& oldName, const std::string& newName,
                      Vision::RobotRenamedEnrolledFace& renamedFace);
    
    // Parameters for camera hardware exposure values
    static constexpr size_t GAMMA_CURVE_SIZE = 17;
    using GammaCurve = std::array<u8, GAMMA_CURVE_SIZE>;
    Result SetCameraExposureParams(const s32 currentExposureTime_ms,
                                   const f32 currentGain,
                                   const GammaCurve& gammaCurve);

    // When SaveImages mode is enabled, how to save them
    void SetSaveParameters(const ImageSaverParams& params);

    Vision::CameraParams GetCurrentCameraParams() const;
    Result SetNextCameraParams(const Vision::CameraParams& params);
    
    bool CheckMailbox(VisionProcessingResult& result);
    
    const RollingShutterCorrector& GetRollingShutterCorrector() { return _rollingShutterCorrector; }
    void  ShouldDoRollingShutterCorrection(bool b) { _doRollingShutterCorrection = b; }
    bool  IsDoingRollingShutterCorrection() const { return _doRollingShutterCorrection; }
    
    static f32 GetBodyTurnSpeedThresh_degPerSec();
    
    s32 GetMinCameraExposureTime_ms() const { return MIN_CAMERA_EXPOSURE_TIME_MS; }
    s32 GetMaxCameraExposureTime_ms() const { return MAX_CAMERA_EXPOSURE_TIME_MS; }
    
    f32 GetMinCameraGain() const { return MIN_CAMERA_GAIN; }
    f32 GetMaxCameraGain() const { return MAX_CAMERA_GAIN; }
    
    void ClearImageCache();

    void AddAllowedTrackedFace(const Vision::FaceID_t trackingID);
    void ClearAllowedTrackedFaces();
    
  protected:
  
    RollingShutterCorrector _rollingShutterCorrector;
    bool _doRollingShutterCorrection = false;
    RobotTimeStamp_t _lastRollingShutterCorrectionTime;
       
    std::unique_ptr<Vision::ImageCache> _imageCache;
    
    bool _isInitialized = false;
    const CozmoContext* _context = nullptr;
    
    Vision::Camera _camera;
    
    Vision::CameraParams _currentCameraParams;
    std::pair<bool,Vision::CameraParams> _nextCameraParams; // bool represents if set but not yet sent
    std::unique_ptr<Vision::CameraParamsController> _cameraParamsController;
    
    VisionModeSet _modes;
    VisionModeSet _futureModes;
    
    s32 _frameNumber = 0;
    
    // Snapshots of robot state
    bool _wasCalledOnce    = false;
    bool _havePrevPoseData = false;
    const Pose3d _poseOrigin;
    VisionPoseData _poseData, _prevPoseData;
  
    // For sending images to basestation
    ImageSendMode                 _imageSendMode = ImageSendMode::Off;
    
    // We hold a pointer to the VizManager since we often want to draw to it
    VizManager*                   _vizManager = nullptr;

    // Sub-components for detection/tracking/etc:
    std::unique_ptr<Vision::FaceTracker>            _faceTracker;
    std::unique_ptr<Vision::PetTracker>             _petTracker;
    std::unique_ptr<Vision::MarkerDetector>         _markerDetector;
    std::unique_ptr<Vision::BrightColorDetector>    _brightColorDetector;
    std::unique_ptr<LaserPointDetector>             _laserPointDetector;
    std::unique_ptr<MotionDetector>                 _motionDetector;
    std::unique_ptr<Vision::ImageCompositor>        _imageCompositor;
    std::unique_ptr<OverheadEdgesDetector>          _overheadEdgeDetector;
    std::unique_ptr<CameraCalibrator>               _cameraCalibrator;
    std::unique_ptr<OverheadMap>                    _overheadMap;
    std::unique_ptr<GroundPlaneClassifier>          _groundPlaneClassifier;
    std::unique_ptr<IlluminationDetector>           _illuminationDetector;
    std::unique_ptr<ImageSaver>                     _imageSaver;
    std::unique_ptr<MirrorModeManager>              _mirrorModeManager;
    std::unique_ptr<Vision::Benchmark>              _benchmark;
    
    std::map<std::string, std::unique_ptr<NeuralNets::NeuralNetRunner>> _neuralNetRunners;
    
    Vision::CompressedImage _compressedDisplayImg;
    s32 _imageCompressQuality = 0;
    
    Result UpdatePoseData(const VisionPoseData& newPoseData);
    Radians GetCurrentHeadAngle();
    Radians GetPreviousHeadAngle();
    
    // NOTE: CLAHE is NOT used when MarkerDetector is in LightOnDark mode
    enum class MarkerDetectionCLAHE : u8 {
      Off         = 0, // Do detection in original image only
      On          = 1, // Do detection in CLAHE image only
      Both        = 2, // Run detection twice: using original image and CLAHE image
      Alternating = 3, // Alternate using CLAHE vs. original in each successive frame
      WhenDark    = 4, // Only if mean of image is below kClaheWhenDarkThreshold
      Count
    };
    
    // Updates the rolling shutter corrector
    // Will only recompute compensation once per timestamp, so can be called multiple times
    void UpdateRollingShutter(const VisionPoseData& poseData, const Vision::ImageCache& imageCache);

    // Uses grayscale
    Result ApplyCLAHE(Vision::ImageCache& imageCache, const MarkerDetectionCLAHE useCLAHE, Vision::Image& claheImage);
    
    Result DetectMarkers(Vision::ImageCache& imageCache,
                         const Vision::Image& claheImage,
                         std::vector<Anki::Rectangle<s32>>& detectionRects,
                         MarkerDetectionCLAHE useCLAHE,
                         const VisionPoseData& poseData);
    
    // Uses grayscale
    static u8 ComputeMean(Vision::ImageCache& imageCache, const s32 sampleInc);
    
    
    // Used for UpdateCameraParams below to keep up with regions to use for metering, based on detected markers/faces
    // The TimeStamp is used to keep metering from recent detections briefly, even after we lose them
    using DetectionRectsByMode = std::map<VisionMode, std::vector<Rectangle<s32>>>;
    DetectionRectsByMode _meteringRegions;
    TimeStamp_t          _lastMeteringTimestamp_ms = 0;
    
    void UpdateMeteringRegions(TimeStamp_t t, DetectionRectsByMode&& detections);
    
    // Uses color or grayscale
    Result UpdateCameraParams(Vision::ImageCache& imageCache);
    
    // Will use color if not empty, or gray otherwise
    Result DetectLaserPoints(Vision::ImageCache& imageCache);

    // Uses grayscale
    Result DetectFaces(Vision::ImageCache& imageCache,
                       std::vector<Anki::Rectangle<s32>>& detectionRects,
                       const bool useCropping);
    
    // Uses grayscale
    Result DetectPets(Vision::ImageCache& imageCache,
                      std::vector<Anki::Rectangle<s32>>& ignoreROIs);
    
    // Will use color if not empty, or gray otherwise
    Result DetectMotion(Vision::ImageCache& imageCache);

    // Uses color
    Result DetectBrightColors(Vision::ImageCache& imageCache);

    // Uses grayscale
    Result DetectIllumination(Vision::ImageCache& imageCache);

    // Uses color
    Result UpdateOverheadMap(Vision::ImageCache& image);

    // Uses colors
    Result UpdateGroundPlaneClassifier(Vision::ImageCache& image);
    
    void CheckForNeuralNetResults();
    void AddFakeDetections(const TimeStamp_t atTimestamp, const std::set<VisionMode>& modes); // For debugging
    
    Result SaveSensorData() const;

    // Contrast-limited adaptive histogram equalization (CLAHE)
    cv::Ptr<cv::CLAHE> _clahe;
    s32 _lastClaheTileSize;
    s32 _lastClaheClipLimit;
    bool _currentUseCLAHE = true;

    // "Mailbox" for passing things out to main thread
    std::mutex _mutex;
    std::queue<VisionProcessingResult> _results;
    VisionProcessingResult _currentResult;

    // Image compositor settings, 
    // Used to manage the cycles of Reset()
    //  and MarkerDetection runs

    // Number of frames composited in order to mark the image
    //  as ready to be used in MarkerDetection.
    u32 _imageCompositorReadyPeriod = 0;

    // Number of frames composited after which the image is Reset
    // Note: if set to zero, the image is never reset
    u32 _imageCompositorResetPeriod = 0;

    // Size of images broadcasted to the Viz
    Vision::ImageCacheSize _vizImageBroadcastSize = Vision::ImageCacheSize::Half;

}; // class VisionSystem
  
} // namespace Vector
} // namespace Anki

#endif // ANKI_COZMO_BASESTATION_VISIONSYSTEM_H
