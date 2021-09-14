/**
 * File: connectionFlow.h
 *
 * Author: Al Chaussee
 * Created: 02/28/2018
 *
 * Description: Functions for updating what to display on the face
 *              during various parts of the connection flow
 *
 * Copyright: Anki, Inc. 2018
 **/

#ifndef ANIM_CONNECTION_FLOW_H
#define ANIM_CONNECTION_FLOW_H

#include <string>

namespace Anki {
namespace Vector {
namespace Anim {
  class AnimContext;
  class AnimationStreamer;
}

namespace SwitchboardInterface {
  struct SetConnectionStatus;
}

void SetBLEPin(uint32_t pin);

bool InitConnectionFlow(Anim::AnimationStreamer* animStreamer);

void UpdateConnectionFlow(const SwitchboardInterface::SetConnectionStatus& msg,
                          Anim::AnimationStreamer* animStreamer,
                          const Anim::AnimContext* context);

}
}

#endif
