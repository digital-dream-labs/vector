/**
 * File: faceDisplay.h
 *
 * Author: Kevin Yoon
 * Created: 07/20/2017
 *
 * Description:
 *               Defines interface to face display
 *
 * Copyright: Anki, Inc. 2017
 *
 **/

#ifndef ANKI_COZMOANIM_FACE_DISPLAY_H
#define ANKI_COZMOANIM_FACE_DISPLAY_H

#include "util/singleton/dynamicSingleton.h"
#include "anki/cozmo/shared/factory/faultCodes.h"

#include "clad/types/lcdTypes.h"

#include <thread>

namespace Anki {

namespace Vision {
  class ImageRGB565;
}

namespace Vector {

class FaceDisplayImpl;
class FaceInfoScreenManager;

class FaceDisplay : public Util::DynamicSingleton<FaceDisplay>
{
  ANKIUTIL_FRIEND_SINGLETON(FaceDisplay); // Allows base class singleton access

public:
  void DrawToFace(const Vision::ImageRGB565& img);

  // For drawing to face in various debug modes
  void DrawToFaceDebug(const Vision::ImageRGB565& img);


  void SetFaceBrightness(LCDBrightness level);

  // Stops the boot animation process if it is running
  void StopBootAnim();
  
protected:
  FaceDisplay();
  virtual ~FaceDisplay();

  void DrawToFaceInternal(const Vision::ImageRGB565& img);

private:
  std::unique_ptr<FaceDisplayImpl>  _displayImpl;

  // Members for managing the drawing thread
  std::unique_ptr<Vision::ImageRGB565>  _faceDrawImg[2];
  Vision::ImageRGB565*                  _faceDrawNextImg = nullptr;
  Vision::ImageRGB565*                  _faceDrawCurImg = nullptr;
  std::thread                           _faceDrawThread;
  std::mutex                            _faceDrawMutex;
  std::atomic<bool>                     _stopDrawFace;

  std::mutex                            _readyMutex;
  std::condition_variable               _readyCondition;
  bool                                  _readyFace;

  // Whether or not the boot animation process has been stopped
  // Atomic because it is checked by the face drawing thread
  std::atomic<bool> _stopBootAnim;
  
  void DrawFaceLoop();
  void UpdateNextImgPtr();
}; // class FaceDisplay

} // namespace Vector
} // namespace Anki

#endif // ANKI_COZMOANIM_FACE_DISPLAY_H
