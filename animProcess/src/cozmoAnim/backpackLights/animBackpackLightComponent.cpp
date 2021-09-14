/**
 * File: animBackpackLightComponent.cpp
 *
 * Author: Al Chaussee
 * Created: 1/23/2017
 *
 * Description: Manages various lights on Vector's body.
 *              Currently this includes the backpack lights.
 *
 * Copyright: Anki, Inc. 2017
 *
 **/

#include "cozmoAnim/backpackLights/animBackpackLightComponent.h"

#include "coretech/common/engine/utils/data/dataPlatform.h"
#include "coretech/common/engine/utils/timer.h"
#include "cozmoAnim/animTimeStamp.h"
#include "cozmoAnim/animComms.h"
#include "cozmoAnim/micData/micDataSystem.h"
#include "cozmoAnim/robotDataLoader.h"
#include "clad/robotInterface/messageEngineToRobot.h"
#include "util/console/consoleInterface.h"
#include "util/fileUtils/fileUtils.h"
#include "util/internetUtils/internetUtils.h"

#include "osState/osState.h"

#define DEBUG_LIGHTS 0

namespace Anki {
namespace Vector {
namespace Anim {

CONSOLE_VAR(u32, kOfflineTimeBeforeLights_ms, "Backpacklights", (1000*60*2));
CONSOLE_VAR(u32, kOfflineCheckFreq_ms,        "Backpacklights", 5000);
  
enum class BackpackLightSourcePrivate : BackpackLightSourceType
{
  Engine = Util::EnumToUnderlying(BackpackLightSource::Count),
  Critical,
  
  Count
};

struct BackpackLightData
{
  BackpackLightAnimation::BackpackAnimation _lightConfiguration;
};
  
BackpackLightComponent::BackpackLightComponent(const AnimContext* context)
: _context(context)
, _offlineAtTime_ms(0)
{  
  static_assert((int)LEDId::NUM_BACKPACK_LEDS == 3, "BackpackLightComponent.WrongNumBackpackLights");

  // Add callbacks so we know when trigger word/audio stream are updated
  _context->GetMicDataSystem()->AddTriggerWordDetectedCallback([this](bool willStream)
    {
      _willStreamOpen = willStream;

      UpdateOfflineCheck(true);
      
      // If we are offline then trigger the offline lights
      // immediately upon trigger word detected
      if(_offlineAtTime_ms > 0)
      {
        _offlineAtTime_ms = 1;
      }
    });
  
  _context->GetMicDataSystem()->AddStreamUpdatedCallback([this](bool streamStart)
    {
      _isStreaming = streamStart;
      _willStreamOpen = false;
    });
}


void BackpackLightComponent::Init()
{
  _backpackLightContainer = std::make_unique<BackpackLightAnimationContainer>(
    _context->GetDataLoader()->GetBackpackLightAnimations());

  _backpackTriggerToNameMap = _context->GetDataLoader()->GetBackpackAnimationTriggerMap();
}


void BackpackLightComponent::UpdateCriticalBackpackLightConfig(bool isCloudStreamOpen, bool isMicMuted, bool isNotificationPending)
{
  const AnimTimeStamp_t curTime_ms = BaseStationTimer::getInstance()->GetCurrentTimeStamp();
 
  // Check which, if any, backpack lights should be displayed
  // Streaming, Low Battery, Offline, Charging, or Nothing
  BackpackAnimationTrigger trigger = BackpackAnimationTrigger::Off;

  // If we are currently streaming to the cloud
  if(isCloudStreamOpen)
  {
    trigger = BackpackAnimationTrigger::Streaming;
  }
  else if( _isBatteryLow && !_isOnChargerContacts )
  {
    // we use _isOnChargerContacts as a proxy for the only case where
    // we need to show the low battery lights, since we can only be
    // off the charger contacts if we are !charging and !disconnected
    // (and still be turned on)
    //
    // charging | disconnected | show low battery lights?
    // Y        | Y            | N (faking charging bc disconnected)
    // Y        | N            | N (actually charging)
    // N        | Y            | N (happens after charging for too long (>25min))
    // N        | N            | Y (no charging taking place)
    trigger = BackpackAnimationTrigger::LowBattery;
  }
  else if(_selfTestRunning)
  {
    trigger = BackpackAnimationTrigger::Off;
  }
  // If we have been offline for long enough
  else if(_offlineAtTime_ms > 0 &&
          ((TimeStamp_t)curTime_ms - _offlineAtTime_ms > kOfflineTimeBeforeLights_ms))
  {
    trigger = BackpackAnimationTrigger::Offline; 
  }
  else if(isMicMuted)
  {
    trigger = BackpackAnimationTrigger::Muted;
  }
  else if ( IsBehaviorBackpackLightActive() )
  {
    // if the engine is playing a "behavior light", then we want to slide that priority in right here
    // turn off the critical lights since the engine light will take priority over everything after this point
    // once it stops, critical lights will be re-started
    trigger = BackpackAnimationTrigger::Off;
  }
  else if(isNotificationPending)
  {
    trigger = BackpackAnimationTrigger::AlexaNotification;
  }
  // If we are on the charger and charging
  else if(_isOnChargerContacts &&
          _isBatteryCharging &&
          !_isBatteryFull &&
          !_isBatteryDisconnected)
  {
    trigger = BackpackAnimationTrigger::Charging;
  }

  if(trigger != _internalCriticalLightsTrigger)
  {
    _internalCriticalLightsTrigger = trigger;
    auto animName = _backpackTriggerToNameMap->GetValue(trigger);
    const auto* anim = _backpackLightContainer->GetAnimation(animName);
    if(anim == nullptr)
    {
      PRINT_NAMED_WARNING("BackpackLightComponent.UpdateChargingLightConfig.NullAnim",
                          "Got null anim for trigger %s",
                          EnumToString(trigger));
      return;
    }

    PRINT_CH_INFO("BackpackLightComponent",
                  "BackpackLightComponent.UpdateCriticalLightConfig",
                  "%s", EnumToString(trigger));
          
    // All of the backpack lights set by the above checks (except for Off)
    // take precedence over all other backpack lights so play them
    // under the "critical" backpack light source
    if(trigger != BackpackAnimationTrigger::Off)
    {
      StartBackpackAnimationInternal(*anim,
                                     Util::EnumToUnderlying(BackpackLightSourcePrivate::Critical),
                                     _criticalLightConfig);
    }
    else
    {
      StopBackpackAnimationInternal(_criticalLightConfig);
    }
  }
}


void BackpackLightComponent::Update()
{
  UpdateOfflineCheck();

  // Consider stream to be open when the trigger word is detected or we are actually
  // streaming. Trigger word stays detected until the stream state is updated
  const bool isCloudStreamOpen = (_willStreamOpen || _isStreaming || _alexaStreaming);
  UpdateCriticalBackpackLightConfig(isCloudStreamOpen, _micMuted, _hasNotification);

  UpdateSystemLightState(isCloudStreamOpen);
  
  BackpackLightDataRefWeak bestNewConfig = GetBestLightConfig();
  
  auto newConfig = bestNewConfig.lock();
  auto curConfig = _curBackpackLightConfig.lock();

  // Prevent spamming of off lights while both configs are null
  static bool sBothConfigsWereNull = false;

  // If the best config at this time is different from what we had, change it
  if (newConfig != curConfig)
  {
    sBothConfigsWereNull = false;
    // If the best config is still a thing, use it. Otherwise use the off config
    if (newConfig != nullptr)
    {
      SendBackpackLights(newConfig->_lightConfiguration);
    }
    else
    {
      SendBackpackLights(BackpackAnimationTrigger::Off);
    }
    
    _curBackpackLightConfig = bestNewConfig;
  }
  // Else if both new and cur configs are null then turn lights off
  else if(newConfig == nullptr && curConfig == nullptr &&
          !sBothConfigsWereNull)
  {
    sBothConfigsWereNull = true;
    SendBackpackLights(BackpackAnimationTrigger::Off);
  }
}

// behavior lights are triggered from the engine and show the state for an active behavior.
// we want these specific behavior lights to take precedence over some of the critical lights, but the way the system
// was setup, all critical lights take precedence over all engine lights.  This is a little workaround for that so we
// can determine if a higher priority "behavior light" (which is triggered from the engine) should take precedence
// over the current critical light ... see UpdateCriticalBackpackLightConfig(...) for how this is used
// ** a more robust solution will follow when we refactor the (many) light components once anim/engine processes are merged **
bool BackpackLightComponent::IsBehaviorBackpackLightActive() const
{
  bool isAnyBehaviorLightActive = false;

  // _mostRecentTrigger tracks the last trigger that was requested from the engine
  switch ( _mostRecentTrigger )
  {
    case BackpackAnimationTrigger::WorkingOnIt:
    case BackpackAnimationTrigger::SpinnerBlueCelebration:
    case BackpackAnimationTrigger::SpinnerBlueHoldTarget:
    case BackpackAnimationTrigger::SpinnerBlueSelectTarget:
    case BackpackAnimationTrigger::SpinnerGreenCelebration:
    case BackpackAnimationTrigger::SpinnerGreenHoldTarget:
    case BackpackAnimationTrigger::SpinnerGreenSelectTarget:
    case BackpackAnimationTrigger::SpinnerPurpleCelebration:
    case BackpackAnimationTrigger::SpinnerPurpleHoldTarget:
    case BackpackAnimationTrigger::SpinnerPurpleSelectTarget:
    case BackpackAnimationTrigger::SpinnerRedCelebration:
    case BackpackAnimationTrigger::SpinnerRedHoldTarget:
    case BackpackAnimationTrigger::SpinnerRedSelectTarget:
    case BackpackAnimationTrigger::SpinnerYellowCelebration:
    case BackpackAnimationTrigger::SpinnerYellowHoldTarget:
    case BackpackAnimationTrigger::SpinnerYellowSelectTarget:
    case BackpackAnimationTrigger::DanceToTheBeat:
    case BackpackAnimationTrigger::MeetVictor:
      isAnyBehaviorLightActive = true;
      break;

    default:
      break;
  }

  return isAnyBehaviorLightActive;
}

void BackpackLightComponent::SetBackpackAnimation(const BackpackLightAnimation::BackpackAnimation& lights)
{
  // if we're forcing a manual light, reset our most recent trigger
  _mostRecentTrigger = BackpackAnimationTrigger::Off;
  StartBackpackAnimationInternal(lights,
                                 Util::EnumToUnderlying(BackpackLightSourcePrivate::Engine),
                                 _engineLightConfig);
}

void BackpackLightComponent::SetBackpackAnimation(const BackpackAnimationTrigger& trigger)
{
  auto animName = _backpackTriggerToNameMap->GetValue(trigger);
  auto anim = _backpackLightContainer->GetAnimation(animName);

  if(anim == nullptr)
  {
    PRINT_NAMED_ERROR("BackpackLightComponent.SetBackpackAnimation.NoAnimForTrigger",
                      "Could not find animation for trigger %s name %s",
                      EnumToString(trigger), animName.c_str());
    return;
  }

  // keep track of what trigger is currently active (from the engine)
  _mostRecentTrigger = trigger;
  StartBackpackAnimationInternal(*anim,
                                 Util::EnumToUnderlying(BackpackLightSourcePrivate::Engine),
                                 _engineLightConfig);
}
 
void BackpackLightComponent::StartBackpackAnimationInternal(const BackpackLightAnimation::BackpackAnimation& lights,
                                                            BackpackLightSourceType source,
                                                            BackpackLightDataLocator& lightLocator_out)
{
  StopBackpackAnimationInternal(lightLocator_out);

  _backpackLightMap[source].emplace_front(new BackpackLightData{lights});
  
  BackpackLightDataLocator result{};
  result._mapIter = _backpackLightMap.find(source);
  result._listIter = result._mapIter->second.begin();
  result._dataPtr = std::weak_ptr<BackpackLightData>(*result._listIter);
  
  lightLocator_out = std::move(result);
}
  
bool BackpackLightComponent::StopBackpackAnimationInternal(const BackpackLightDataLocator& lightDataLocator)
{
  if (!lightDataLocator.IsValid())
  {
    PRINT_CH_INFO("BackpackLightComponent",
                  "BackpackLightComponent.StopBackpackAnimationInternal.InvalidLocator",
                  "Trying to remove an invalid locator.");
    return false;
  }
  
  if(!lightDataLocator._mapIter->second.empty())
  {
    lightDataLocator._mapIter->second.erase(lightDataLocator._listIter);
  }
  else
  {
    PRINT_NAMED_WARNING("BackpackLightComponent.StopBackpackAnimationInternal.NoLocators",
                        "Trying to remove supposedly valid locator but locator list is empty");
    return false;
  }
  
  if (lightDataLocator._mapIter->second.empty())
  {
    _backpackLightMap.erase(lightDataLocator._mapIter);
  }
  
  return true;
}

Result BackpackLightComponent::SendBackpackLights(const BackpackLightAnimation::BackpackAnimation& lights)
{
  RobotInterface::SetBackpackLights setBackpackLights = lights.lights;
  setBackpackLights.layer = EnumToUnderlyingType(BackpackLightLayer::BPL_USER);

  const auto msg = RobotInterface::EngineToRobot(setBackpackLights);
  const bool res = AnimComms::SendPacketToRobot((char*)msg.GetBuffer(), msg.Size());
  return (res ? RESULT_OK : RESULT_FAIL);
}

Result BackpackLightComponent::SendBackpackLights(const BackpackAnimationTrigger& trigger)
{
  auto animName = _backpackTriggerToNameMap->GetValue(trigger);
  auto anim = _backpackLightContainer->GetAnimation(animName);

  if(anim == nullptr)
  {
    PRINT_NAMED_ERROR("BackpackLightComponent.SendBackpackLights.NoAnimForTrigger",
                      "Could not find animation for trigger %s name %s",
                      EnumToString(trigger), animName.c_str());
    return RESULT_FAIL;
  }

  return SendBackpackLights(*anim);
}


std::vector<BackpackLightSourceType> BackpackLightComponent::GetLightSourcePriority()
{
  constexpr BackpackLightSourceType priorityOrder[] =
  {
    Util::EnumToUnderlying(BackpackLightSourcePrivate::Critical),
    Util::EnumToUnderlying(BackpackLightSourcePrivate::Engine),
  };
  constexpr auto numElements = sizeof(priorityOrder) / sizeof(priorityOrder[0]);
  static_assert(numElements == Util::EnumToUnderlying(BackpackLightSourcePrivate::Count),
                "BackpackLightSource priority list does not contain an entry for each type of BackpackLightSource.");
  
  const auto* beginIter = &priorityOrder[0];
  const auto* endIter = beginIter + numElements;
  return std::vector<BackpackLightSourceType>(beginIter, endIter);
}
  
BackpackLightDataRefWeak BackpackLightComponent::GetBestLightConfig()
{
  if (_backpackLightMap.empty())
  {
    return BackpackLightDataRef{};
  }
  
  static const auto priorityList = GetLightSourcePriority();
  for (const auto& source : priorityList)
  {
    auto iter = _backpackLightMap.find(source);
    if (iter != _backpackLightMap.end())
    {
      const auto& listForSource = iter->second;
      if (!listForSource.empty())
      {
        return *listForSource.begin();
      }
    }
  }
  
  return BackpackLightDataRef{};
}

void BackpackLightComponent::SetPairingLight(bool isOn)
{
  _systemLightState = (isOn ? SystemLightState::Pairing : SystemLightState::Off);
}

void BackpackLightComponent::UpdateSystemLightState(bool isCloudStreamOpen)
{
  if(_systemLightState == SystemLightState::Off &&
     _selfTestRunning)
  {
    _systemLightState = SystemLightState::SelfTest;
  }
  else if(_systemLightState == SystemLightState::SelfTest &&
          !_selfTestRunning)
  {
    _systemLightState = SystemLightState::Off;
  }

  // Check if cloud streaming has changed
  // Only show streaming system light if we are not showing anything else
  // We don't want to accidentally override the pairing light. We will still
  // indicate we are streaming with the other backpack lights.
  if(_systemLightState == SystemLightState::Off &&
     isCloudStreamOpen)
  {
    _systemLightState = SystemLightState::Streaming; 
  }
  else if(_systemLightState == SystemLightState::Streaming &&
          !isCloudStreamOpen)
  {
    _systemLightState = SystemLightState::Off; 
  }

  static SystemLightState prevState = SystemLightState::Invalid;
  if(prevState != _systemLightState)
  {
    prevState = _systemLightState;

    LightState light;
    switch(_systemLightState)
    {
      case SystemLightState::Invalid:
      {
        DEV_ASSERT(false, "BackpackLightComponent.UpdateSystemLightState.Invalid");
        return;
      }
      break;
      
      case SystemLightState::Off:
      {
        light.onColor = 0x00FF0000;
        light.offColor = 0x00FF0000;
        light.onPeriod_ms = 960;
        light.offPeriod_ms = 960;
        light.transitionOnPeriod_ms = 0;
        light.transitionOffPeriod_ms = 0;
        light.offset_ms = 0;
      }
      break;

      case SystemLightState::Pairing:
      {
        // Pulsing yellow
        light.onColor = 0xFFFF0000;
        light.offColor = 0x00FF0000;
        light.onPeriod_ms = 960;
        light.offPeriod_ms = 960;
        light.transitionOnPeriod_ms = 0;
        light.transitionOffPeriod_ms = 0;
        light.offset_ms = 0;
      }
      break;

      case SystemLightState::Streaming:
      {
        // Pulsing cyan
        light.onColor = 0x00FFFF00;
        light.offColor = 0x00FFFF00;
        light.onPeriod_ms = 960;
        light.offPeriod_ms = 960;
        light.transitionOnPeriod_ms = 0;
        light.transitionOffPeriod_ms = 0;
        light.offset_ms = 0;
      }
      break;

      case SystemLightState::SelfTest:
      {
        // White
        light.onColor = 0xFFFFFF00;
        light.offColor = 0xFFFFFF00;
        light.onPeriod_ms = 960;
        light.offPeriod_ms = 960;
        light.transitionOnPeriod_ms = 0;
        light.transitionOffPeriod_ms = 0;
        light.offset_ms = 0;
      }
      break;
    }

    // If user space is unsecure then mix white in
    // to the system light as the off color (normally green)
    if(!OSState::getInstance()->IsUserSpaceSecure())
    {
      light.offColor = 0xFFFFFF00;
      light.onPeriod_ms = 960;
      light.offPeriod_ms = 960;
      light.transitionOnPeriod_ms = 0;
      light.transitionOffPeriod_ms = 0;
      light.offset_ms = 0;
    }

    const auto msg = RobotInterface::EngineToRobot(RobotInterface::SetSystemLight({light}));
    AnimComms::SendPacketToRobot((char*)msg.GetBuffer(), msg.Size());
  }
}

void BackpackLightComponent::UpdateOfflineCheck(bool force)
{
  static AnimTimeStamp_t lastCheck_ms = 0;
  const AnimTimeStamp_t curTime_ms = BaseStationTimer::getInstance()->GetCurrentTimeStamp();

  if((curTime_ms - lastCheck_ms > kOfflineCheckFreq_ms) || force)
  {
    lastCheck_ms = curTime_ms;
    const std::string& ip = OSState::getInstance()->GetIPAddress(true);
    const bool isValidIP = OSState::getInstance()->IsValidIPAddress(ip);

    if(_offlineAtTime_ms == 0 && !isValidIP)
    {
      _offlineAtTime_ms = (TimeStamp_t)curTime_ms;
    }
    else if(_offlineAtTime_ms > 0 && isValidIP)
    {
      _offlineAtTime_ms = 0;
    }
  }
}

void BackpackLightComponent::UpdateBatteryStatus(const RobotInterface::BatteryStatus& msg)
{
  _isBatteryLow = msg.isLow;
  _isBatteryCharging = msg.isCharging;
  _isOnChargerContacts = msg.onChargerContacts;
  _isBatteryFull = msg.isBatteryFull;
  _isBatteryDisconnected = msg.isBatteryDisconnected;
}

}
}
}
