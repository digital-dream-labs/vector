/*
 * File: RobotToEngineImplMessaging.h
 * Author: Meith Jhaveri
 * Created: 7/11/16
 * Description: System for handling Robot to Engine messages.
 * Copyright: Anki, inc. 2016
 */

#ifndef __Anki_Cozmo_Basestation_RobotToEngineImplMessaging_H__
#define __Anki_Cozmo_Basestation_RobotToEngineImplMessaging_H__

#include "util/entityComponent/iDependencyManagedComponent.h"
#include "engine/components/visionScheduleMediator/iVisionModeSubscriber.h"
#include "engine/robotComponents_fwd.h"
#include "engine/robotInterface/messageHandler.h"
#include "coretech/vision/engine/image.h"
#include "clad/robotInterface/messageRobotToEngine_hash.h"
#include "util/helpers/noncopyable.h"
#include "util/signals/signalHolder.h"

#include <fstream>
#include <memory.h>

namespace Anki {
namespace Vector {

class Robot;
  
class RobotToEngineImplMessaging : public IDependencyManagedComponent<RobotComponentID>,
                                   private Util::noncopyable,
                                   public Util::SignalHolder
{
public:
  RobotToEngineImplMessaging();
  ~RobotToEngineImplMessaging();

  //////
  // IDependencyManagedComponent functions
  //////
  virtual void InitDependent(Vector::Robot* robot, const RobotCompMap& dependentComps) override {};
  virtual void GetInitDependencies(RobotCompIDSet& dependencies) const override {};
  virtual void GetUpdateDependencies(RobotCompIDSet& dependencies) const override {};
  //////
  // end IDependencyManagedComponent functions
  //////
  
  void InitRobotMessageComponent(RobotInterface::MessageHandler* messageHandler, Robot* const robot);
  void HandlePrint(const AnkiEvent<RobotInterface::RobotToEngine>& message, Robot* const robot);
  void HandlePickAndPlaceResult(const AnkiEvent<RobotInterface::RobotToEngine>& message, Robot* const robot);
  void HandleDockingStatus(const AnkiEvent<RobotInterface::RobotToEngine>& message);
  void HandleFallingEvent(const AnkiEvent<RobotInterface::RobotToEngine>& message, Robot* const robot);
  void HandleFallImpactEvent(const AnkiEvent<RobotInterface::RobotToEngine>& message, Robot* const robot);
  void HandleGoalPose(const AnkiEvent<RobotInterface::RobotToEngine>& message, Robot* const robot);
  void HandleRobotStopped(const AnkiEvent<RobotInterface::RobotToEngine>& message, Robot* const robot);
  void HandleCliffEvent(const AnkiEvent<RobotInterface::RobotToEngine>& message, Robot* const robot);
  void HandlePotentialCliffEvent(const AnkiEvent<RobotInterface::RobotToEngine>& message, Robot* const robot);
  void HandleRobotPoked(const AnkiEvent<RobotInterface::RobotToEngine>& message, Robot* const robot);
  
  // For processing imu data chunks arriving from robot.
  // Writes the entire log of 3-axis accelerometer and 3-axis
  // gyro readings to a .m file in kP_IMU_LOGS_DIR so they
  // can be read in from Matlab. (See robot/util/imuLogsTool.m)
  void HandleImuData(const AnkiEvent<RobotInterface::RobotToEngine>& message, Robot* const robot);
  void HandleImuRawData(const AnkiEvent<RobotInterface::RobotToEngine>& message, Robot* const robot);
  void HandleSyncRobotAck(const AnkiEvent<RobotInterface::RobotToEngine>& message, Robot* const robot);
  void HandleMotorCalibration(const AnkiEvent<RobotInterface::RobotToEngine>& message, Robot* const robot);
  void HandleMotorAutoEnabled(const AnkiEvent<RobotInterface::RobotToEngine>& message, Robot* const robot);
  void HandleAudioInput(const AnkiEvent<RobotInterface::RobotToEngine>& message, Robot* const robot);
  void HandleMicDirection(const AnkiEvent<RobotInterface::RobotToEngine>& message, Robot* const robot);
  void HandleMicDataState(const AnkiEvent<RobotInterface::RobotToEngine>& message, Robot* const robot);
  void HandleStreamCameraImages(const AnkiEvent<RobotInterface::RobotToEngine>& message, Robot* const robot);  
  void HandleDisplayedFaceImage(const AnkiEvent<RobotInterface::RobotToEngine>& message, Robot* const robot);

private:

  ///////// Messaging ////////
  // These methods actually do the creation of messages and sending
  // (via MessageHandler) to the physical robot

  uint32_t _imuSeqID = 0;
  std::ofstream _imuLogFileStream;
  
  // For tracking time since last power level report (per accessory)
  std::map<uint32_t, uint32_t> _lastPowerLevelSentTime;
  std::map<uint32_t, uint32_t> _lastMissedPacketCount;

  // For tracking face image data sent back from robot
  Vision::ImageRGB565 _faceImageRGB565;
  u32                 _faceImageRGBId                    = 0;          // Used only for tracking chunks of the same image as they are received
  u32                 _faceImageRGBChunksReceivedBitMask = 0;
  const u32           kAllFaceImageRGBChunksReceivedMask = 0x3fffffff; // 30 bits for 30 expected chunks 

};

} // namespace Vector
} // namespace Anki
#endif /* RobotToEngineImplMessaging_h */
