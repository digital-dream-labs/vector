/**
 * File: keyframe.cpp
 *
 * Authors: Andrew Stein
 * Created: 2015-06-25
 *
 * Description:
 *   Defines the various KeyFrames used to store an animation on the
 *   the robot, all of which inherit from a common interface,
 *   IKeyFrame.
 *
 * Copyright: Anki, Inc. 2015
 *
 **/


#include "coretech/common/shared/array2d.h"
#include "coretech/common/engine/colorRGBA.h"
#include "coretech/common/engine/jsonTools.h"
#include "coretech/common/engine/utils/timer.h"
#include "cannedAnimLib/baseTypes/cozmo_anim_generated.h"
#include "cannedAnimLib/baseTypes/keyframe.h"
#include "anki/cozmo/shared/cozmoConfig.h"
#include "clad/robotInterface/messageEngineToRobot.h"
#include "util/cpuProfiler/cpuProfiler.h"
#include "util/helpers/quoteMacro.h"
#include "util/logging/logging.h"

#include <opencv2/core.hpp>
#include <cassert>


bool has_any_digits(const std::string& s)
{
  return std::any_of(s.begin(), s.end(), ::isdigit);
}


#define CREATE_STREAM_MSG(msgName, sourceMsg)

namespace Anki {
  namespace Vector {
#pragma mark -
#pragma mark IKeyFrame
    
    // Static initialization
    Util::RandomGenerator IKeyFrame::sRNG;
    
    IKeyFrame::IKeyFrame()
    {
      
    }
    
    IKeyFrame::~IKeyFrame()
    {
      
    }
    
    bool IKeyFrame::IsTimeToPlay(const TimeStamp_t timeSinceAnimStart_ms) const
    {
      return GetTriggerTime_ms() <= timeSinceAnimStart_ms;
    }
    
    Result IKeyFrame::DefineFromJson(const Json::Value &json, const std::string& animNameDebug)
    {
      Result lastResult = RESULT_OK;
      
      // Read the frame time from the json file as well
      if(!json.isMember("triggerTime_ms")) {
        PRINT_NAMED_ERROR("IKeyFrame.ReadFromJson",
                          "%s: Expecting 'triggerTime_ms' field in KeyFrame Json",
                          animNameDebug.c_str());
        lastResult = RESULT_FAIL;
      } else {
        _triggerTime_ms = json["triggerTime_ms"].asUInt();
      }
      
      if(lastResult == RESULT_OK) {
        lastResult = SetMembersFromJson(json, animNameDebug);
      }
      
      return lastResult;
    }
    
#pragma mark -
#pragma mark Helpers
    
    // Helper macro used in SetMembersFromJson() overrides below to look for
    // member variable in Json node and fail if it doesn't exist
#define GET_MEMBER_FROM_JSON_AND_STORE_IN(__JSON__, __NAME__, __MEMBER_NAME__) do { \
if(!JsonTools::GetValueOptional(__JSON__, QUOTE(__NAME__), this->_##__MEMBER_NAME__)) { \
PRINT_NAMED_ERROR("IKeyFrame.GetMemberFromJsonMacro", \
"Failed to get '%s' from Json file.", QUOTE(__NAME__)); \
return RESULT_FAIL; \
} } while(0)
    
#define GET_MEMBER_FROM_JSON(__JSON__, __NAME__) GET_MEMBER_FROM_JSON_AND_STORE_IN(__JSON__, __NAME__, __NAME__)

namespace {
  
// Cast the value in fromVal to toVal, clamping to the numerical limits of the target type and printing a debug message if so
template<typename FromType, typename ToType>
void SafeNumericCast(const FromType& fromVal, ToType& toVal, const char* debugName = "InvalidCast")
{
  if (!Util::IsValidNumericCast<ToType>(fromVal)) {
    toVal = Util::numeric_cast_clamped<ToType>(fromVal);
#if ANKI_DEV_CHEATS
    std::stringstream debugStr;
    debugStr << "cast of " << fromVal << " would be invalid, clamping to " << toVal;
    PRINT_NAMED_WARNING("IKeyFrame.SafeNumericCast.InvalidCast",
                        "%s: %s",
                        debugName,
                        debugStr.str().c_str());
#endif
  } else {
    toVal = Util::numeric_cast<ToType>(fromVal);
  }
}
  
} // end anonymous namespace
  
#pragma mark -
#pragma mark HeadAngleKeyFrame

    //
    // HeadAngleKeyFrame
    //
    
     HeadAngleKeyFrame::HeadAngleKeyFrame(s8 angle_deg, u8 angle_variability_deg, TimeStamp_t duration_ms)
     : _angle_deg(angle_deg)
     , _angleVariability_deg(angle_variability_deg)
     {
     }
    
    #if CAN_STREAM
      RobotInterface::EngineToRobot* HeadAngleKeyFrame::GetStreamMessage(const TimeStamp_t timeSinceAnimStart_ms) const
      {
        if(!IsFirstKeyframeTick(timeSinceAnimStart_ms)){
          return nullptr;
        }
        
        RobotInterface::SetHeadAngle streamHeadMsg;
        streamHeadMsg.actionID = 0;
        streamHeadMsg.duration_sec = 0.001 * _keyframeActiveDuration_ms;
        
        // Add variability:
        if(_angleVariability_deg > 0) {
          streamHeadMsg.angle_rad = DEG_TO_RAD(static_cast<s8>(GetRNG().RandIntInRange(_angle_deg - _angleVariability_deg,
                                                                                        _angle_deg + _angleVariability_deg)));
        } else {
          streamHeadMsg.angle_rad = DEG_TO_RAD(_angle_deg);
        }
        
        return new RobotInterface::EngineToRobot(streamHeadMsg);
      }

    #endif

    Result HeadAngleKeyFrame::DefineFromFlatBuf(const CozmoAnim::HeadAngle* headAngleKeyframe, const std::string& animNameDebug)
    {
      DEV_ASSERT(headAngleKeyframe != nullptr, "HeadAngleKeyFrame.DefineFromFlatBuf.NullAnim");
      SafeNumericCast(headAngleKeyframe->triggerTime_ms(), _triggerTime_ms, animNameDebug.c_str());
      Result lastResult = SetMembersFromFlatBuf(headAngleKeyframe, animNameDebug);
      return lastResult;
    }

    Result HeadAngleKeyFrame::SetMembersFromFlatBuf(const CozmoAnim::HeadAngle* headAngleKeyframe, const std::string& animNameDebug)
    {
      SafeNumericCast(headAngleKeyframe->durationTime_ms(),      _keyframeActiveDuration_ms,    animNameDebug.c_str());
      SafeNumericCast(headAngleKeyframe->angle_deg(),            _angle_deg,            animNameDebug.c_str());
      SafeNumericCast(headAngleKeyframe->angleVariability_deg(), _angleVariability_deg, animNameDebug.c_str());
      return RESULT_OK;
    }
    
    Result HeadAngleKeyFrame::SetMembersFromJson(const Json::Value &jsonRoot, const std::string& animNameDebug)
    {
      GET_MEMBER_FROM_JSON_AND_STORE_IN(jsonRoot, durationTime_ms, keyframeActiveDuration_ms);
      GET_MEMBER_FROM_JSON(jsonRoot, angle_deg);
      GET_MEMBER_FROM_JSON(jsonRoot, angleVariability_deg);
      return RESULT_OK;
    }
    
#pragma mark -
#pragma mark LiftHeightKeyFrame
    //
    // LiftHeightKeyFrame
    //

    LiftHeightKeyFrame::LiftHeightKeyFrame(u8 height_mm, u8 heightVariability_mm, TimeStamp_t duration_ms)
    : _height_mm(height_mm)
    , _heightVariability_mm(heightVariability_mm)
    {
    }
    
    #if CAN_STREAM
      RobotInterface::EngineToRobot* LiftHeightKeyFrame::GetStreamMessage(const TimeStamp_t timeSinceAnimStart_ms) const
      {
        if(!IsFirstKeyframeTick(timeSinceAnimStart_ms)){
          return nullptr;
        }
        
        RobotInterface::SetLiftHeight streamLiftMsg;
        streamLiftMsg.actionID = 0;
        streamLiftMsg.duration_sec = Util::MilliSecToSec(static_cast<float>(_keyframeActiveDuration_ms));
        
        // Add variability:
        if(_heightVariability_mm > 0) {
          streamLiftMsg.height_mm = (uint8_t)static_cast<s8>(GetRNG().RandIntInRange(_height_mm - _heightVariability_mm,
                                                                                     _height_mm + _heightVariability_mm));
        } else {
          streamLiftMsg.height_mm = _height_mm;
        }

        return new RobotInterface::EngineToRobot(streamLiftMsg);
      }
    #endif

    Result LiftHeightKeyFrame::DefineFromFlatBuf(const CozmoAnim::LiftHeight* liftHeightKeyframe, const std::string& animNameDebug)
    {
      DEV_ASSERT(liftHeightKeyframe != nullptr, "LiftHeightKeyFrame.DefineFromFlatBuf.NullAnim");
      SafeNumericCast(liftHeightKeyframe->triggerTime_ms(), _triggerTime_ms, animNameDebug.c_str());
      Result lastResult = SetMembersFromFlatBuf(liftHeightKeyframe, animNameDebug);
      return lastResult;
    }

    Result LiftHeightKeyFrame::SetMembersFromFlatBuf(const CozmoAnim::LiftHeight* liftHeightKeyframe, const std::string& animNameDebug)
    {
      SafeNumericCast(liftHeightKeyframe->durationTime_ms(),      _keyframeActiveDuration_ms,    animNameDebug.c_str());
      SafeNumericCast(liftHeightKeyframe->height_mm(),            _height_mm,            animNameDebug.c_str());
      SafeNumericCast(liftHeightKeyframe->heightVariability_mm(), _heightVariability_mm, animNameDebug.c_str());
      
      return RESULT_OK;
    }
    
    Result LiftHeightKeyFrame::SetMembersFromJson(const Json::Value &jsonRoot, const std::string& animNameDebug)
    {
      GET_MEMBER_FROM_JSON_AND_STORE_IN(jsonRoot, durationTime_ms, keyframeActiveDuration_ms);
      GET_MEMBER_FROM_JSON(jsonRoot, height_mm);
      GET_MEMBER_FROM_JSON(jsonRoot, heightVariability_mm);
      return RESULT_OK;
    }
    
#pragma mark -
#pragma mark ProceduralFaceKeyFrame
    
    Result ProceduralFaceKeyFrame::DefineFromFlatBuf(const CozmoAnim::ProceduralFace* procFaceKeyframe, const std::string& animNameDebug)
    {
      DEV_ASSERT(procFaceKeyframe != nullptr, "ProceduralFaceKeyFrame.DefineFromFlatBuf.NullAnim");
      SafeNumericCast(procFaceKeyframe->triggerTime_ms(), _triggerTime_ms, animNameDebug.c_str());
      Result lastResult = SetMembersFromFlatBuf(procFaceKeyframe, animNameDebug);
      return lastResult;
    }

    Result ProceduralFaceKeyFrame::SetMembersFromFlatBuf(const CozmoAnim::ProceduralFace* procFaceKeyframe, const std::string& animNameDebug)
    {
      _procFace.SetFromFlatBuf(procFaceKeyframe);
      return RESULT_OK;
    }

    Result ProceduralFaceKeyFrame::SetMembersFromJson(const Json::Value &jsonRoot, const std::string& animNameDebug)
    {
      _procFace.SetFromJson(jsonRoot);
      return RESULT_OK;
    }


    ProceduralFace ProceduralFaceKeyFrame::GetInterpolatedFace(const ProceduralFaceKeyFrame& nextFrame, const TimeStamp_t currentTime_ms)
    {
      // The interpolation fraction is how far along in time we are from this frame's
      // trigger time (which currentTime was initialized to) and the next frame's
      // trigger time.
      const f32 fraction = std::min(1.f, static_cast<f32>(currentTime_ms - GetTriggerTime_ms()) / static_cast<f32>(nextFrame.GetTriggerTime_ms() - GetTriggerTime_ms()));
      
      ProceduralFace interpFace;
      interpFace.Interpolate(_procFace, nextFrame._procFace, fraction);
      
      return interpFace;
    }
    
#pragma mark -
#pragma mark RobotAudioKeyFrame
    
    //
    // RobotAudioKeyFrame
    //
    
    // By Default use "Animation" Audio Game Object
    const auto kAnimationGameObject = AudioMetaData::GameObjectType::Animation;
    
    Result RobotAudioKeyFrame::AddAudioRef(AudioKeyFrameType::AudioRef&& audioRef)
    {
      _audioReferences.push_back( std::move(audioRef) );
      return RESULT_OK;
    }

    Result RobotAudioKeyFrame::AddAudioRef(AudioKeyFrameType::AudioEventGroupRef&& eventGroupRef)
    {
      _audioReferences.push_back( AudioKeyFrameType::AudioRef( std::move(eventGroupRef) ) );
      return RESULT_OK;
    }

    Result RobotAudioKeyFrame::AddAudioRef(AudioKeyFrameType::AudioParameterRef&& parameterRef)
    {
      _audioReferences.push_back( AudioKeyFrameType::AudioRef( std::move(parameterRef) ) );
      return RESULT_OK;
    }

    Result RobotAudioKeyFrame::AddAudioRef(AudioKeyFrameType::AudioStateRef&& stateRef)
    {
      _audioReferences.push_back( AudioKeyFrameType::AudioRef( std::move(stateRef) ) );
      return RESULT_OK;
    }

    Result RobotAudioKeyFrame::AddAudioRef(AudioKeyFrameType::AudioSwitchRef&& switchRef)
    {
      _audioReferences.push_back( AudioKeyFrameType::AudioRef( std::move(switchRef) ) );
      return RESULT_OK;
    }
    
    void RobotAudioKeyFrame::MergeKeyFrame(RobotAudioKeyFrame&& otherFrame)
    {
      _audioReferences.insert(_audioReferences.end(),
                              std::make_move_iterator(otherFrame._audioReferences.begin()),
                              std::make_move_iterator(otherFrame._audioReferences.end()));
      // Remove junk
      otherFrame._audioReferences.clear();
    }

    Result RobotAudioKeyFrame::DefineFromFlatBuf(const CozmoAnim::RobotAudio* audioKeyframe, const std::string& animNameDebug)
    {
      DEV_ASSERT(audioKeyframe != nullptr, "RobotAudioKeyFrame.DefineFromFlatBuf.NullAnim");
      SafeNumericCast(audioKeyframe->triggerTime_ms(), _triggerTime_ms, animNameDebug.c_str());
      Result lastResult = SetMembersFromFlatBuf(audioKeyframe, animNameDebug);
      return lastResult;
    }
    
#define JSON_KEY( __KEY__ ) static const char* kKey_##__KEY__ = QUOTE(__KEY__)
    
    Result RobotAudioKeyFrame::SetMembersFromJson(const Json::Value &jsonRoot, const std::string& animNameDebug)
    {
      using namespace AudioEngine;
      using namespace AudioKeyFrameType;
      using namespace AudioMetaData;
      
      // Check for deprecated format
      if ( jsonRoot.isMember("audioEventId") ) {
        // Deprecated format
        return SetMembersFromDeprecatedJson(jsonRoot, animNameDebug);
      }
      
      // Frame type list keys
      JSON_KEY(eventGroups);
      JSON_KEY(states);
      JSON_KEY(switches);
      JSON_KEY(parameters);
      
      // Add States
      const auto& states = jsonRoot[kKey_states];
      if (states.isArray()) {
        JSON_KEY(stateGroupId);
        JSON_KEY(stateId);
        for (auto stateIt = states.begin(); stateIt != states.end(); ++stateIt) {
          auto groupId = static_cast<u32>(GameState::StateGroupType::Invalid);
          auto stateId = static_cast<u32>(GameState::GenericState::Invalid);
          JsonTools::GetValueOptional(*stateIt, kKey_stateGroupId, groupId);
          JsonTools::GetValueOptional(*stateIt, kKey_stateId, stateId);
          if (((u32)GameState::StateGroupType::Invalid == groupId) || ((u32)GameState::GenericState::Invalid == stateId)) {
            PRINT_NAMED_ERROR("RobotAudioKeyFrame.SetMembersFromJson.InvalidGameState",
                              "'%s' @ %i ms : Has an invalid stateGroupId (%i) or stateId (%i)",
                              animNameDebug.c_str(), _triggerTime_ms, groupId, stateId);
            // Move to next state
            continue;
          }
          Result addResult = AddAudioRef(AudioStateRef(static_cast<GameState::StateGroupType>(groupId),
                                                       static_cast<GameState::GenericState>(stateId)));
          if(addResult != RESULT_OK) {
            return addResult;
          }
        }
      } // States
      
      // Add Switches
      const auto& switches = jsonRoot[kKey_switches];
      if (switches.isArray()) {
        JSON_KEY(switchGroupId);
        JSON_KEY(stateId);
        for (auto switchIt = switches.begin(); switchIt != switches.end(); ++switchIt) {
          auto groupId = static_cast<u32>(SwitchState::SwitchGroupType::Invalid);
          auto stateId = static_cast<u32>(SwitchState::GenericSwitch::Invalid);
          JsonTools::GetValueOptional(*switchIt, kKey_switchGroupId, groupId);
          JsonTools::GetValueOptional(*switchIt, kKey_stateId, stateId);
          if (((u32)SwitchState::SwitchGroupType::Invalid == groupId) ||
              ((u32)SwitchState::GenericSwitch::Invalid == stateId)) {
            PRINT_NAMED_ERROR("RobotAudioKeyFrame.SetMembersFromJson.InvalidSwitchState",
                              "'%s' @ %i ms : Has an invalid switchGroupId (%i) or stateId (%i)",
                              animNameDebug.c_str(), _triggerTime_ms, groupId, stateId);
            // Move to next switch
            continue;
          }
          
          Result addResult = AddAudioRef(AudioSwitchRef(static_cast<SwitchState::SwitchGroupType>(groupId),
                                                        static_cast<SwitchState::GenericSwitch>(stateId),
                                                        kAnimationGameObject));
          if(addResult != RESULT_OK) {
            return addResult;
          }
        }
      } // Switches
      
      // Add Parameters
      const auto& parameters = jsonRoot[kKey_parameters];
      if (parameters.isArray()) {
        JSON_KEY(parameterID);
        JSON_KEY(value);
        JSON_KEY(time_ms);
        JSON_KEY(curveType);
        for (auto parameterIt = parameters.begin(); parameterIt != parameters.end(); ++parameterIt) {
          auto parameterId = static_cast<u32>(GameParameter::ParameterType::Invalid);
          float value = 0.0f;
          u32   time_ms = 0;
          u8    curve = static_cast<u8>(AudioEngine::Multiplexer::CurveType::Linear);
          JsonTools::GetValueOptional(*parameterIt, kKey_parameterID, parameterId);
          if ((u32)GameParameter::ParameterType::Invalid == parameterId) {
            PRINT_NAMED_ERROR("RobotAudioKeyFrame.SetMembersFromJson.InvalidParameter",
                              "'%s' @ %i ms : Has an invalid parameterID", animNameDebug.c_str(), _triggerTime_ms);
            // Move to next parameter
            continue;
          }
          JsonTools::GetValueOptional(*parameterIt, kKey_value, value);
          JsonTools::GetValueOptional(*parameterIt, kKey_time_ms, time_ms);
          JsonTools::GetValueOptional(*parameterIt, kKey_curveType, curve);
          
          Result addResult = AddAudioRef(AudioParameterRef(static_cast<GameParameter::ParameterType>(parameterId),
                                                           value,
                                                           time_ms,
                                                           static_cast<AudioEngine::Multiplexer::CurveType>(curve),
                                                           kAnimationGameObject));
          if(addResult != RESULT_OK) {
            return addResult;
          }
        }
      } // Parameters
      
      // Add Event Groups
      // Note: Add events last to the AudioRef list, they need to be posted last when performing a key frame
      const auto& eventGroups = jsonRoot[kKey_eventGroups];
      if (eventGroups.isArray()) {
        JSON_KEY(eventIds);
        JSON_KEY(volumes);
        JSON_KEY(probabilities);
        
        for (auto eventGroupIt = eventGroups.begin(); eventGroupIt != eventGroups.end(); ++eventGroupIt) {
          const auto& eventIds = (*eventGroupIt)[kKey_eventIds];
          const auto& volumes = (*eventGroupIt)[kKey_volumes];
          const auto& probabilities = (*eventGroupIt)[kKey_probabilities];
          if ((eventIds.size() != volumes.size()) || (eventIds.size() != probabilities.size())) {
            PRINT_NAMED_ERROR("RobotAudioKeyFrame.SetMembersFromJson.InvlaidEventGroup",
                              "'%s' @ %i ms : EventIds, Volumes & Probabilities don't have the same count",
                              animNameDebug.c_str(), _triggerTime_ms);
            // Move to next Event Group
            continue;
          }
          // Check sum of all probabilities to ensure it is <= 1.0
          f32 totalProb = 0.0f;
          for (auto probIt = probabilities.begin(); probIt != probabilities.end(); ++probIt) {
            totalProb += probIt->asFloat();
            if (totalProb > 1.0f) {
              PRINT_NAMED_ERROR("RobotAudioKeyFrame.SetMembersFromJson.TotalProbabilitiesTooHigh",
                                "'%s' @ %i ms : The total probability of all audio events combined exceeds 1.0",
                                animNameDebug.c_str(), _triggerTime_ms);
              return RESULT_FAIL;
            }
          }
          
          // Add events to group
          AudioEventGroupRef eventGroup(kAnimationGameObject);
          GameEvent::GenericEvent eventId;
          for (int idx = 0; idx < eventIds.size(); ++idx) {
            eventId = static_cast<GameEvent::GenericEvent>( eventIds[idx].asUInt() );
            if (GameEvent::GenericEvent::Invalid == eventId) {
              PRINT_NAMED_ERROR("RobotAudioKeyFrame.SetMembersFromJson.InvalidGameEvent",
                                "'%s' @ %i ms : Has an invalid audio event", animNameDebug.c_str(), _triggerTime_ms);
              // Move on to next event
              continue;
            }
            eventGroup.AddEvent(eventId, volumes[idx].asFloat(), probabilities[idx].asFloat());
          }
          
          if (eventGroup.Events.empty()) {
            PRINT_NAMED_ERROR("RobotAudioKeyFrame.SetMembersFromJson.InvalidGameEventGroup",
                              "'%s' @ %i ms : Has an empty event group", animNameDebug.c_str(), _triggerTime_ms);
            return  RESULT_FAIL;
          }
          Result addResult = AddAudioRef(std::move(eventGroup));
          if (addResult != RESULT_OK) {
            return addResult;
          }
        }
      } // EventGroups
      return RESULT_OK;
    }
    
    Result RobotAudioKeyFrame::SetMembersFromDeprecatedJson(const Json::Value &jsonRoot,
                                                            const std::string& animNameDebug)
    {
      using namespace AudioKeyFrameType;
      using namespace AudioMetaData;
      
      JSON_KEY(audioEventId);
      JSON_KEY(volume);
      JSON_KEY(probability);
      
      // Get volume settings
      f32 volume = 1.0f;
      JsonTools::GetValueOptional(jsonRoot, kKey_volume, volume);
      f32 probability = 1.0f;
      
      const Json::Value& eventIds = jsonRoot[kKey_audioEventId];
      if (eventIds.isArray()) {
        std::vector<f32> probabilities;
        const bool probabilitiesSet = JsonTools::GetVectorOptional(jsonRoot, kKey_probability, probabilities);
        if (!probabilitiesSet) {
          const bool probabilitySet = JsonTools::GetValueOptional(jsonRoot, kKey_probability, probability);
          if (probabilitySet) {
            probabilities.push_back(probability);
          }
        }
        
        if (probabilities.empty() && !eventIds.empty()) {
          // If no probability is set, use equal probability for all audio events
          const f32 eachProbability = 1.0f / eventIds.size();
          for (int idx = 0; idx < eventIds.size(); idx++) {
            probabilities.push_back(eachProbability);
          }
        }
        else if (probabilities.size() != eventIds.size()) {
          // We must have the same number of probability values as audio events
          PRINT_NAMED_ERROR("RobotAudioKeyFrame.SetMembersFromDeprecatedJson.UnknownProbabilities",
                            "%s: The number of audio event IDs (%u) does not match number of probabilities (%zu)",
                            animNameDebug.c_str(), eventIds.size(), probabilities.size());
          return RESULT_FAIL;
        }
        
        // Check sum of all probabilities to ensure it is <= 1.0
        f32 totalProb = 0.0f;
        for (int probIdx = 0; probIdx < probabilities.size(); probIdx++) {
          totalProb += probabilities[probIdx];
          if(totalProb > 1.0) {
            PRINT_NAMED_ERROR("RobotAudioKeyFrame.SetMembersFromDeprecatedJson.TotalProbabilitiesTooHigh",
                              "%s: The total probability of all audio events combined exceeds 1.0",
                              animNameDebug.c_str());
            return RESULT_FAIL;
          }
        }
        
        AudioEventGroupRef eventGroup(kAnimationGameObject);
        for (int idx = 0; idx < eventIds.size(); ++idx) {
          probability = probabilities[idx];
          const auto eventId = static_cast<GameEvent::GenericEvent>( eventIds[idx].asUInt() );
          if (GameEvent::GenericEvent::Invalid == eventId) {
            PRINT_NAMED_ERROR("RobotAudioKeyFrame.SetMembersFromDeprecatedJson.InvalidGameEvent",
                              "'%s' @ %i ms : Has an invalid audio event", animNameDebug.c_str(), _triggerTime_ms);
            // Move on to next event
            continue;
          }
          eventGroup.AddEvent(eventId, volume, probability);
        }
        Result addResult = AddAudioRef(std::move(eventGroup));
        if (addResult != RESULT_OK) {
          return addResult;
        }
      }
      else {
        JsonTools::GetValueOptional(jsonRoot, kKey_probability, probability);
        const auto eventId = static_cast<GameEvent::GenericEvent>( eventIds.asUInt() );
        if (GameEvent::GenericEvent::Invalid == eventId) {
          PRINT_NAMED_ERROR("RobotAudioKeyFrame.SetMembersFromDeprecatedJson.InvalidGameEvent",
                            "'%s' @ %i ms : Has an invalid audio event", animNameDebug.c_str(), _triggerTime_ms);
          return RESULT_FAIL;
        }
        AudioEventGroupRef eventGroup(kAnimationGameObject);
        eventGroup.AddEvent(eventId, volume, probability);
        Result addResult = AddAudioRef(std::move(eventGroup));
        if(addResult != RESULT_OK) {
          return addResult;
        }
      }
      return RESULT_OK;
    }
    
    
    Result RobotAudioKeyFrame::SetMembersFromFlatBuf(const CozmoAnim::RobotAudio* audioKeyframe,
                                                    const std::string& animNameDebug)
    {
      using namespace AudioEngine;
      using namespace AudioKeyFrameType;
      using namespace AudioMetaData;
      
      // Add States
      const auto* states = audioKeyframe->states();
      if (nullptr != states) {
        for (const auto& aState : *states) {
          const auto groupId = static_cast<GameState::StateGroupType>(aState->stateGroupId());
          const auto stateId = static_cast<GameState::GenericState>(aState->stateId());
          if ((GameState::StateGroupType::Invalid == groupId) || (GameState::GenericState::Invalid == stateId)) {
            PRINT_NAMED_ERROR("RobotAudioKeyFrame.SetMembersFromFlatBuf.InvalidGameState",
                              "'%s' @ %i ms : Has an invalid stateGroupId (%i) or stateId (%i)",
                              animNameDebug.c_str(), _triggerTime_ms, groupId, stateId);
            // Move to next State
            continue;
          }
          Result addResult = AddAudioRef(AudioStateRef(groupId, stateId));
          if(addResult != RESULT_OK) {
            return addResult;
          }
        }
      }
      
      // Add Switches
      const auto* switches = audioKeyframe->switches();
      if (nullptr != switches) {
        for (const auto& aSwitch : *switches) {
          const auto groupId = static_cast<SwitchState::SwitchGroupType>(aSwitch->switchGroupId());
          const auto stateId = static_cast<SwitchState::GenericSwitch>(aSwitch->stateId());
          if ((SwitchState::SwitchGroupType::Invalid == groupId) || (SwitchState::GenericSwitch::Invalid == stateId)) {
            PRINT_NAMED_ERROR("RobotAudioKeyFrame.SetMembersFromFlatBuf.InvalidSwitchState",
                              "'%s' @ %i ms : Has an invalid switchGroupId (%i) or stateId (%i)",
                              animNameDebug.c_str(), _triggerTime_ms, groupId, stateId);
            // Move to next Switch
            continue;
          }
          Result addResult = AddAudioRef(AudioSwitchRef(groupId, stateId, kAnimationGameObject));
          if(addResult != RESULT_OK) {
            return addResult;
          }
        }
      }
      
      // Add Parameters
      const auto* parameters = audioKeyframe->parameters();
      if (nullptr != parameters) {
        for (const auto& aParameter : *parameters) {
          const auto parameterId = static_cast<GameParameter::ParameterType>(aParameter->parameterID());
          if (GameParameter::ParameterType::Invalid == parameterId) {
            PRINT_NAMED_ERROR("RobotAudioKeyFrame.SetMembersFromFlatBuf.InvalidParameter",
                              "'%s' @ %i ms : Has an invalid parameterID", animNameDebug.c_str(), _triggerTime_ms);
            // Move to next Parameter
            continue;
          }
          auto parameterRef = AudioParameterRef(parameterId,
                                                aParameter->value(),
                                                aParameter->time_ms(),
                                                static_cast<Multiplexer::CurveType>(aParameter->curveType()),
                                                kAnimationGameObject);
          Result addResult = AddAudioRef(std::move(parameterRef));
          if(addResult != RESULT_OK) {
            return addResult;
          }
        }
      }
      
      // Add Event Groups
      // Note: Add events last to the AudioRef list, they need to be posted last when performing a key frame
      const auto* eventGroups = audioKeyframe->eventGroups();
      if (nullptr != eventGroups) {
        // Loop through groups
        for (const auto& aGroup : *eventGroups) {
          // Create event group
          AudioEventGroupRef anEventGroup(kAnimationGameObject);
          const auto* eventIds      = aGroup->eventIds();
          const auto* volumes       = aGroup->volumes();
          const auto* probabilities = aGroup->probabilities();
          
          if ((eventIds->size() != volumes->size()) || (eventIds->size() != probabilities->size())) {
            PRINT_NAMED_ERROR("RobotAudioKeyFrame.SetMembersFromFlatBuf.InvlaidEventGroup",
                              "'%s' @ %i ms : EventIds, Volumes & Probabilities don't have the same count",
                              animNameDebug.c_str(), _triggerTime_ms);
            // Move to next Event Group
            continue;
          }
          
          // Loop through events in group
          for (uint idx = 0; idx < eventIds->size();  ++idx) {
            const auto& anEventId = static_cast<GameEvent::GenericEvent>(eventIds->Get(idx));
            if (GameEvent::GenericEvent::Invalid == anEventId) {
              PRINT_NAMED_ERROR("RobotAudioKeyFrame.SetMembersFromFlatBuf.InvalidGameEvent",
                                "'%s' @ %i ms : Has an invalid audio event", animNameDebug.c_str(), _triggerTime_ms);
              // Move on to next Event
              continue;
            }
            anEventGroup.AddEvent(anEventId, volumes->Get(idx), probabilities->Get(idx));
          }
          
          if (anEventGroup.Events.empty()) {
            PRINT_NAMED_ERROR("RobotAudioKeyFrame.SetMembersFromFlatBuf.InvalidGameEventGroup",
                              "'%s' @ %i ms : Has an empty event group", animNameDebug.c_str(), _triggerTime_ms);
            return  RESULT_FAIL;
          }
          Result addResult = AddAudioRef(std::move(anEventGroup));
          if (addResult != RESULT_OK) {
            return addResult;
          }
        }
      }
      return RESULT_OK;
    }
    
#pragma mark -
#pragma mark EventKeyFrame
    //
    // EventKeyFrame
    //
    
    #if CAN_STREAM
      RobotInterface::EngineToRobot* EventKeyFrame::GetStreamMessage(const TimeStamp_t timeSinceAnimStart_ms) const
      {
        // This function isn't actually used. Instead GetAnimEvent() is used by animationStreamer.
        DEV_ASSERT(false, "EventKeyFrame.GetStreamMessage.ShouldntCallThis");
        return nullptr;
      }
    #endif

    Result EventKeyFrame::DefineFromFlatBuf(const CozmoAnim::Event* eventKeyframe, const std::string& animNameDebug)
    {
      DEV_ASSERT(eventKeyframe != nullptr, "EventKeyFrame.DefineFromFlatBuf.NullAnim");
      SafeNumericCast(eventKeyframe->triggerTime_ms(), _triggerTime_ms, animNameDebug.c_str());
      Result lastResult = SetMembersFromFlatBuf(eventKeyframe, animNameDebug);
      return lastResult;
    }
    
    Result EventKeyFrame::SetMembersFromFlatBuf(const CozmoAnim::Event* eventKeyframe, const std::string& animNameDebug)
    {
      // Convert event_id string to AnimEvent enum
      const std::string& eventStr = eventKeyframe->event_id()->str();
      AnimEvent e = AnimEventFromString(eventStr.c_str());
      if (e == AnimEvent::Count) {
        PRINT_NAMED_WARNING("EventKeyFrame.UnrecognizedEventName", "%s", eventStr.c_str());
        return RESULT_FAIL;
      }
      _event_id = e;
      return RESULT_OK;
    }

    Result EventKeyFrame::SetMembersFromJson(const Json::Value &jsonRoot, const std::string& animNameDebug)
    {
      // Convert event_id string to AnimEvent enum
      if (!jsonRoot.isMember("event_id")) {
        PRINT_NAMED_WARNING("EventKeyFrame.NoEventIDFound", "");
        return RESULT_FAIL;
      } else {
        if(jsonRoot["event_id"].isString()) {
          const std::string& eventStr = jsonRoot["event_id"].asString();
          AnimEvent e = AnimEventFromString(eventStr.c_str());
          if (e == AnimEvent::Count) {
            PRINT_NAMED_WARNING("EventKeyFrame.UnrecognizedEventName", "%s", eventStr.c_str());
            return RESULT_FAIL;
          }
          _event_id = e;
        } else {
          PRINT_NAMED_WARNING("EventKeyFrame.EventIDNotString", "");
          return RESULT_FAIL;
        }
      }

      return RESULT_OK;
    }
    
#pragma mark -
#pragma mark BackpackLightsKeyFrame
    
    BackpackLightsKeyFrame::BackpackLightsKeyFrame()
    {
      _streamMsg.layer = 1; // 1 == BPL_ANIMATION
    }
    
    Result BackpackLightsKeyFrame::DefineFromFlatBuf(CozmoAnim::BackpackLights* backpackKeyframe, const std::string& animNameDebug)
    {
      DEV_ASSERT(backpackKeyframe != nullptr, "BackpackLightsKeyFrame.DefineFromFlatBuf.NullAnim");
      SafeNumericCast(backpackKeyframe->triggerTime_ms(), _triggerTime_ms, animNameDebug.c_str());
      SafeNumericCast(backpackKeyframe->durationTime_ms(), _keyframeActiveDuration_ms, animNameDebug.c_str());
      Result lastResult = SetMembersFromFlatBuf(backpackKeyframe, animNameDebug);
      return lastResult;
    }

    Result BackpackLightsKeyFrame::SetMembersFromFlatBuf(CozmoAnim::BackpackLights* backpackKeyframe, const std::string& animNameDebug)
    {
      // TODO: IMPLEMENT THIS METHOD

      PRINT_NAMED_ERROR("BackpackLightsKeyFrame::SetMembersFromFlatBuf",
                        "The BackpackLightsKeyFrame::SetMembersFromFlatBuf() method still needs to be implemented");

      return RESULT_OK;
    }

    Result BackpackLightsKeyFrame::SetMembersFromJson(const Json::Value &jsonRoot, const std::string& animNameDebug)
    {
      ColorRGBA color;
     
      // Special helper macro for getting the LED colors out of the Json and
      // store them directly in the streamMsg
#define GET_COLOR_FROM_JSON(__NAME__, __LED_NAME__) do {             \
if(!JsonTools::GetColorOptional(jsonRoot, QUOTE(__NAME__), color)) { \
  PRINT_NAMED_ERROR("BackpackLightsKeyFrame.SetMembersFromJson",        \
                    "%s: Failed to get '%s' LED color from Json file", \
                    animNameDebug.c_str(), QUOTE(__NAME__));            \
  return RESULT_FAIL;                                                   \
}                                                                       \
_streamMsg.lights[__LED_NAME__].onColor = color; \
_streamMsg.lights[__LED_NAME__].offColor = color; \
_streamMsg.lights[__LED_NAME__].onPeriod_ms = 0; \
_streamMsg.lights[__LED_NAME__].offPeriod_ms = 0; \
_streamMsg.lights[__LED_NAME__].transitionOnPeriod_ms = 0; \
_streamMsg.lights[__LED_NAME__].transitionOffPeriod_ms = 0; \
_streamMsg.lights[__LED_NAME__].offset_ms = 0; } while(0)

      GET_COLOR_FROM_JSON(Front,  (int)LEDId::LED_BACKPACK_FRONT);
      GET_COLOR_FROM_JSON(Middle, (int)LEDId::LED_BACKPACK_MIDDLE);
      GET_COLOR_FROM_JSON(Back,   (int)LEDId::LED_BACKPACK_BACK);
      
      GET_MEMBER_FROM_JSON_AND_STORE_IN(jsonRoot, durationTime_ms, keyframeActiveDuration_ms);

      return RESULT_OK;
    }
  
    #if CAN_STREAM
      RobotInterface::EngineToRobot* BackpackLightsKeyFrame::GetStreamMessage(const TimeStamp_t timeSinceAnimStart_ms) const
      {
        if(!IsFirstKeyframeTick(timeSinceAnimStart_ms)){
          return nullptr;
        }
        return new RobotInterface::EngineToRobot(_streamMsg);
      }
    #endif
    
#pragma mark -
#pragma mark BodyMotionKeyFrame
    
    BodyMotionKeyFrame::BodyMotionKeyFrame()
    {
      _streamMsg.accel = 0.f;
      
      // The stop message should command the wheel speeds to zero immediately, so command
      // zero velocity and 'infinite' radius
      _stopMsg.speed = 0.f;
      _stopMsg.accel = 0.f;
      _stopMsg.curvatureRadius_mm = std::numeric_limits<decltype(_stopMsg.curvatureRadius_mm)>::max();
    }
    
    BodyMotionKeyFrame::BodyMotionKeyFrame(s16 speed, s16 curvatureRadius_mm, s32 duration_ms)
    : BodyMotionKeyFrame()
    {
      bool isPointTurn = curvatureRadius_mm == 0;
      
      _keyframeActiveDuration_ms = duration_ms;
      _streamMsg.speed = isPointTurn ? DEG_TO_RAD(speed) : speed;
      _streamMsg.curvatureRadius_mm = curvatureRadius_mm;
      _streamMsg.accel = isPointTurn ? 50.f : 0.f;  // 50 is what has been used on V1
    }
    
    void BodyMotionKeyFrame::CheckRotationSpeed(const std::string& animNameDebug)
    {
      // Check that speed is valid
      if (std::abs(_streamMsg.speed) > MAX_BODY_ROTATION_SPEED_DEG_PER_SEC) {
        PRINT_CH_DEBUG("Animations", "BodyMotionKeyFrame.CheckRotationSpeed.PointTurnSpeedExceedsLimit",
                       "%s: PointTurn speed %f deg/s exceeds limit of %f deg/s. Clamping",
                       animNameDebug.c_str(), std::abs(_streamMsg.speed), MAX_BODY_ROTATION_SPEED_DEG_PER_SEC);
        _streamMsg.speed = CLIP((f32)_streamMsg.speed,
                                -MAX_BODY_ROTATION_SPEED_DEG_PER_SEC,
                                MAX_BODY_ROTATION_SPEED_DEG_PER_SEC);
      }
    }

    void BodyMotionKeyFrame::CheckStraightSpeed(const std::string& animNameDebug)
    {
      // Check that speed is valid
      if (std::abs(_streamMsg.speed) > MAX_WHEEL_SPEED_MMPS) {
        PRINT_CH_DEBUG("Animations", "BodyMotionKeyFrame.CheckStraightSpeed.StraightSpeedExceedsLimit",
                       "%s: Speed %f mm/s exceeds limit of %f mm/s. Clamping",
                       animNameDebug.c_str(), std::abs(_streamMsg.speed), MAX_WHEEL_SPEED_MMPS);
        _streamMsg.speed = CLIP((f32)_streamMsg.speed, -MAX_WHEEL_SPEED_MMPS, MAX_WHEEL_SPEED_MMPS);
      }
    }

    void BodyMotionKeyFrame::CheckTurnSpeed(const std::string& animNameDebug)
    {
      // Check that speed is valid
      // NOTE: This should actually be checking the speed of the outer wheel
      //       when driving at the given curvature, but not exactly sure what
      //       speed limit should look like between straight and point turns so
      //       just using straight limit for now as a sanity check.
      if (std::abs(_streamMsg.speed) > MAX_WHEEL_SPEED_MMPS) {
        PRINT_CH_DEBUG("Animations", "BodyMotionKeyFrame.CheckTurnSpeed.ArcSpeedExceedsLimit",
                       "%s: Speed %f mm/s exceeds limit of %f mm/s. Clamping",
                       animNameDebug.c_str(), std::abs(_streamMsg.speed), MAX_WHEEL_SPEED_MMPS);
        _streamMsg.speed = CLIP((f32)_streamMsg.speed, -MAX_WHEEL_SPEED_MMPS, MAX_WHEEL_SPEED_MMPS);
      }
    }

    Result BodyMotionKeyFrame::DefineFromFlatBuf(const CozmoAnim::BodyMotion* bodyKeyframe, const std::string& animNameDebug)
    {
      DEV_ASSERT(bodyKeyframe != nullptr, "BodyMotionKeyFrame.DefineFromFlatBuf.NullAnim");
      SafeNumericCast(bodyKeyframe->triggerTime_ms(), _triggerTime_ms, animNameDebug.c_str());
      Result lastResult = SetMembersFromFlatBuf(bodyKeyframe, animNameDebug);
      return lastResult;
    }

    Result BodyMotionKeyFrame::SetMembersFromFlatBuf(const CozmoAnim::BodyMotion* bodyKeyframe, const std::string& animNameDebug)
    {
      Result res = RESULT_OK;
      
      SafeNumericCast(bodyKeyframe->durationTime_ms(), _keyframeActiveDuration_ms, animNameDebug.c_str());
      SafeNumericCast(bodyKeyframe->speed(),           _streamMsg.speed,   animNameDebug.c_str());

      const std::string& radiusStr = bodyKeyframe->radius_mm()->str();
      if (has_any_digits(radiusStr)) {
        SafeNumericCast(std::atoi(radiusStr.c_str()), _streamMsg.curvatureRadius_mm, animNameDebug.c_str());
        CheckTurnSpeed(animNameDebug);
        if (_streamMsg.curvatureRadius_mm == 0) {
          _streamMsg.accel = 50.f;  // 50 was used in V1 for point turns
        }
      } else {
        res = ProcessRadiusString(radiusStr, animNameDebug);
      }

      return res;
    }

    Result BodyMotionKeyFrame::SetMembersFromJson(const Json::Value &jsonRoot, const std::string& animNameDebug)
    {
      Result res = RESULT_OK;
      
      GET_MEMBER_FROM_JSON_AND_STORE_IN(jsonRoot, durationTime_ms, keyframeActiveDuration_ms);
      GET_MEMBER_FROM_JSON_AND_STORE_IN(jsonRoot, speed, streamMsg.speed);

      if(!jsonRoot.isMember("radius_mm")) {
        PRINT_NAMED_ERROR("BodyMotionKeyFrame.SetMembersFromJson.MissingRadius",
                          "%s: Missing 'radius_mm' field.",
                          animNameDebug.c_str());
        return RESULT_FAIL;
      } else if(jsonRoot["radius_mm"].isString()) {
        const std::string& radiusStr = jsonRoot["radius_mm"].asString();
        res = ProcessRadiusString(radiusStr, animNameDebug);
      } else {
        GET_MEMBER_FROM_JSON_AND_STORE_IN(jsonRoot, radius_mm, streamMsg.curvatureRadius_mm);
        CheckTurnSpeed(animNameDebug);
        if (_streamMsg.curvatureRadius_mm == 0) {
          _streamMsg.accel = 50.f;  // 50 was used in V1 for point turns
        }
      }
      
      return res;
    }
    
    Result BodyMotionKeyFrame::ProcessRadiusString(const std::string& radiusStr, const std::string& animNameDebug)
    {
      if(radiusStr == "TURN_IN_PLACE" || radiusStr == "POINT_TURN") {
        _streamMsg.curvatureRadius_mm = 0;
        _streamMsg.accel = 50.f;  // 50 is what was used on V1 for point turns
        CheckRotationSpeed(animNameDebug);
        
        // Convert speed to radians from degrees
        _streamMsg.speed = DEG_TO_RAD(_streamMsg.speed);
        
      } else if(radiusStr == "STRAIGHT") {
        _streamMsg.curvatureRadius_mm = std::numeric_limits<s16>::max();
        _streamMsg.accel = 0.f;   // 0 is what was used on V1 for non point turns
        CheckStraightSpeed(animNameDebug);
      } else {
        PRINT_NAMED_ERROR("BodyMotionKeyFrame.BadRadiusString",
                          "%s: Unrecognized string for 'radius_mm' field: %s",
                          animNameDebug.c_str(),
                          radiusStr.c_str());
        return RESULT_FAIL;
      }
      return RESULT_OK;
    }

    #if CAN_STREAM
      RobotInterface::EngineToRobot* BodyMotionKeyFrame::GetStreamMessage(const TimeStamp_t timeSinceAnimStart_ms) const
      {
        //PRINT_NAMED_INFO("BodyMotionKeyFrame.GetStreamMessage",
        //                 "currentTime=%d, duration=%d\n", timeSinceAnimStart_ms, _keyframeActiveDuration_ms);
        if(IsFirstKeyframeTick(timeSinceAnimStart_ms)) {
          // Send the motion command at the beginning
          return new RobotInterface::EngineToRobot(_streamMsg);
        } else if(_enableStopMessage && GetTimeSinceTrigger(timeSinceAnimStart_ms) >= _keyframeActiveDuration_ms) {
          // Send a stop command when the duration has passed
          return new RobotInterface::EngineToRobot(_stopMsg);
        } else {
          // Do nothing in the middle or if no done message is required.
          return nullptr;
        }
      }
    #endif
    
    TimeStamp_t BodyMotionKeyFrame::GetKeyframeDuration_ms() const 
    {
      // TODO: Kevin K. - add an animation unit test to assert this
      // If enable stop message is false there's another body motion keyframe that wants to run at the exact 
      // timestamp of the internal active duration
      // Therefore, if there's no stop message we want to finish body motion while there's still "motion duration" left
      return _enableStopMessage ? _keyframeActiveDuration_ms + ANIM_TIME_STEP_MS : _keyframeActiveDuration_ms;
    }

    
#pragma mark -
#pragma mark RecordHeadingKeyFrame
    
    RecordHeadingKeyFrame::RecordHeadingKeyFrame()
    {
    }
    
    Result RecordHeadingKeyFrame::DefineFromFlatBuf(const CozmoAnim::RecordHeading* recordHeadingKeyframe, const std::string& animNameDebug)
    {
      DEV_ASSERT(recordHeadingKeyframe != nullptr, "RecordedHeadingKeyFrame.DefineFromFlatBuf.NullAnim");
      SafeNumericCast(recordHeadingKeyframe->triggerTime_ms(), _triggerTime_ms, animNameDebug.c_str());
      Result lastResult = SetMembersFromFlatBuf(recordHeadingKeyframe, animNameDebug);
      return lastResult;
    }
    
    Result RecordHeadingKeyFrame::SetMembersFromFlatBuf(const CozmoAnim::RecordHeading* recordHeadingKeyframe, const std::string& animNameDebug)
    {
      return RESULT_OK;
    }
    
    Result RecordHeadingKeyFrame::SetMembersFromJson(const Json::Value &jsonRoot, const std::string& animNameDebug)
    {
      return RESULT_OK;
    }
    
    #if CAN_STREAM
      RobotInterface::EngineToRobot* RecordHeadingKeyFrame::GetStreamMessage(const TimeStamp_t timeSinceAnimStart_ms) const
      {
        if(!IsFirstKeyframeTick(timeSinceAnimStart_ms)){
          return nullptr;
        }
        return new RobotInterface::EngineToRobot(_streamMsg);
      }
    #endif
    
    
#pragma mark -
#pragma mark TurnToRecordedHeadingKeyFrame
    
    TurnToRecordedHeadingKeyFrame::TurnToRecordedHeadingKeyFrame()
    {
    }
    
    TurnToRecordedHeadingKeyFrame::TurnToRecordedHeadingKeyFrame(s16 offset_deg,
                                                                 s16 speed_degPerSec,
                                                                 s16 accel_degPerSec2,
                                                                 s16 decel_degPerSec2,
                                                                 u16 tolerance_deg,
                                                                 u16 numHalfRevs,
                                                                 bool useShortestDir,
                                                                 s32 duration_ms)
    : TurnToRecordedHeadingKeyFrame()
    {
      _streamMsg.offset_deg = offset_deg;
      _streamMsg.speed_degPerSec = speed_degPerSec;
      _streamMsg.accel_degPerSec2 = accel_degPerSec2;
      _streamMsg.decel_degPerSec2 = decel_degPerSec2;
      _streamMsg.tolerance_deg = tolerance_deg;
      _streamMsg.numHalfRevs = numHalfRevs;
      _streamMsg.useShortestDir = useShortestDir;
    }
    
    void TurnToRecordedHeadingKeyFrame::CheckRotationSpeed(const std::string& animNameDebug)
    {
      // Check that speed is valid
      if (std::abs(_streamMsg.speed_degPerSec) > MAX_BODY_ROTATION_SPEED_DEG_PER_SEC) {
        PRINT_CH_DEBUG("Animations", "TurnToRecordedHeadingKeyFrame.CheckRotationSpeed.PointTurnSpeedExceedsLimit",
                       "%s: PointTurn speed %d deg/s exceeds limit of %f deg/s. Clamping",
                       animNameDebug.c_str(),
                       std::abs(_streamMsg.speed_degPerSec),
                       MAX_BODY_ROTATION_SPEED_DEG_PER_SEC);
        _streamMsg.speed_degPerSec = CLIP((f32)_streamMsg.speed_degPerSec,
                                          -MAX_BODY_ROTATION_SPEED_DEG_PER_SEC,
                                          MAX_BODY_ROTATION_SPEED_DEG_PER_SEC);
      }
      
      // Check that accel/decel are within range
      if (std::abs(_streamMsg.accel_degPerSec2) > MAX_BODY_ROTATION_ACCEL_DEG_PER_SEC2) {
        PRINT_CH_DEBUG("Animations", "TurnToRecordedHeadingKeyFrame.CheckRotationAccel.PointTurnAccelExceedsLimit",
                       "%s: PointTurn accel %d deg/s^2 exceeds limit of %f deg/s^2. Clamping",
                       animNameDebug.c_str(),
                       std::abs(_streamMsg.accel_degPerSec2),
                       MAX_BODY_ROTATION_ACCEL_DEG_PER_SEC2);
        _streamMsg.accel_degPerSec2 = CLIP((f32)_streamMsg.accel_degPerSec2,
                                          -MAX_BODY_ROTATION_ACCEL_DEG_PER_SEC2,
                                          MAX_BODY_ROTATION_ACCEL_DEG_PER_SEC2);
      }
      if (std::abs(_streamMsg.decel_degPerSec2) > MAX_BODY_ROTATION_ACCEL_DEG_PER_SEC2) {
        PRINT_CH_DEBUG("Animations", "TurnToRecordedHeadingKeyFrame.CheckRotationAccel.PointTurnDecelExceedsLimit",
                       "%s: PointTurn decel %d deg/s^2 exceeds limit of %f deg/s^2. Clamping",
                       animNameDebug.c_str(),
                       std::abs(_streamMsg.decel_degPerSec2),
                       MAX_BODY_ROTATION_ACCEL_DEG_PER_SEC2);
        _streamMsg.decel_degPerSec2 = CLIP((f32)_streamMsg.decel_degPerSec2,
                                           -MAX_BODY_ROTATION_ACCEL_DEG_PER_SEC2,
                                           MAX_BODY_ROTATION_ACCEL_DEG_PER_SEC2);
      }
    }
    
    Result TurnToRecordedHeadingKeyFrame::DefineFromFlatBuf(const CozmoAnim::TurnToRecordedHeading* turnToRecordedHeadingKeyframe, const std::string& animNameDebug)
    {
      DEV_ASSERT(turnToRecordedHeadingKeyframe != nullptr, "TurnToRecordedHeadingKeyFrame.DefineFromFlatBuf.NullAnim");
      SafeNumericCast(turnToRecordedHeadingKeyframe->triggerTime_ms(), _triggerTime_ms, animNameDebug.c_str());
      Result lastResult = SetMembersFromFlatBuf(turnToRecordedHeadingKeyframe, animNameDebug);
      return lastResult;
    }
    
    Result TurnToRecordedHeadingKeyFrame::SetMembersFromFlatBuf(const CozmoAnim::TurnToRecordedHeading* turnToRecordedHeadingKeyframe, const std::string& animNameDebug)
    {
      const char* const dbgName = animNameDebug.c_str();
      SafeNumericCast(turnToRecordedHeadingKeyframe->durationTime_ms(),  _keyframeActiveDuration_ms,        dbgName);
      SafeNumericCast(turnToRecordedHeadingKeyframe->offset_deg(),       _streamMsg.offset_deg,       dbgName);
      SafeNumericCast(turnToRecordedHeadingKeyframe->speed_degPerSec(),  _streamMsg.speed_degPerSec,  dbgName);
      SafeNumericCast(turnToRecordedHeadingKeyframe->accel_degPerSec2(), _streamMsg.accel_degPerSec2, dbgName);
      SafeNumericCast(turnToRecordedHeadingKeyframe->decel_degPerSec2(), _streamMsg.decel_degPerSec2, dbgName);
      SafeNumericCast(turnToRecordedHeadingKeyframe->tolerance_deg(),    _streamMsg.tolerance_deg,    dbgName);
      SafeNumericCast(turnToRecordedHeadingKeyframe->numHalfRevs(),      _streamMsg.numHalfRevs,      dbgName);
      _streamMsg.useShortestDir = turnToRecordedHeadingKeyframe->useShortestDir();
      
      CheckRotationSpeed(animNameDebug);
      
      return RESULT_OK;
    }
    
    Result TurnToRecordedHeadingKeyFrame::SetMembersFromJson(const Json::Value &jsonRoot, const std::string& animNameDebug)
    {
      GET_MEMBER_FROM_JSON_AND_STORE_IN(jsonRoot, durationTime_ms,  keyframeActiveDuration_ms);
      GET_MEMBER_FROM_JSON_AND_STORE_IN(jsonRoot, offset_deg,       streamMsg.offset_deg);
      GET_MEMBER_FROM_JSON_AND_STORE_IN(jsonRoot, speed_degPerSec,  streamMsg.speed_degPerSec);
      GET_MEMBER_FROM_JSON_AND_STORE_IN(jsonRoot, accel_degPerSec2, streamMsg.accel_degPerSec2);
      GET_MEMBER_FROM_JSON_AND_STORE_IN(jsonRoot, decel_degPerSec2, streamMsg.decel_degPerSec2);
      GET_MEMBER_FROM_JSON_AND_STORE_IN(jsonRoot, tolerance_deg,    streamMsg.tolerance_deg);
      GET_MEMBER_FROM_JSON_AND_STORE_IN(jsonRoot, numHalfRevs,      streamMsg.numHalfRevs);
      GET_MEMBER_FROM_JSON_AND_STORE_IN(jsonRoot, useShortestDir,   streamMsg.useShortestDir);

      CheckRotationSpeed(animNameDebug);

      return RESULT_OK;
    }
    
    #if CAN_STREAM
      RobotInterface::EngineToRobot* TurnToRecordedHeadingKeyFrame::GetStreamMessage(const TimeStamp_t timeSinceAnimStart_ms) const
      {
        if(!IsFirstKeyframeTick(timeSinceAnimStart_ms)){
          return nullptr;
        }
        return new RobotInterface::EngineToRobot(_streamMsg);
      }
    #endif

  } // namespace Vector
} // namespace Anki
