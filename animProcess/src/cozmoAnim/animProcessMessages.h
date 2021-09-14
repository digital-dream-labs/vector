/**
 * File: animProcessMessages.h
 *
 * Author: Kevin Yoon
 * Created: 6/30/2017
 *
 * Description: Shuttles messages between engine and robot processes.
 *              Responds to engine messages pertaining to animations
 *              and inserts messages as appropriate into robot-bound stream.
 *
 * Copyright: Anki, Inc. 2017
 **/

#ifndef ANIM_PROCESS_MESSAGES_H
#define ANIM_PROCESS_MESSAGES_H

#include "coretech/common/shared/types.h"

#include "clad/robotInterface/messageEngineToRobot.h"
#include "clad/robotInterface/messageRobotToEngine.h"

namespace Anki {
namespace Vector {

// Forward declarations
namespace Anim {
  class AnimationStreamer;
  class StreamingAnimationModifier;
  class AnimContext;
  class AnimEngine;
}

namespace Audio {
class EngineRobotAudioInput;
}

class AnimProcessMessages
{
public:

  // Initialize message handlers.
  // Arguments may not be null.
  static Result Init(Anim::AnimEngine* animEngine,
                     Anim::AnimationStreamer* animStreamer,
                     Anim::StreamingAnimationModifier* streamingAnimationModifier,
                     Audio::EngineRobotAudioInput* audioInput,
                     const Anim::AnimContext* context);

  // Process message traffic
  static Result Update(BaseStationTime_t currTime_nanosec);

  // Send message to engine
  // Returns true on success, false on error
  static bool SendAnimToEngine(const RobotInterface::RobotToEngine& msg);

  // Send message to robot
  // Returns true on success, false on error
  static bool SendAnimToRobot(const RobotInterface::EngineToRobot& msg);

  // Dispatch message from engine
  static void ProcessMessageFromEngine(const RobotInterface::EngineToRobot& msg);

  // Dispatch message from robot
  static void ProcessMessageFromRobot(const RobotInterface::RobotToEngine& msg);

  static uint32_t GetMessageCountAtR() { return _messageCountAnimToRobot; }
  static uint32_t GetMessageCountAtE() { return _messageCountAnimToEngine; }
  static uint32_t GetMessageCountRtA() { return _messageCountRobotToAnim; }
  static uint32_t GetMessageCountEtA() { return _messageCountEngineToAnim; }

private:
  // Check state & send firmware handshake when engine connects
  static Result MonitorConnectionState(BaseStationTime_t currTime_nanosec);

  static uint32_t _messageCountAnimToRobot;
  static uint32_t _messageCountAnimToEngine;
  static uint32_t _messageCountRobotToAnim;
  static uint32_t _messageCountEngineToAnim;
};

} // namespace Vector
} // namespace Anki


#endif  // #ifndef ANIM_PROCESS_MESSAGES_H
