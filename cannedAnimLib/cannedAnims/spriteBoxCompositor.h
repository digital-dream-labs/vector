/**
 * File: spriteBoxCompositor.h
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

#ifndef __Anki_Vector_SpriteBoxCompositor_H__
#define __Anki_Vector_SpriteBoxCompositor_H__

#include "clad/types/compositeImageTypes.h"

#include "coretech/common/engine/jsonTools.h"
#include "coretech/common/shared/types.h"
#include "coretech/vision/shared/spriteCache/spriteWrapper.h"
#include "coretech/vision/shared/spritePathMap.h"

#include <set>
#include <unordered_map>

// Fwd Decl
namespace CozmoAnim {
  struct SpriteBox;
  struct FaceAnimation;
}

namespace Anki{

// Fwd Decl
namespace Vision{
  class CompositeImage;
  class SpriteCache;
  class SpriteSequenceContainer;
}

namespace Vector{
namespace Animations{

// Fwd Decl
class SpriteBoxTrack;

class SpriteBoxCompositor
{
public:
  SpriteBoxCompositor();
  SpriteBoxCompositor(const SpriteBoxCompositor& other); // Copy ctor
  ~SpriteBoxCompositor() {}

  Result AddKeyFrame(const CozmoAnim::SpriteBox* spriteBoxKeyFrame);
  Result AddKeyFrame(const Json::Value& jsonRoot, const std::string& animName);
  Result AddKeyFrame(Vision::SpriteBoxKeyFrame&& keyFrame) {
    return AddKeyFrameInternal(std::move(keyFrame));
  }

  // Legacy SpriteSequence animation support
  Result AddFullFaceSpriteSeq(const CozmoAnim::FaceAnimation* faceAnimationKeyFrame, 
                              const Vision::SpriteSequenceContainer& spriteSeqContainer);
  Result AddFullFaceSpriteSeq(const Json::Value& faceAnimationKeyFrame, 
                              const Vision::SpriteSequenceContainer& spriteSeqContainer,
                              const std::string& animName);
  Result AddFullFaceSpriteSeqInternal(const Vision::SpritePathMap::AssetID assetID,
                                      const TimeStamp_t triggerTime_ms,
                                      const Vision::SpriteSequenceContainer& spriteSeqContainer);

  void SetFaceImageOverride(const Vision::SpriteHandle& spriteHandle,
                            const TimeStamp_t relativeStreamTime_ms,
                            const TimeStamp_t duration_ms);
  void SetOverrideAllSpritesToEyeHue(){ _overrideAllSpritesToEyeHue = true; }
  void ClearOverrides();

  void AddSpriteBoxRemap(const Vision::SpriteBoxName spriteBox,
                         const Vision::SpritePathMap::AssetID remappedAssetID);

  void CacheInternalSprites(Vision::SpriteCache* spriteCache);

  void AppendTracks(const SpriteBoxCompositor& other, const TimeStamp_t animOffset_ms);
  void Clear();

  // Return true if there are no SpriteBoxKeyFrames in this compositor
  bool IsEmpty() const;

  // Sets the reference time for future calls to time-relative functions
  void AdvanceTrack(const TimeStamp_t& toTime_ms);
  void MoveToStart();

  void ClearUpToCurrent();
  bool HasFramesLeft() const;

  TimeStamp_t CompareLastFrameTime(const TimeStamp_t lastFrameTime_ms) const;

  bool PopulateCompositeImage(Vision::SpriteCache& spriteCache,
                              Vision::SpriteSequenceContainer& spriteSeqContainer,
                              TimeStamp_t timeSinceAnimStart_ms,
                              Vision::CompositeImage& outCompImg) const;

private:
  SpriteBoxCompositor(SpriteBoxCompositor&& other); // Move ctor
  SpriteBoxCompositor& operator=(SpriteBoxCompositor&& other); // Move assignment
  SpriteBoxCompositor& operator=(const SpriteBoxCompositor& other); // Copy assignment

  Result AddKeyFrameInternal(Vision::SpriteBoxKeyFrame&& spriteBox);

  Vision::SpriteBoxKeyFrame InterpolateKeyFrames(const Vision::SpriteBoxKeyFrame& thisKeyFrame,
                                                 const Vision::SpriteBoxKeyFrame& nextKeyFrame,
                                                 TimeStamp_t timeSinceAnimStart_ms) const;

  TimeStamp_t _lastKeyFrameTime_ms;
  TimeStamp_t _advanceTime_ms;

  Vision::SpriteHandle _faceImageOverride = nullptr;
  TimeStamp_t _faceImageOverrideEndTime_ms;

  bool _overrideAllSpritesToEyeHue;

  // Map from SpriteBoxName to set of keyframes ordered by triggerTime_ms
  using SpriteBoxMap = std::unordered_map<Vision::SpriteBoxName, SpriteBoxTrack>;
  std::unique_ptr<SpriteBoxMap> _spriteBoxMap;

};

class SpriteBoxTrack
{
public:
  // Sort SpriteBoxKeyFrames by triggerTime_ms within a set. This also implies that
  // for a given SpriteBoxName, two KeyFrames are considered duplicates if they have
  // the same triggerTime_ms. Ergo, multiple keyframes for the same SBName and trigger
  // time are not allowed.
  struct SpriteBoxKeyFrameCompare{
    bool operator()(const Vision::SpriteBoxKeyFrame& lhs, const Vision::SpriteBoxKeyFrame& rhs) const {
      return lhs.triggerTime_ms < rhs.triggerTime_ms;
    }
  };

  using SpriteBoxKeyFrameSet = std::set<Vision::SpriteBoxKeyFrame, SpriteBoxKeyFrameCompare>;
  using TrackIterator = SpriteBoxKeyFrameSet::iterator;

  SpriteBoxTrack();
  SpriteBoxTrack(const SpriteBoxTrack& other);

  bool InsertKeyFrame(Vision::SpriteBoxKeyFrame&& spriteBox);
  bool IsEmpty() const { return _track.empty(); }
  void ClearUpToTime(const TimeStamp_t toTime_ms);

  bool GetCurrentKeyFrame(const TimeStamp_t timeSinceAnimStart_ms, Vision::SpriteBoxKeyFrame& outKeyFrame);
  const SpriteBoxKeyFrameSet& GetKeyFrames() const { return _track; }

  void SetAssetRemap(const Vision::SpritePathMap::AssetID remappedAssetID){ _remappedAssetID = remappedAssetID; }
  void ClearAssetRemap(){ _remappedAssetID = Vision::SpritePathMap::kInvalidSpriteID; }

private:
  SpriteBoxTrack(SpriteBoxTrack&& other); // Move ctor
  SpriteBoxTrack& operator=(SpriteBoxTrack&& other); // Move assignment
  SpriteBoxTrack& operator=(const SpriteBoxTrack& other); // Copy assignment

  SpriteBoxKeyFrameSet _track;

  TimeStamp_t _lastAccessTime_ms;
  TimeStamp_t _firstKeyFrameTime_ms;
  TimeStamp_t _lastKeyFrameTime_ms;

  bool _iteratorsAreValid;
  TrackIterator _currentKeyFrameIter;
  TrackIterator _nextKeyFrameIter;

  Vision::SpritePathMap::AssetID _remappedAssetID;
};

} // namespace Animations
} // namespace Vector
} // namespace Anki

#endif //__Anki_Vector_SpriteBoxCompositor_H__
