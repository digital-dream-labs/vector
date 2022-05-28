/**
 * File: cameraService.h
 *
 * Author: chapados
 * Created: 02/07/2018
 *
 * based on androidHAL.h
 * Author: Kevin Yoon
 * Created: 02/17/2017
 *
 * Description:
 *               Defines interface to a camera system provided by the OS/platform
 *
 * Copyright: Anki, Inc. 2017
 *
 **/

#ifndef __platform_camera_camera_service_h__
#define __platform_camera_camera_service_h__

#include "coretech/common/shared/types.h"
#include "coretech/vision/engine/imageBuffer/imageBuffer.h"
#include "anki/cozmo/shared/cozmoConfig.h"
#include "clad/types/imageTypes.h"

// Forward declaration
namespace webots {
  class Supervisor;
}

namespace Anki
{
  namespace Vector
  {
    class CameraService
    {
    public:

      // Method to fetch singleton instance.
      static CameraService* getInstance();

      static bool hasInstance() { return nullptr != _instance; }

      // Removes instance
      static void removeInstance();

      // Dtor
      ~CameraService();

      // TODO: Is this necessary?
      TimeStamp_t GetTimeStamp();

      Result Update();

#ifdef SIMULATOR
      // NOTE: Only NVStorageComponent::LoadSimData() should call this function.
      //       Everyone else should be getting CameraCalibration data from NVStorageComponent!
      const CameraCalibration* GetHeadCamInfo();

      // Assign Webots supervisor
      // Webots processes must do this before creating AndroidHAL for the first time.
      // Unit test processes must call SetSupervisor(nullptr) to run without a supervisor.
      static void SetSupervisor(webots::Supervisor *sup);

#endif

// #pragma mark --- Cameras ---
      /////////////////////////////////////////////////////////////////////
      // CAMERAS
      // TODO: Add functions for adjusting ROI of cameras?
      //

      Result InitCamera();
      Result DeleteCamera();
      void PauseCamera(bool pause);

      void RegisterOnCameraRestartCallback(std::function<void()> callback);

      // Sets the camera parameters (non-blocking call)
      void CameraSetParameters(u16 exposure_ms, f32 gain);
      void CameraSetWhiteBalanceParameters(f32 r_gain, f32 g_gain, f32 b_gain);
      void CameraSetCaptureFormat(Vision::ImageEncoding format);

      void CameraSetCaptureSnapshot(bool start);
        
      // Points provided frame to a buffer of image data if available.
      // Returns true if image available.
      // Will return image data captured closest to or before atTimestamp_ms
      // If this method results in acquiring a frame, the frame is locked, ensuring
      // that the camera system will not update the buffer.
      // The caller is responsible for releasing the lock by calling CameraFrameRelease.
      bool CameraGetFrame(u32 atTimestamp_ms, Vision::ImageBuffer& buffer);

      // Releases lock on buffer for specified frameID acquired by calling CameraGetFrame.
      bool CameraReleaseFrame(u32 imageID);

      u16 CameraGetHeight() const {return _imageCaptureHeight;}
      u16 CameraGetWidth()  const {return _imageCaptureWidth; }

      // Width and height of camera sensor
      u16 CameraGetSensorHeight() const {return _imageSensorCaptureHeight;}
      u16 CameraGetSensorWidth()  const {return _imageSensorCaptureWidth; }

      bool HaveGottenFrame() const { return _imageFrameID > 1; }

    private:
      
      CameraService();

      // If needed, will temporarily unpause the camera in order to
      // for camera settings to be applied
      void UnpauseForCameraSetting();
      
      static CameraService* _instance;

#ifdef SIMULATOR
      CameraCalibration headCamInfo_;
#else

      // Time
      std::chrono::steady_clock::time_point _timeOffset;
#endif

      // Camera
      u16             _imageCaptureHeight = DEFAULT_CAMERA_RESOLUTION_HEIGHT;
      u16             _imageCaptureWidth  = DEFAULT_CAMERA_RESOLUTION_WIDTH;
      u16             _imageSensorCaptureHeight = DEFAULT_CAMERA_RESOLUTION_HEIGHT;
      u16             _imageSensorCaptureWidth  = DEFAULT_CAMERA_RESOLUTION_WIDTH;
      u32             _imageFrameID = 0;


    }; // class CameraService

  } // namespace Vector
} // namespace Anki

#endif // __platform_camera_camera_service_h__
