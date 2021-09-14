/**
 * File: animBackpackLightComponent.h
 *
 * Author: Al Chaussee
 * Created: 1/23/2017
 *
 * Description: Manages various lights on Vector's body. 
 *              Critical backpack lights take precedence over lights set by external (engine) sources
 *              Current priority order is Low Battery, Offline, Streaming, Charging, then lights sent by engine
 *
 * Copyright: Anki, Inc. 2017
 *
 **/

#ifndef __Anki_Cozmo_Basestation_Components_BackpackLightComponent_H__
#define __Anki_Cozmo_Basestation_Components_BackpackLightComponent_H__

#include "cozmoAnim/backpackLights/backpackLightAnimationContainer.h"
#include "cozmoAnim/backpackLights/animBackpackLightAnimation.h"
#include "cozmoAnim/backpackLights/animBackpackLightComponentTypes.h"
#include "cozmoAnim/animContext.h"

#include "json/json.h"
#include "util/cladHelpers/cladEnumToStringMap.h"
#include "util/helpers/noncopyable.h"

#include "coretech/common/shared/types.h"

#include "clad/types/ledTypes.h"
#include "clad/types/backpackAnimationTriggers.h"

#include <list>
#include <memory>
#include <future>
#include <atomic>

namespace Anki {
namespace Vector {
  
class Robot;
namespace RobotInterface {
  struct BatteryStatus;
}
 
namespace Anim {
class BackpackLightComponent : private Util::noncopyable
{
public:
  BackpackLightComponent(const AnimContext* context);

  void Init();  
  void Update();
  
  // General purpose call to set backpack lights. The light pattern will persist until this function is called again.
  // Plays the lights on the Engine priority level
  void SetBackpackAnimation(const BackpackLightAnimation::BackpackAnimation& lights);
  
  // Start the backpack lights associated with a trigger
  // Plays the lights on the Engine priority level
  void SetBackpackAnimation(const BackpackAnimationTrigger& trigger);

  // Turn the pairing light on/off
  // Pairing light is the highest priority system light
  void SetPairingLight(bool on);
  
  // Set backpack lights to indicate whether the microphone / wake word is muted
  void SetMicMute(bool muted) { _micMuted = muted; }
  
  void SetAlexaNotification(bool hasNotification) { _hasNotification = hasNotification; }
  
  // Set backpack lights to indicate whether alexa is streaming
  void SetAlexaStreaming(bool streaming) { _alexaStreaming = streaming; }

  void SetSelfTestRunning(bool running) { _selfTestRunning = running; }

  // Update battery status as we need to know when to play charging/low battery lights
  // Priority of battery related lights Low Battery > Charging > Fully Charged (Off)
  void UpdateBatteryStatus(const RobotInterface::BatteryStatus& msg);
  
private:

  // Start playing lights from the given source
  // lightLocator_out is updated to refer to the newly set lights and can be used to stop
  // the animation
  void StartBackpackAnimationInternal(const BackpackLightAnimation::BackpackAnimation& lights,
                                      BackpackLightSourceType source,
                                      BackpackLightDataLocator& lightLocator_out);

  // Stops the lights that are refered to by the locator
  bool StopBackpackAnimationInternal(const BackpackLightDataLocator& lightDataLocator);

  // Constructs and sends the actual backpack light message to the robot
  Result SendBackpackLights(const BackpackLightAnimation::BackpackAnimation& lights);
  Result SendBackpackLights(const BackpackAnimationTrigger& trigger);
  
  // Returns a sorted vector based on light source priority
  // with elements closer to the begining as having higher priorty
  static std::vector<BackpackLightSourceType> GetLightSourcePriority();

  // Returns a weak reference to the best light configuration based on priority
  BackpackLightDataRefWeak GetBestLightConfig();

  // Updates the critical backpack light config if neccessary
  void UpdateCriticalBackpackLightConfig(bool isCloudStreamOpen, bool isMicMuted, bool isNotificationPending);

  // Updates the current system light pattern if neccessary
  void UpdateSystemLightState(bool isCloudStreamOpen);

  // Updates the offline connectivity check if neccessary
  void UpdateOfflineCheck(bool force = false);

  // is there a current request for a light that's associated with an active behavior?
  bool IsBehaviorBackpackLightActive() const;

  
  /// member variables ...
  
  const AnimContext* _context = nullptr;

  // Pointer to the container of json defined backpack light animations
  std::unique_ptr<BackpackLightAnimationContainer> _backpackLightContainer;

  // Contains the mapping of backpack triggers to actual animation file names
  Util::CladEnumToStringMap<BackpackAnimationTrigger>* _backpackTriggerToNameMap = nullptr;
  
  // Contains overall mapping of light config sources to list of configurations
  BackpackLightMap _backpackLightMap;
  
  // Reference to the most recently used light configuration
  BackpackLightDataRefWeak _curBackpackLightConfig;
  
  // Locator handles for the private backpack light sources
  BackpackLightDataLocator _engineLightConfig{};
  BackpackLightDataLocator _criticalLightConfig{};

  BackpackAnimationTrigger _mostRecentTrigger = BackpackAnimationTrigger::Off;
  
  // Note: this variable does NOT track the current trigger playing, it tracks internal state for 
  // UpdateChargingLightConfig and should not be used for any other decision making
  BackpackAnimationTrigger _internalCriticalLightsTrigger = BackpackAnimationTrigger::Off;

  enum class SystemLightState
  {
    Invalid,
    Off,
    Pairing,
    Streaming,
    SelfTest,
  };
  SystemLightState _systemLightState = SystemLightState::Off;

  std::future<void> _offlineCheckFuture;
  std::atomic<TimeStamp_t> _offlineAtTime_ms; // anim timestamp

  // State for battery/charging related lights
  bool _isBatteryLow = false;
  bool _isBatteryCharging = false;
  bool _isOnChargerContacts = false;
  bool _isBatteryFull = false;
  bool _isBatteryDisconnected = false;

  // State for streaming lights
  bool _willStreamOpen = false;
  bool _isStreaming = false;
  bool _alexaStreaming = false; // separate state in case we decide to change lights
  bool _micMuted = false;
  bool _hasNotification = false;

  bool _selfTestRunning = false;

};

}
}
}

#endif

