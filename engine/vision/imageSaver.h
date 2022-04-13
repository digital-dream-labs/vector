/**
 * File: imageSaver.h
 *
 * Author: Andrew Stein
 * Date:   06/07/2018
 *
 * Description: Class for saving image data according to a variety of parameters.
 *              Can optionally create thumbnails and also undistort images.
 *
 * Copyright: Anki, Inc. 2018
 **/

#ifndef __Anki_Cozmo_Engine_Vision_ImageSaver_H__
#define __Anki_Cozmo_Engine_Vision_ImageSaver_H__

#include "clad/types/imageTypes.h"
#include "clad/types/visionModes.h"
#include "coretech/vision/engine/imageCache.h"

namespace Anki {
  
namespace Vision {
  class Camera;
  class ImageCache;
  class ImageRGB;
  class Undistorter;
}
  
namespace Vector {

// Forward declaration
struct VisionProcessingResult;

struct ImageSaverParams
{
  using Mode = ImageSendMode;
  
  enum class SaveConditionType : uint8_t {
    ModeProcessed = 0,  // Save when mode was run, whether or not it found anything
    OnDetection,        // Save when mode ran and detected something
    NoDetection,        // Save when mode ran and detected nothing
  };
  
  std::string            path;  // absolute path for output images (including thumbnails)
  std::string            basename; // leave empty to use frame number
  Mode                   mode              = Mode::Off;
  int8_t                 quality           = -1; // -1 for .png, [0,100] for .jpg quality
  Vision::ImageCacheSize size              = Vision::ImageCacheSize::Half;
  float                  thumbnailScale    = 0.f; // in range [0,1], as fraction of size, 0 to disable
  float                  saveScale         = 1.f; // > 0, as fraction of size
  bool                   removeDistortion  = false;
  uint8_t                medianFilterSize  = 0; // 0 to disable
  float                  sharpeningAmount  = 0.f; // 0 to disable
  
  std::map<VisionMode, SaveConditionType> saveConditions;
  
  ImageSaverParams() = default;
  
  explicit ImageSaverParams(const std::string&     path,
                            Mode                   saveMode,
                            int8_t                 quality,
                            const std::string&     basename         = "",
                            Vision::ImageCacheSize size             = Vision::ImageCacheSize::Half,
                            float                  thumbnailScale   = 0.f,
                            float                  saveScale        = 1.f,
                            bool                   removeDistortion = false,
                            uint8_t                medianFilterSize = 0,
                            float                  sharpeningAmount = 0.f);
  
  
  static bool SaveConditionTypeFromString(const std::string& str, SaveConditionType& saveCondType);
};
  
class ImageSaver
{
public:
  
  ImageSaver();
  
  virtual ~ImageSaver();
  
  // This must be called before calling SetParams with removeDistortion=true, and before CacheUndistortionMaps
  void SetCalibration(const std::shared_ptr<Vision::CameraCalibration>& camCalib);
  
  // Pre-cache maps for undistortion, for a given image size. Will fail if SetCalibration not called yet.
  Result CacheUndistortionMaps(s32 nrows, s32 ncols);
  
  Result SetParams(const ImageSaverParams& params);
  const ImageSaverParams& GetParams() const { return _params; }
  
  // Returns true if the current mode is set such that the saver wants to save an image (SingleShot* or Stream)
  bool WantsToSave() const;
  
  // Uses specified save conditions (if any) and the given processing result to determine
  // whether to save. Will check whether detections within result match atTimestamp.
  bool WantsToSave(const VisionProcessingResult& result, const TimeStamp_t atTimestamp) const;
  
  // Returns true if the current mode is SingleShotWithSensorData or Stream
  bool ShouldSaveSensorData() const;
  
  // Return the full filename to use for saving, using the given path and frameNumber/basename, and appending
  // the given extension.
  // If no basename has been provided in params, will use frameNumber. Otherwise frameNumber is ignored.
  std::string GetFullFilename(const s32 frameNumber, const char* extension) const;
  
  // Save the specified size image from the cache and a corresponding thumbnail if requested.
  Result Save(Vision::ImageCache& imageCache, const s32 frameNumber);
  
  // Same as above, but uses specific image ("size" parameter will be ignored)
  Result Save(const Vision::ImageRGB& img, const s32 frameNumber);
  
  // Return the extension for the given quality
  static const char* GetExtension(int8_t forQuality);
  static const char* GetThumbnailExtension(int8_t forQuality);
  
private: 
  
  using Mode = ImageSaverParams::Mode;
  
  ImageSaverParams _params;
  
  std::unique_ptr<Vision::Undistorter> _undistorter;
};

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
inline const char* ImageSaver::GetExtension(int8_t forQuality)
{
  return (forQuality < 0 ? "png" : "jpg");
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
inline const char* ImageSaver::GetThumbnailExtension(int8_t forQuality)
{
  return (forQuality < 0 ? "thm.png" : "thm.jpg");
}
  
} // namespace Vector
} // namespace Anki

#endif /* __Anki_Cozmo_Engine_Vision_ImageSaver_H__ */
