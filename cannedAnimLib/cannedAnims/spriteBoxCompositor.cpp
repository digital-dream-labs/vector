/**
 * File: spriteBoxCompositor.cpp
 *
 * Authors: Sam Russell
 * Created: 2019-03-01
 *
 * Description:
 *    Class for storing SpriteBoxKeyFrames and constructing CompositeImages from them
 *
 * Copyright: Anki, Inc. 2019
 *
 **/

#include "cannedAnimLib/cannedAnims/spriteBoxCompositor.h"

#include "anki/cozmo/shared/cozmoConfig.h"

#include "cannedAnimLib/baseTypes/cozmo_anim_generated.h"
#include "coretech/common/engine/jsonTools.h"
#include "coretech/vision/shared/compositeImage/compositeImage.h"
#include "coretech/vision/shared/spriteCache/spriteCache.h"
#include "coretech/vision/shared/spriteSequence/spriteSequenceContainer.h"

#define LOG_CHANNEL "Animations"

namespace Anki{
namespace Vector{
namespace Animations{

namespace{

const char* kSpriteBoxNameKey = "spriteBoxName";
const char* kTriggerTimeKey   = "triggerTime_ms";
const char* kAssetNameKey     = "assetName";
const char* kLayerNameKey     = "layer";
const char* kRenderMethodKey  = "renderMethod";
const char* kSpriteSeqEndKey  = "spriteSeqEndType";
const char* kAlphaKey         = "alpha";
const char* kXPosKey          = "xPos";
const char* kYPosKey          = "yPos";
const char* kWidthKey         = "width";
const char* kHeightKey        = "height";

// Legacy support
const char* kFaceKeyFrameAssetNameKey = "animName";

constexpr TimeStamp_t kOverrideIndefinitely = std::numeric_limits<TimeStamp_t>::infinity();

// Fwd declare local helper
bool GetFrameFromSpriteSequenceHelper(const Vision::SpriteSequence& sequence,
                                      const uint16_t frameIdx,
                                      const Vision::SpriteSeqEndType& spriteSeqEndType,
                                      Vision::SpriteHandle& outSpriteHandle);

const Vision::SpriteBox kFullFaceSpriteBox {
  100.0f,
  0,
  0,
  FACE_DISPLAY_WIDTH,
  FACE_DISPLAY_HEIGHT,
  Vision::SpriteBoxName::SpriteBox_40,
  Vision::LayerName::Layer_10,
  Vision::SpriteRenderMethod::RGBA,
  0
};

}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// SpriteBoxCompositor
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
SpriteBoxCompositor::SpriteBoxCompositor(const SpriteBoxCompositor& other)
: _lastKeyFrameTime_ms(other._lastKeyFrameTime_ms)
, _advanceTime_ms(0)
, _faceImageOverride(nullptr)
, _faceImageOverrideEndTime_ms(0)
, _overrideAllSpritesToEyeHue(false)
{
  if(nullptr != other._spriteBoxMap){
    _spriteBoxMap = std::make_unique<SpriteBoxMap>(*other._spriteBoxMap);
  } else {
    _spriteBoxMap.reset();
  }
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
SpriteBoxCompositor::SpriteBoxCompositor()
: _lastKeyFrameTime_ms(0)
, _advanceTime_ms(0)
, _faceImageOverride(nullptr)
, _faceImageOverrideEndTime_ms(0)
, _overrideAllSpritesToEyeHue(false)
{
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
Result SpriteBoxCompositor::AddKeyFrame(const CozmoAnim::SpriteBox* spriteBox)
{
  if(nullptr == spriteBox){
    return RESULT_FAIL;
  }

  Vision::SpriteBoxKeyFrame newKeyFrame;
  newKeyFrame.triggerTime_ms = (u32)spriteBox->triggerTime_ms();
  newKeyFrame.assetID = Vision::SpritePathMap::GetAssetID(spriteBox->assetName()->str());
  newKeyFrame.spriteSeqEndType = Vision::SpriteSeqEndTypeFromString(spriteBox->spriteSeqEndType()->str());
  newKeyFrame.spriteBox.alpha = spriteBox->alpha();
  newKeyFrame.spriteBox.name = Vision::SpriteBoxNameFromString(spriteBox->spriteBoxName()->str());
  newKeyFrame.spriteBox.layer = Vision::LayerNameFromString(spriteBox->layer()->str());
  newKeyFrame.spriteBox.xPos = spriteBox->xPos();
  newKeyFrame.spriteBox.yPos = spriteBox->yPos();
  newKeyFrame.spriteBox.width = spriteBox->width();
  newKeyFrame.spriteBox.height = spriteBox->height();

  // Handle legacy "CustomHue" method as EyeColor
  std::string renderMethodString = spriteBox->renderMethod()->str();
  renderMethodString = ("CustomHue" == renderMethodString) ? "EyeColor" : renderMethodString;
  newKeyFrame.spriteBox.renderMethod = Vision::SpriteRenderMethodFromString(renderMethodString);

  return AddKeyFrameInternal(std::move(newKeyFrame));
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
Result SpriteBoxCompositor::AddKeyFrame(const Json::Value& json, const std::string& animName)
{
  Vision::SpriteBoxKeyFrame newKeyFrame;
  newKeyFrame.triggerTime_ms = JsonTools::ParseInt32(json, kTriggerTimeKey, animName);
  newKeyFrame.assetID = Vision::SpritePathMap::GetAssetID(JsonTools::ParseString(json, kAssetNameKey, animName));
  newKeyFrame.spriteSeqEndType = Vision::SpriteSeqEndTypeFromString(JsonTools::ParseString(json,
                                                                                           kSpriteSeqEndKey,
                                                                                           animName));
  newKeyFrame.spriteBox.name = Vision::SpriteBoxNameFromString(JsonTools::ParseString(json,
                                                                                      kSpriteBoxNameKey,
                                                                                      animName));
  newKeyFrame.spriteBox.alpha = JsonTools::ParseInt32(json, kAlphaKey, animName);
  newKeyFrame.spriteBox.layer = Vision::LayerNameFromString(JsonTools::ParseString(json, kLayerNameKey, animName));
  newKeyFrame.spriteBox.xPos = JsonTools::ParseInt32(json, kXPosKey, animName);
  newKeyFrame.spriteBox.yPos = JsonTools::ParseInt32(json, kYPosKey, animName);
  newKeyFrame.spriteBox.width = JsonTools::ParseInt32(json, kWidthKey, animName);
  newKeyFrame.spriteBox.height = JsonTools::ParseInt32(json, kHeightKey, animName);

  // Handle legacy "CustomHue" method as EyeColor
  std::string renderMethodString = JsonTools::ParseString(json, kRenderMethodKey, animName);
  renderMethodString = ("CustomHue" == renderMethodString) ? "EyeColor" : renderMethodString;
  newKeyFrame.spriteBox.renderMethod = Vision::SpriteRenderMethodFromString(renderMethodString);

  return AddKeyFrameInternal(std::move(newKeyFrame));
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
Result SpriteBoxCompositor::AddFullFaceSpriteSeq(const CozmoAnim::FaceAnimation* faceAnimationKeyFrame,
                                                 const Vision::SpriteSequenceContainer& spriteSeqContainer)
{
  return AddFullFaceSpriteSeqInternal(Vision::SpritePathMap::GetAssetID(faceAnimationKeyFrame->animName()->str()),
                                      (u32)faceAnimationKeyFrame->triggerTime_ms(),
                                      spriteSeqContainer);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
Result SpriteBoxCompositor::AddFullFaceSpriteSeq(const Json::Value& json,
                                                 const Vision::SpriteSequenceContainer& spriteSeqContainer,
                                                 const std::string& animName)
{
  return AddFullFaceSpriteSeqInternal(
    Vision::SpritePathMap::GetAssetID(JsonTools::ParseString(json, kFaceKeyFrameAssetNameKey, animName)),
    JsonTools::ParseUInt32(json, kTriggerTimeKey, animName),
    spriteSeqContainer);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
Result SpriteBoxCompositor::AddFullFaceSpriteSeqInternal(const Vision::SpritePathMap::AssetID assetID,
                                                         const TimeStamp_t triggerTime_ms,
                                                         const Vision::SpriteSequenceContainer& spriteSeqContainer)
{
  Vision::SpriteBoxKeyFrame startKeyFrame;
  startKeyFrame.triggerTime_ms = triggerTime_ms;
  startKeyFrame.assetID = assetID;
  startKeyFrame.spriteSeqEndType = Vision::SpriteSeqEndType::Clear;
  startKeyFrame.spriteBox.alpha = 100;
  startKeyFrame.spriteBox.xPos = 0;
  startKeyFrame.spriteBox.yPos = 0;
  startKeyFrame.spriteBox.width = FACE_DISPLAY_WIDTH;
  startKeyFrame.spriteBox.height = FACE_DISPLAY_HEIGHT;
  startKeyFrame.spriteBox.name = Vision::SpriteBoxName::SpriteBox_40;
  startKeyFrame.spriteBox.layer = Vision::LayerName::Layer_10;
  startKeyFrame.spriteBox.renderMethod = Vision::SpriteRenderMethod::RGBA;

  // SpriteBoxKeyFrames don't have a notion of duration, so the length of the animation is determined by the 
  // triggerTime of the last keyframe in the SpriteBoxCompositor if there are no other keyframes. To make sure 
  // animations with legacy keyframes play all the way through the Sequence we have to deliberately add a clear 
  // keyframe at the end of it. The animation team requested this design and knows to bracket animations with 
  // "end" keyframes going forward. 
  Vision::SpriteBoxKeyFrame clearKeyFrame(startKeyFrame);
  clearKeyFrame.assetID = Vision::SpritePathMap::kClearSpriteBoxID;
  clearKeyFrame.triggerTime_ms += spriteSeqContainer.GetSpriteSequence(assetID)->GetNumFrames() * ANIM_TIME_STEP_MS;

  Result addKeyFrameResult = AddKeyFrameInternal(std::move(startKeyFrame));
  if( Result::RESULT_OK != addKeyFrameResult ){
    return RESULT_FAIL;
  }

  addKeyFrameResult = AddKeyFrameInternal(std::move(clearKeyFrame));
  return addKeyFrameResult;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
Result SpriteBoxCompositor::AddKeyFrameInternal(Vision::SpriteBoxKeyFrame&& spriteBoxKeyFrame)
{
  if(nullptr == _spriteBoxMap){
    _spriteBoxMap = std::make_unique<SpriteBoxMap>();
  }

  // Grab a couple of copies since insertion operations will invoke std::move
  const Vision::SpriteBoxName name = spriteBoxKeyFrame.spriteBox.name;
  const TimeStamp_t triggerTime_ms = spriteBoxKeyFrame.triggerTime_ms;

  auto iter = _spriteBoxMap->find(name);
  if(_spriteBoxMap->end() == iter){
    // This is the first keyframe for this SpriteBoxName. Add a new track.
    auto emplacePair = _spriteBoxMap->emplace(name, SpriteBoxTrack());
    emplacePair.first->second.InsertKeyFrame(std::move(spriteBoxKeyFrame));
  } else {
    auto& track = iter->second;
    if(!track.InsertKeyFrame(std::move(spriteBoxKeyFrame))){
      LOG_ERROR("SpriteBoxCompositor.AddKeyFrame.DuplicateKeyFrame",
                "Attempted to add overlapping keyframe for SpriteBoxName: %s at time: %d ms",
                EnumToString(name),
                triggerTime_ms);
      return Result::RESULT_FAIL;
    }
  }

  if(triggerTime_ms > _lastKeyFrameTime_ms){
    _lastKeyFrameTime_ms = triggerTime_ms;
  }

  return Result::RESULT_OK;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void SpriteBoxCompositor::SetFaceImageOverride(const Vision::SpriteHandle& spriteHandle,
                                               const TimeStamp_t relativeStreamTime_ms,
                                               const TimeStamp_t duration_ms)
{
  _faceImageOverride = spriteHandle;
  _faceImageOverrideEndTime_ms = duration_ms != 0 ? (relativeStreamTime_ms + duration_ms) : kOverrideIndefinitely;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void SpriteBoxCompositor::ClearOverrides()
{
  _faceImageOverride.reset();
  _faceImageOverrideEndTime_ms = 0;
  _overrideAllSpritesToEyeHue = false;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void SpriteBoxCompositor::AddSpriteBoxRemap(const Vision::SpriteBoxName spriteBox,
                                            const Vision::SpritePathMap::AssetID remappedAssetID)
{

  if(Vision::SpritePathMap::kClearSpriteBoxID == remappedAssetID){
    LOG_ERROR("SpriteBoxCompositor.SetAssetRemap.InvalidRemap",
              "kClearSpriteBoxID should not be used in engine. Use kEmptySpriteBoxID instead.");
    return;
  }

  if(IsEmpty()){
    LOG_ERROR("SpriteBoxCompositor.AddSpriteBoxRemap.EmptyCompositor",
              "Attempted to add remap for SpriteBox %s with remapped AssetID %d",
              EnumToString(spriteBox),
              remappedAssetID);
    return;
  }

  auto iter = _spriteBoxMap->find(spriteBox);
  if(_spriteBoxMap->end() == iter){
    LOG_ERROR("SpriteBoxCompositor.AddSpriteBoxRemap.InvalidSpriteBox",
              "Attempted to add remap for invalid SpriteBox %s with remapped AssetID %d",
              EnumToString(spriteBox),
              remappedAssetID);
    return;
  }

  iter->second.SetAssetRemap(remappedAssetID);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void SpriteBoxCompositor::CacheInternalSprites(Vision::SpriteCache* spriteCache)
{
  // TODO: Implement this if we try to enable pre-caching
  LOG_WARNING("SpriteBoxCompositor.CacheInternalSprites.CachingNotSupported",
              "Caching of internal sprites from the SpriteBoxCompositor is currently unsupported");
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void SpriteBoxCompositor::AppendTracks(const SpriteBoxCompositor& other, const TimeStamp_t animOffset_ms)
{
  if(nullptr != other._spriteBoxMap){
    for(const auto& spriteBoxTrackIter : *other._spriteBoxMap){
      for(const auto& spriteBoxKeyFrame : spriteBoxTrackIter.second.GetKeyFrames()){
        Vision::SpriteBoxKeyFrame newKeyFrame = spriteBoxKeyFrame;
        newKeyFrame.triggerTime_ms += animOffset_ms;
        AddKeyFrameInternal(std::move(newKeyFrame));
      }
    }
  }
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void SpriteBoxCompositor::Clear()
{
  _lastKeyFrameTime_ms = 0;
  _advanceTime_ms = 0;
  ClearOverrides();

  if(nullptr != _spriteBoxMap){
    _spriteBoxMap->clear();
    _spriteBoxMap.reset();
  }
};

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
bool SpriteBoxCompositor::IsEmpty() const
{
  return ( (nullptr == _spriteBoxMap) || _spriteBoxMap->empty());
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void SpriteBoxCompositor::AdvanceTrack(const TimeStamp_t& toTime_ms)
{
  _advanceTime_ms = toTime_ms;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void SpriteBoxCompositor::MoveToStart()
{
  _advanceTime_ms = 0;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void SpriteBoxCompositor::ClearUpToCurrent()
{
  if(!IsEmpty()){
    auto spriteBoxTrackIter = _spriteBoxMap->begin();
    while(_spriteBoxMap->end() != spriteBoxTrackIter){
      auto& track = spriteBoxTrackIter->second;
      track.ClearUpToTime(_advanceTime_ms);
      if(track.IsEmpty()){
        spriteBoxTrackIter = _spriteBoxMap->erase(spriteBoxTrackIter);
      } else {
        ++spriteBoxTrackIter;
      }
    }
  }
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
bool SpriteBoxCompositor::HasFramesLeft() const
{
  // If we've been given an override image, keep this animation running as expected by the caller
  if( (nullptr != _faceImageOverride) &&
      ( (kOverrideIndefinitely == _faceImageOverrideEndTime_ms) || 
        (_advanceTime_ms < _faceImageOverrideEndTime_ms) ) ){
    return true;
  }

  // TODO(str): VIC-13519 Linearize Face Rendering
  // Keep tabs on this when working in AnimationStreamer. It currently has to use '<=' in
  // order to have the final frame displayed due to calling locations in AnimationStreamer,
  // but that feels odd since it ends up requiring that we run one frame beyond the last
  // frame of the animation before AnimationStreamer cleans up the animation.
  return (_advanceTime_ms <= _lastKeyFrameTime_ms);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
TimeStamp_t SpriteBoxCompositor::CompareLastFrameTime(const TimeStamp_t lastFrameTime_ms) const
{
  return (lastFrameTime_ms > _lastKeyFrameTime_ms) ? lastFrameTime_ms : _lastKeyFrameTime_ms;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
bool SpriteBoxCompositor::PopulateCompositeImage(Vision::SpriteCache& spriteCache,
                                                 Vision::SpriteSequenceContainer& spriteSeqContainer,
                                                 TimeStamp_t timeSinceAnimStart_ms,
                                                 Vision::CompositeImage& outCompImg) const
{
  if( (nullptr != _faceImageOverride) &&
      ((kOverrideIndefinitely == _faceImageOverrideEndTime_ms) || 
       (timeSinceAnimStart_ms < _faceImageOverrideEndTime_ms)) ){
    outCompImg.AddImage(kFullFaceSpriteBox, _faceImageOverride);
    return true;
  }

  if(IsEmpty()){
    return false;
  }

  bool addedImage = false;
  for(auto& trackPair : *_spriteBoxMap){
    auto& track = trackPair.second;

    Vision::SpriteBoxKeyFrame currentKeyFrame;
    if(!track.GetCurrentKeyFrame(timeSinceAnimStart_ms, currentKeyFrame)){
      // Nothing to render for this track. Skip to the next
      continue;
    }

    if(_overrideAllSpritesToEyeHue){
      currentKeyFrame.spriteBox.renderMethod = Vision::SpriteRenderMethod::EyeColor;
    }

    // Get a SpriteHandle to the image we want to display
    Vision::SpriteHandle spriteHandle;
    if(spriteSeqContainer.IsValidSpriteSequenceID(currentKeyFrame.assetID)){
      const uint16_t frameIdx = (timeSinceAnimStart_ms - currentKeyFrame.triggerTime_ms) / ANIM_TIME_STEP_MS;
      const Vision::SpriteSequence& sequence = 
        *spriteSeqContainer.GetSpriteSequence(currentKeyFrame.assetID);

      if(!GetFrameFromSpriteSequenceHelper(sequence, frameIdx, currentKeyFrame.spriteSeqEndType, spriteHandle)){
        // The spriteSequence has nothing to draw. Skip to the next track
        continue;
      }
    } else {
      spriteHandle = spriteCache.GetSpriteHandleForAssetID(currentKeyFrame.assetID);
    }

    outCompImg.AddImage(currentKeyFrame.spriteBox, spriteHandle);
    addedImage = true;
  }

  return addedImage;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// Local Helpers
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
namespace{
bool GetFrameFromSpriteSequenceHelper(const Vision::SpriteSequence& sequence,
                                      const uint16_t absFrameIdx,
                                      const Vision::SpriteSeqEndType& spriteSeqEndType,
                                      Vision::SpriteHandle& outSpriteHandle)
{
  uint16_t relFrameIdx = absFrameIdx;
  switch(spriteSeqEndType){
    case Vision::SpriteSeqEndType::Loop:
    {
      relFrameIdx = absFrameIdx % sequence.GetNumFrames();
      break;
    }
    case Vision::SpriteSeqEndType::Hold:
    {
      uint16_t maxIndex = sequence.GetNumFrames() - 1;
      relFrameIdx = (absFrameIdx > maxIndex) ? maxIndex : absFrameIdx;
      break;
    }
    case Vision::SpriteSeqEndType::Clear:
    {
      // Draw Nothing for this SpriteBox
      if(absFrameIdx >= sequence.GetNumFrames()){
        return false;
      }
    }
  }

  sequence.GetFrame(relFrameIdx, outSpriteHandle);
  return true;
}
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// SpriteBoxTrack
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
SpriteBoxTrack::SpriteBoxTrack()
: _lastAccessTime_ms(0)
, _firstKeyFrameTime_ms(std::numeric_limits<TimeStamp_t>::max())
, _lastKeyFrameTime_ms(0)
, _iteratorsAreValid(false)
, _remappedAssetID(Vision::SpritePathMap::kInvalidSpriteID)
{
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
SpriteBoxTrack::SpriteBoxTrack(const SpriteBoxTrack& other)
:_track(other._track)
// last access should not copy
, _lastAccessTime_ms(0)
, _firstKeyFrameTime_ms(other._firstKeyFrameTime_ms)
, _lastKeyFrameTime_ms(other._lastKeyFrameTime_ms)
// Remaps and iterators do not copy
, _iteratorsAreValid(false)
, _remappedAssetID(Vision::SpritePathMap::kInvalidSpriteID)
{
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
bool SpriteBoxTrack::InsertKeyFrame(Vision::SpriteBoxKeyFrame&& spriteBox)
{
  _iteratorsAreValid = false;
  TimeStamp_t triggerTime_ms = spriteBox.triggerTime_ms;
  if(_track.insert(std::move(spriteBox)).second){
    if(_firstKeyFrameTime_ms > triggerTime_ms){
      _firstKeyFrameTime_ms = triggerTime_ms;
    }
    if (_lastKeyFrameTime_ms < triggerTime_ms){
      _lastKeyFrameTime_ms = triggerTime_ms;
    }
    return true;
  }

  return false;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void SpriteBoxTrack::ClearUpToTime(const TimeStamp_t toTime_ms)
{
  if(_track.empty()){
    // Make sure we don't try to increment an end() iterator
    return;
  }

  // Find the current set of keyframes
  auto currentKeyFrameIter = _track.begin();
  auto nextKeyFrameIter = ++_track.begin();

  while( (_track.end() != nextKeyFrameIter) && (nextKeyFrameIter->triggerTime_ms <= toTime_ms) ){
    ++currentKeyFrameIter;
    ++nextKeyFrameIter;
  }

  bool modifiedTrack = false;
  if(_track.begin() != currentKeyFrameIter){
    currentKeyFrameIter = _track.erase(_track.begin(), currentKeyFrameIter);
    modifiedTrack = true;
    _firstKeyFrameTime_ms = _track.begin()->triggerTime_ms;
  }

  _iteratorsAreValid &= !modifiedTrack;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
bool SpriteBoxTrack::GetCurrentKeyFrame(const TimeStamp_t timeSinceAnimStart_ms, Vision::SpriteBoxKeyFrame& outKeyFrame)
{
  if(_track.empty() || (timeSinceAnimStart_ms < _firstKeyFrameTime_ms) ){
    // Nothing to draw yet
    return false;
  }

  // Use the last access as a search start point (if appropriate) to optimize search for linear playback
  if(!_iteratorsAreValid || (timeSinceAnimStart_ms < _lastAccessTime_ms) ){
    // Rewind
    _currentKeyFrameIter = _track.begin();
    _nextKeyFrameIter = ++_track.begin();
    _iteratorsAreValid = true;
  }
  _lastAccessTime_ms = timeSinceAnimStart_ms;

  // Find the current set of keyframes
  while( (_track.end() != _nextKeyFrameIter) && (_nextKeyFrameIter->triggerTime_ms <= timeSinceAnimStart_ms) ){
    ++_currentKeyFrameIter;
    ++_nextKeyFrameIter;
  }
  outKeyFrame = *_currentKeyFrameIter;

  // "Clear" keyframes override everything, including remaps and "Empty"s. Render nothing for this keyframe
  if(Vision::SpritePathMap::kClearSpriteBoxID == outKeyFrame.assetID){
    // Nothing to display
    return false;
  }

  if(Vision::SpritePathMap::kInvalidSpriteID != _remappedAssetID){
    outKeyFrame.assetID = _remappedAssetID;
  }

  // Could have remapped to Empty, so check after remaps are applied
  if(Vision::SpritePathMap::kEmptySpriteBoxID == outKeyFrame.assetID){
    // Nothing to render for an empty spritebox
    return false;
  }

  if( (timeSinceAnimStart_ms == outKeyFrame.triggerTime_ms) ||
      (_track.end() == _nextKeyFrameIter) )
  {
    // No interpolation required/possible
    return true;
  }

  // Interpolate between keyframes as appropriate for timestamp
  const auto& currentKeyFrame = *_currentKeyFrameIter;
  const auto& nextKeyFrame = *_nextKeyFrameIter;

  const float interpRatio = ( (float)(timeSinceAnimStart_ms - currentKeyFrame.triggerTime_ms) /
                              (float)(nextKeyFrame.triggerTime_ms - currentKeyFrame.triggerTime_ms) );

  if(currentKeyFrame.spriteBox.alpha != nextKeyFrame.spriteBox.alpha){
    outKeyFrame.spriteBox.alpha = 
      (1.0f - interpRatio) * currentKeyFrame.spriteBox.alpha + interpRatio * nextKeyFrame.spriteBox.alpha;
  }
  if(currentKeyFrame.spriteBox.xPos  != nextKeyFrame.spriteBox.xPos){
    outKeyFrame.spriteBox.xPos = 
      (1.0f - interpRatio) * currentKeyFrame.spriteBox.xPos + interpRatio * nextKeyFrame.spriteBox.xPos;
  }
  if(currentKeyFrame.spriteBox.yPos  != nextKeyFrame.spriteBox.yPos){
    outKeyFrame.spriteBox.yPos = 
      (1.0f - interpRatio) * currentKeyFrame.spriteBox.yPos + interpRatio * nextKeyFrame.spriteBox.yPos;
  }

  return true;
}

} // namespace Animations
} // namespace Vector
} // namespace Anki
