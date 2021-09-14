/**
 * File: animationMessageWrapper.h
 *
 * Authors: Kevin M. Karol
 * Created: 5/31/18
 *
 * Description:
 *
 * Copyright: Anki, Inc. 2018
 *
 **/


#ifndef ANKI_COZMO_ANIMATION_MESSAGE_WRAPPER_H
#define ANKI_COZMO_ANIMATION_MESSAGE_WRAPPER_H

#include "clad/robotInterface/messageEngineToRobot.h"


namespace Anki {

namespace Vision{
class ImageRGB565;
}

namespace Vector {

class RobotAudioKeyFrame;

struct AnimationEvent;

struct AnimationMessageWrapper{
  AnimationMessageWrapper(Vision::ImageRGB565& img)
  : faceImg(img){}
  using ETR = RobotInterface::EngineToRobot;

  ETR* moveHeadMessage         = nullptr;
  ETR* moveLiftMessage         = nullptr;
  ETR* bodyMotionMessage       = nullptr;
  ETR* recHeadMessage          = nullptr;
  ETR* turnToRecHeadMessage    = nullptr;
  ETR* backpackLightsMessage   = nullptr;
  RobotAudioKeyFrame* audioKeyFrameMessage = nullptr;
  AnimationEvent* eventMessage = nullptr;

  bool haveFaceToSend = false;
  Vision::ImageRGB565& faceImg;
};


} // namespace Vector
} // namespace Anki

#endif // ANKI_COZMO_ANIMATION_MESSAGE_WRAPPER_H
