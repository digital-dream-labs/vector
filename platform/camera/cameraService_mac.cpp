/**
 * File: cameraService_mac.cpp
 *
 * Author: chapados
 * Created: 02/07/2018
 *
 * based on androidHAL_android.cpp
 * Author: Kevin Yoon
 * Created: 02/17/2017
 *
 * Description:
 *               Defines interface to a camera system provided by the OS/platform
 *
 * Copyright: Anki, Inc. 2017
 *
 **/

#include "camera/cameraService.h"

#include "coretech/common/shared/array2d.h"

#include "simulator/controllers/shared/webotsHelpers.h"

#include "util/logging/logging.h"

#include "util/container/fixedCircularBuffer.h"
#include "util/random/randomGenerator.h"

#include <vector>

#include <webots/Supervisor.hpp>
#include <webots/Camera.hpp>

#define BLUR_CAPTURED_IMAGES 0

#include "opencv2/imgproc/imgproc.hpp"

#ifndef SIMULATOR
#error SIMULATOR should be defined by any target using cameraService_mac.cpp
#endif

namespace Anki {
  namespace Vector {

    namespace { // "Private members"

      // Has SetSupervisor() been called yet?
      bool _engineSupervisorSet = false;

      // Current supervisor (if any)
      webots::Supervisor* _engineSupervisor = nullptr;

      // Const parameters / settings
      const u32 VISION_TIME_STEP = 65; // This should be a multiple of the world's basic time step!

      // Cameras / Vision Processing
      webots::Camera* headCam_;

      // Lens distortion
      const bool kUseLensDistortion = false;
      const f32 kRadialDistCoeff1     = -0.07178328295562293f;
      const f32 kRadialDistCoeff2     = -0.2195788148163958f;
      const f32 kRadialDistCoeff3     = 0.13393879360293f;
      const f32 kTangentialDistCoeff1 = 0.001433240008548796f;
      const f32 kTangentialDistCoeff2 = 0.001523473592445885f;
      const f32 kDistCoeffNoiseFrac   = 0.0f; // fraction of the true value to use for uniformly distributed noise (0 to disable)

      // This buffers Webots camera images from the recent past, so that engine can request an image from a specific
      // timestamp (in the past). The buffer contains pairs, where the first element is timestamp of image capture, and
      // the second element is the RGB image itself.
      constexpr size_t nBufferEntries = 3; // 3 images = 195 ms
      Util::FixedCircularBuffer<std::pair<TimeStamp_t, std::vector<u8>>, nBufferEntries> webotsImageBuffer_;
      
      std::vector<u8> imageBuffer_;
      Vision::ImageRGB rgb_; // Wrapper around imageBuffer_
      TimeStamp_t cameraStartTime_ms_;
      TimeStamp_t lastImageCapturedTime_ms_;

      bool _skipNextImage = false;
    } // "private" namespace


#pragma mark --- Simulated Hardware Method Implementations ---

    // Declarations
    void FillCameraInfo(const webots::Camera *camera, CameraCalibration &info);

    // Apply lens distortion to the RGB image in frame, using the information from headCamInfo
    void ApplyLensDistortion(u8* frame, const CameraCalibration& headCamInfo);

    // Definition of static field
    CameraService* CameraService::_instance = nullptr;

    /**
     * Returns the single instance of the object.
     */
    CameraService* CameraService::getInstance() {
      // Did you remember to call SetSupervisor()?
      DEV_ASSERT(_engineSupervisorSet, "cameraService_mac.NoSupervisorSet");
      // check if the instance has been created yet
      if (nullptr == _instance) {
        // if not, then create it
        _instance = new CameraService();
      }
      // return the single instance
      return _instance;
    }

    /**
     * Removes instance
     */
    void CameraService::removeInstance() {
      // check if the instance has been created yet
      if (nullptr != _instance) {
        delete _instance;
        _instance = nullptr;
      }
    };

    void CameraService::SetSupervisor(webots::Supervisor *sup)
    {
      _engineSupervisor = sup;
      _engineSupervisorSet = true;
    }

    CameraService::CameraService()
    : _imageSensorCaptureHeight(DEFAULT_CAMERA_RESOLUTION_HEIGHT)
    , _imageSensorCaptureWidth(DEFAULT_CAMERA_RESOLUTION_WIDTH)
    {
      imageBuffer_.resize(CAMERA_SENSOR_RESOLUTION_WIDTH * CAMERA_SENSOR_RESOLUTION_HEIGHT * 3);
            
      if (nullptr != _engineSupervisor) {

        // Is the step time defined in the world file >= than the robot time? It should be!
        DEV_ASSERT(ROBOT_TIME_STEP_MS >= _engineSupervisor->getBasicTimeStep(), "cameraService_mac.UnexpectedTimeStep");

        if (VISION_TIME_STEP % static_cast<u32>(_engineSupervisor->getBasicTimeStep()) != 0) {
          PRINT_NAMED_WARNING("cameraService_mac.InvalidVisionTimeStep",
                              "VISION_TIME_STEP (%d) must be a multiple of the world's basic timestep (%.0f).",
                              VISION_TIME_STEP, _engineSupervisor->getBasicTimeStep());
          return;
        }

        // Head Camera
        headCam_ = _engineSupervisor->getCamera("HeadCamera");
        if (nullptr != headCam_) {
          headCam_->enable(VISION_TIME_STEP);
          FillCameraInfo(headCam_, headCamInfo_);

          // HACK: Figure out when first camera image will actually be taken (next
          // timestep from now), so we can reference to it when computing frame
          // capture time from now on.
          // TODO: Not sure from Cyberbotics support message whether this should include "+ VISION_TIME_STEP" or not...
          cameraStartTime_ms_ = GetTimeStamp(); // + VISION_TIME_STEP;
          lastImageCapturedTime_ms_ = 0;
          
          // Make the CozmoVizDisplay (which includes the nav map, etc.) invisible to the camera. Note that the call to
          // setVisibility() requires a pointer to the camera NODE, _not_ the camera device (which is of type
          // webots::Camera*). There seems to be no good way to get the underlying node pointer of the camera, so we
          // have to do this somewhat hacky iteration over all of the nodes in the world to find the camera node.
          const auto& vizNodes = WebotsHelpers::GetMatchingSceneTreeNodes(*_engineSupervisor, "CozmoVizDisplay");
          
          webots::Node* cameraNode = nullptr;
          const int maxNodesToSearch = 10000;
          for (int i=0 ; i < maxNodesToSearch ; i++) {
            auto* node = _engineSupervisor->getFromId(i);
            if ((node != nullptr) && (node->getTypeName() == "CozmoCamera")) {
              cameraNode = node;
              break;
            }
          }
          DEV_ASSERT(cameraNode != nullptr, "CameraService.NoWebotsCameraFound");
          
          for (const auto& vizNode : vizNodes) {
            vizNode.nodePtr->setVisibility(cameraNode, false);
          }
        }
      }
    }

    CameraService::~CameraService()
    {

    }

    void CameraService::RegisterOnCameraRestartCallback(std::function<void()> callback)
    {
      return;
    }
    
    TimeStamp_t CameraService::GetTimeStamp(void)
    {
      if (nullptr != _engineSupervisor) {
        return static_cast<TimeStamp_t>(_engineSupervisor->getTime() * 1000.0);
      }
      return 0;
    }

    Result CameraService::Update()
    {
      return RESULT_OK;
    }


    // Helper function to create a CameraInfo struct from Webots camera properties:
    void FillCameraInfo(const webots::Camera *camera, CameraCalibration &info)
    {

      const u16 nrows  = static_cast<u16>(camera->getHeight());
      const u16 ncols  = static_cast<u16>(camera->getWidth());
      const f32 width  = static_cast<f32>(ncols);
      const f32 height = static_cast<f32>(nrows);
      //f32 aspect = width/height;

      const f32 fov_hor = camera->getFov();

      // Compute focal length from simulated camera's reported FOV:
      const f32 f = width / (2.f * std::tan(0.5f*fov_hor));

      // There should only be ONE focal length, because simulated pixels are
      // square, so no need to compute/define a separate fy
      //f32 fy = height / (2.f * std::tan(0.5f*fov_ver));

      info.focalLength_x = f;
      info.focalLength_y = f;
      info.center_x      = 0.5f*(width-1);
      info.center_y      = 0.5f*(height-1);
      info.skew          = 0.f;
      info.nrows         = nrows;
      info.ncols         = ncols;
      info.distCoeffs.fill(0.f);

      if(kUseLensDistortion)
      {
        info.distCoeffs[0] = kRadialDistCoeff1;
        info.distCoeffs[1] = kRadialDistCoeff2;
        info.distCoeffs[2] = kTangentialDistCoeff1;
        info.distCoeffs[3] = kTangentialDistCoeff2;
        info.distCoeffs[4] = kRadialDistCoeff3;

        if(Util::IsFltGTZero(kDistCoeffNoiseFrac))
        {
          // Simulate not having perfectly calibrated distortion coefficients
          static Util::RandomGenerator rng(0);
          for(s32 i=0; i<5; ++i) {
            info.distCoeffs[i] *= rng.RandDblInRange(1.f-kDistCoeffNoiseFrac, 1.f+kDistCoeffNoiseFrac);
          }
        }
      }
    } // FillCameraInfo
    
    void ApplyLensDistortion(u8* frame, const CameraCalibration& headCamInfo)
    {
      // Apply radial/lens distortion. Note that cv::remap uses in inverse lookup to find where the pixels in
      // the output (distorted) image came from in the source. So we have to compute the inverse distortion here.
      // We do that using cv::undistortPoints to create the necessary x/y maps for remap:
      static cv::Mat_<f32> x_undistorted, y_undistorted;
      if(x_undistorted.empty())
      {
        // Compute distortion maps on first use
        std::vector<cv::Point2f> points;
        points.reserve(headCamInfo.nrows * headCamInfo.ncols);
        
        for (s32 i=0; i < headCamInfo.nrows; i++) {
          for (s32 j=0; j < headCamInfo.ncols; j++) {
            points.emplace_back(j,i);
          }
        }
        
        const std::vector<f32> distCoeffs{
          kRadialDistCoeff1, kRadialDistCoeff2, kTangentialDistCoeff1, kTangentialDistCoeff2, kRadialDistCoeff3
        };
        const cv::Matx<f32,3,3> cameraMatrix(headCamInfo.focalLength_x, 0.f, headCamInfo.center_x,
                                             0.f, headCamInfo.focalLength_y, headCamInfo.center_y,
                                             0.f, 0.f, 1.f);
        
        cv::undistortPoints(points, points, cameraMatrix, distCoeffs, cv::noArray(), cameraMatrix);
        
        x_undistorted.create(headCamInfo.nrows, headCamInfo.ncols);
        y_undistorted.create(headCamInfo.nrows, headCamInfo.ncols);
        std::vector<cv::Point2f>::const_iterator pointIter = points.begin();
        for (s32 i=0; i < headCamInfo.nrows; i++)
        {
          f32* x_i = x_undistorted.ptr<f32>(i);
          f32* y_i = y_undistorted.ptr<f32>(i);
          
          for (s32 j=0; j < headCamInfo.ncols; j++)
          {
            x_i[j] = pointIter->x;
            y_i[j] = pointIter->y;
            ++pointIter;
          }
        }
      }
      cv::Mat  cvFrame(headCamInfo.nrows, headCamInfo.ncols, CV_8UC3, frame);
      cv::remap(cvFrame, cvFrame, x_undistorted, y_undistorted, CV_INTER_LINEAR);
    }

    const CameraCalibration* CameraService::GetHeadCamInfo(void)
    {
      return &headCamInfo_;
    }

    void CameraService::CameraSetParameters(u16 exposure_ms, f32 gain)
    {
      // Can't control simulated camera's exposure.

      // TODO: Simulate this somehow?

      return;

    } // HAL::CameraSetParameters()

    void CameraService::CameraSetWhiteBalanceParameters(f32 r_gain, f32 g_gain, f32 b_gain)
    {
      return;
    }

    void CameraService::CameraSetCaptureFormat(Vision::ImageEncoding format)
    {
      return;
    }

    void CameraService::CameraSetCaptureSnapshot(bool start)
    {
      return;
    }

    Result CameraService::InitCamera()
    {
      return RESULT_OK;
    }

    Result CameraService::DeleteCamera()
    {
      return RESULT_OK;
    }

    void CameraService::UnpauseForCameraSetting()
    {
      return;
    }
    
    void CameraService::PauseCamera(bool pause)
    {
      if(pause)
      {
        headCam_->disable();
      }
      else
      {
        headCam_->enable(VISION_TIME_STEP);
      }
      
      // Technically only need to skip the next image when unpausing but since
      // you can't get images while paused it does not matter that this is being set
      // when pausing
      _skipNextImage = true;
    }

    // Starts camera frame synchronization.
    // Returns true and popuates buffer if we have an available image from at or before atTimestamp_ms.
    bool CameraService::CameraGetFrame(u32 atTimestamp_ms, Vision::ImageBuffer& buffer)
    {
      if (nullptr == headCam_) {
        return false;
      }

      if (_skipNextImage) {
        _skipNextImage = false;
        return false;
      }
      
      const TimeStamp_t currentTime_ms = GetTimeStamp();

      // This computation is based on Cyberbotics support's explanation for how to compute
      // the actual capture time of the current available image from the simulated camera
      const TimeStamp_t currentImageTime_ms = (std::floor((currentTime_ms-cameraStartTime_ms_)/VISION_TIME_STEP) * VISION_TIME_STEP
                                               + cameraStartTime_ms_);

      // Do we have a 'new' image from webots?
      if(lastImageCapturedTime_ms_ != currentImageTime_ms)
      {
        // A 'new' image is available. Push the current webots image into the buffer of available webots images
        auto& thisImage = webotsImageBuffer_.push_back();
        thisImage.first = currentImageTime_ms;
        auto& imageVec = thisImage.second;
        imageVec.resize(CAMERA_SENSOR_RESOLUTION_WIDTH * CAMERA_SENSOR_RESOLUTION_HEIGHT * 3);
        
        const u8* image = headCam_->getImage();
        DEV_ASSERT(image != NULL, "cameraService_mac.CameraGetFrame.NullImagePointer");
        DEV_ASSERT_MSG(headCam_->getWidth() == headCamInfo_.ncols,
                       "cameraService_mac.CameraGetFrame.MismatchedImageWidths",
                       "HeadCamInfo:%d HeadCamWidth:%d", headCamInfo_.ncols, headCam_->getWidth());
        
        // Copy from the webots 'image' into imageVec, converting from BGRA to RGB along the way
        u8* frame = imageVec.data();
        u8* pixel = frame;
        for (s32 i=0 ; i < headCamInfo_.nrows * headCamInfo_.ncols; i++) {
          pixel[2] = *image++; // blue
          pixel[1] = *image++; // green
          pixel[0] = *image++; // red
          ++image;             // don't need alpha channel, so skip it
          pixel+=3;
        }
        
        if (kUseLensDistortion)
        {
          ApplyLensDistortion(frame, headCamInfo_);
        }

        if (BLUR_CAPTURED_IMAGES)
        {
          // Add some blur to simulated images
          cv::Mat cvImg(headCamInfo_.nrows, headCamInfo_.ncols, CV_8UC3, frame);
          cv::GaussianBlur(cvImg, cvImg, cv::Size(0,0), 0.75f);
        }
        
        // Mark that we've buffered this image for the current time
        lastImageCapturedTime_ms_ = currentImageTime_ms;
      }

      if (webotsImageBuffer_.empty()) {
        // no image available
        return false;
      }
      
      const auto earliestImageTimestamp = webotsImageBuffer_.front().first;
      if ((atTimestamp_ms != 0) &&
          (atTimestamp_ms < earliestImageTimestamp)) {
        return false;
      }
      
      u32 outputTimestamp = 0;
      // If atTimestamp_ms is zero, this indicates that the caller simply wants the latest available image
      if (atTimestamp_ms == 0) {
        if (!webotsImageBuffer_.empty()) {
          outputTimestamp = webotsImageBuffer_.back().first;
          imageBuffer_.swap(webotsImageBuffer_.back().second);
          // Clear the buffer to prevent the same image from being used twice
          webotsImageBuffer_.clear();
        }
      } else {
        // Find the image in the webots image buffer that is before or equal to atTimestamp_ms, popping older images
        // from the buffer along the way.
        while (!webotsImageBuffer_.empty() &&
               webotsImageBuffer_.front().first <= atTimestamp_ms) {
          outputTimestamp = webotsImageBuffer_.front().first;
          imageBuffer_.swap(webotsImageBuffer_.front().second);
          webotsImageBuffer_.pop_front();
        }
      }

      // Wrap imageBuffer_ in an ImageRGB so we can easily resize it
      // On physical robot images are captured at 1280x720, on simulated robot
      // images are captured at 640x360 so we need to scale the image by 2
      // to make it match the physical robot
      rgb_ = Vision::ImageRGB(headCamInfo_.nrows,
                              headCamInfo_.ncols,
                              imageBuffer_.data());

      rgb_.Resize(2.f, Vision::ResizeMethod::NearestNeighbor);

      buffer = Vision::ImageBuffer(const_cast<u8*>(reinterpret_cast<const u8*>(rgb_.GetDataPointer())),
                                   CAMERA_SENSOR_RESOLUTION_HEIGHT,
                                   CAMERA_SENSOR_RESOLUTION_WIDTH,
                                   Vision::ImageEncoding::RawRGB,
                                   outputTimestamp,
                                   _imageFrameID);

      _imageFrameID++;
            
      return true;

    } // CameraGetFrame()

    bool CameraService::CameraReleaseFrame(u32 imageID)
    {
      // no-op
      return true;
    }
  } // namespace Vector
} // namespace Anki
