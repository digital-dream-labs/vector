/**
* File: track
*
* Author: damjan stulic
* Created: 9/16/15
*
* Description: 
*
* Copyright: Anki, inc. 2015
*
*/


#ifndef __Anki_Cozmo_Basestation_Animations_Track_H__
#define __Anki_Cozmo_Basestation_Animations_Track_H__

#include "cannedAnimLib/baseTypes/keyframe.h"
#include "coretech/common/shared/types.h"
#include "json/json-forwards.h"
#include "util/helpers/boundedWhile.h"
#include <stdint.h>
#include <list>

namespace CozmoAnim {
  struct HeadAngle;
  struct LiftHeight;
  struct RobotAudio;
  struct FaceAnimation;
  struct ProceduralFace;
  struct Event;
  struct BodyMotion;
  struct RecordHeading;
  struct TurnToRecordedHeading;
}

namespace Anki {
namespace Vector {

namespace Animations {

// Templated class for storing/accessing various "tracks", which
// hold different types of KeyFrames.
template<class FRAME_TYPE>
class Track
{
public:
  static constexpr size_t ConstMaxFramesPerTrack() { return 1000; }

  //
  // Default copy constructor and copy assignment operator do not do the right thing for std::list iterators.
  // When a std::list is copied, any associated iterator must be updated to point to the same location in
  // the new location.  The default member-wise copy produces an invalid iterator.
  //
  
  // Default constructor, move constructor, move assignment
  Track<FRAME_TYPE>() = default;
  Track<FRAME_TYPE>(Track<FRAME_TYPE>&&) noexcept = default;
  Track<FRAME_TYPE>& operator = (Track<FRAME_TYPE>&&) noexcept = default;
  
  // Custom copy constructor
  Track<FRAME_TYPE>(const Track<FRAME_TYPE>& other) :
    _frames(other._frames),
    _frameIter(_frames.begin())
  {
    if(!other._frames.empty()){
      // Advance iterator to preserve original position
      for (auto pos = other._frames.begin(); pos != other._frameIter; ++pos) {
        ++_frameIter;
      }
    }
  }
  
  // Custom copy assignment
  Track<FRAME_TYPE>& operator = (const Track<FRAME_TYPE>& other)
  {
    if (this != &other) {
      _frames = other._frames;
      _frameIter = _frames.begin();
      // Advance iterator to preserve original position
      for (auto pos = other._frames.begin(); pos != other._frameIter; ++pos) {
        ++_frameIter;
      }
    }
    return *this;
  }

  bool operator ==(const Track<FRAME_TYPE>& other) const
  {
    return _frames == other._frames;
  }


  Result AddKeyFrameToBack(const FRAME_TYPE& keyFrame);

  // Define from JSON. Second argument is used to print nicer debug strings if something goes wrong.
  Result AddKeyFrameToBack(const Json::Value& jsonRoot, const std::string& animNameDebug = "");

  // Define from FlatBuffers. Second argument is used to print nicer debug strings if something goes wrong.
  // TODO: Reduce some code duplication in this method (COZMO-8766). Rather than have all the different types
  //       for the different FlatBuffers-generated classes, can we use a single AddFlatBufKeyFrameToBack()
  //       method that has the type of the FlatBuffers class as an additional template argument?
  Result AddKeyFrameToBack(const CozmoAnim::LiftHeight* liftKeyframe, const std::string& animNameDebug = "");
  Result AddKeyFrameToBack(const CozmoAnim::ProceduralFace* procFaceKeyframe, const std::string& animNameDebug = "");
  Result AddKeyFrameToBack(const CozmoAnim::HeadAngle* headKeyframe, const std::string& animNameDebug = "");
  Result AddKeyFrameToBack(const CozmoAnim::RobotAudio* audioKeyframe, const std::string& animNameDebug = "");
  Result AddKeyFrameToBack(const CozmoAnim::Event* eventKeyframe, const std::string& animNameDebug = "");
  Result AddKeyFrameToBack(const CozmoAnim::BodyMotion* bodyKeyframe, const std::string& animNameDebug = "");
  Result AddKeyFrameToBack(const CozmoAnim::RecordHeading* recordHeadingKeyframe, const std::string& animNameDebug = "");
  Result AddKeyFrameToBack(const CozmoAnim::TurnToRecordedHeading* turnToRecordedHeadingKeyframe, const std::string& animNameDebug = "");
  
  Result AddNewKeyFrameToBack(const FRAME_TYPE& newKeyFrame);
  
  Result AddKeyFrameByTime(const FRAME_TYPE& keyFrame);

  // Return the Streaming message for the current KeyFrame if it is time,
  // nullptr otherwise. Also returns nullptr if there are no KeyFrames
  // left in the track.
  RobotInterface::EngineToRobot* GetCurrentStreamingMessage(const TimeStamp_t relativeStreamingTime_ms) const;
  
  // Get a reference to the current KeyFrame in the track.
  FRAME_TYPE& GetCurrentKeyFrame() const {
    ANKI_VERIFY(HasFramesLeft(),"Track.GetCurrentKeyframe.NoFramesLeft","");
    return *_frameIter;
  }
  
  // Get pointer to next keyframe. Returns nullptr if the track is on the last frame.
  const FRAME_TYPE* GetNextKeyFrame() const;
  
  // Get a pointer to the first KeyFrame in the track. Returns nullptr if track is empty.
  const FRAME_TYPE* GetFirstKeyFrame() const;
  
  // Get a pointer to the last KeyFrame in the track. Returns nullptr if track is empty.
  const FRAME_TYPE* GetLastKeyFrame() const;
  FRAME_TYPE* GetLastKeyFrame();
  
  std::list<FRAME_TYPE> GetCopyOfKeyframes() const { return _frames;}

  std::list<FRAME_TYPE>& GetAllKeyframes() { return _frames;}

  
  // Move to next frame and delete the current one if it's marked "live".
  // Will not advance past end.
  void MoveToNextKeyFrame();
  
  // Move to previous frame. Will not rewind before beginning.
  void MoveToPrevKeyFrame();
  
  // Move to the last keyframe in the track. Deletes keyframes along the way if
  // this is a "live" track.
  void MoveToLastKeyFrame();
  
  // Set the track back to the first keyframe
  void MoveToStart();

  // Move to the very end, deleting keyframes along the way if this is a "live"
  // track. HasFramesLeft() will be false after this.
  void MoveToEnd();

  bool HasFramesLeft() const { return _frameIter != _frames.end(); }

  // Check to see whether the return value of GetCurrentKeyFrame is valid
  bool CurrentFrameIsValid(const TimeStamp_t relativeStreamingTime_ms) const {
    return HasFramesLeft() && 
           GetCurrentKeyFrame().IsTimeToPlay(relativeStreamingTime_ms);
  }

  bool IsEmpty() const { return _frames.empty(); }
  int TrackLength() const { return static_cast<int>(_frames.size()); }


  void Clear() { _frames.clear(); _frameIter = _frames.end(); }

  // Clear all frames up to, but not including, the current one.
  void ClearUpToCurrent();

  // Append Track to current track
  void AppendTrack(const Track& appendTrack, const TimeStamp_t appendStartTime_ms);

  // Moves the frameIter forward to the keyframe that should be set for the given time
  // NOTE: This function only moves the track forward
  void AdvanceTrack(const TimeStamp_t toTime_ms);



  std::list<FRAME_TYPE>& GetAllFrames() { return _frames;}

private:

  using FrameList = std::list<FRAME_TYPE>;
  using FrameListIter = typename std::list<FRAME_TYPE>::iterator;

  // List of frames
  FrameList _frames;

  // Pointer to current position
  FrameListIter _frameIter = _frames.begin();

  Result AddKeyFrameToBackHelper(const FRAME_TYPE& keyFrame, FRAME_TYPE* &prevKeyFrame);
  Result AddKeyFrameByTimeHelper(const FRAME_TYPE& keyFrame, FRAME_TYPE* &prevKeyFrame);

  // Use to setup keyframe duration for specific keyframe types
  void SetKeyFrameDurationHelper();
  void AdvanceTrackHelper(const TimeStamp_t toTime_ms);


}; // class Track
  
template<typename FRAME_TYPE>
void Track<FRAME_TYPE>::MoveToStart()
{
  _frameIter = _frames.begin();
}

template<typename FRAME_TYPE>
void Track<FRAME_TYPE>::MoveToNextKeyFrame()
{
  if(_frameIter != _frames.end()) {
    ++_frameIter;
  }
}
  
template<typename FRAME_TYPE>
void Track<FRAME_TYPE>::MoveToPrevKeyFrame()
{
  if(_frameIter != _frames.begin()) {
    --_frameIter;
  }
}
  
template<typename FRAME_TYPE>
void Track<FRAME_TYPE>::MoveToLastKeyFrame()
{
  if(_frames.empty()) {
    // nothing to do
    return;
  }
  
  // Jump to end and then move back one
  _frameIter = _frames.end();
  --_frameIter;
}
  
template<typename FRAME_TYPE>
void Track<FRAME_TYPE>::MoveToEnd()
{
  _frameIter = _frames.end();
}
  
template<typename FRAME_TYPE>
const FRAME_TYPE* Track<FRAME_TYPE>::GetNextKeyFrame() const
{
  DEV_ASSERT(_frameIter != _frames.end(), "Frame iterator should not be at end");
  
  auto nextIter = _frameIter;
  ++nextIter;
  
  if(nextIter == _frames.end()) {
    return nullptr;
  } else {
    return &(*nextIter);
  }
}

template<typename FRAME_TYPE>
const FRAME_TYPE* Track<FRAME_TYPE>::GetFirstKeyFrame() const
{
  if(_frames.empty()) {
    return nullptr;
  } else {
    return &(_frames.front());
  }
}

template<typename FRAME_TYPE>
FRAME_TYPE* Track<FRAME_TYPE>::GetLastKeyFrame()
{
  if(_frames.empty()) {
    return nullptr;
  } else {
    return &(_frames.back());
  }
}

template<typename FRAME_TYPE>
const FRAME_TYPE* Track<FRAME_TYPE>::GetLastKeyFrame() const
{
  if(_frames.empty()) {
    return nullptr;
  } else {
    return &(_frames.back());
  }
}
  
template<typename FRAME_TYPE>
Result Track<FRAME_TYPE>::AddKeyFrameToBackHelper(const FRAME_TYPE& keyFrame,
                                                  FRAME_TYPE* &prevKeyFrame)
{
  prevKeyFrame = nullptr;
  
  if(_frames.size() > ConstMaxFramesPerTrack()) {
    PRINT_NAMED_WARNING("Animation.Track.AddKeyFrameToBack.TooManyFrames",
                        "There are already %zu frames in track of type %s. Refusing to add more.",
                        _frames.size(), typeid(keyFrame).name());
    return RESULT_FAIL;
  }
  
  if(!_frames.empty()) {
    prevKeyFrame = &(_frames.back());
  } else {
    prevKeyFrame = nullptr;
  }

  _frames.emplace_back(keyFrame);

  // If we just added the first keyframe we need to reset the frameIter to point
  // back to the beginning.
  if(_frames.size() == 1) {
    _frameIter = _frames.begin();
  }

  return RESULT_OK;
}
  
template<typename FRAME_TYPE>
inline Result Track<FRAME_TYPE>::AddKeyFrameToBack(const FRAME_TYPE& keyFrame)
{
  FRAME_TYPE* dummy = nullptr;
  return AddKeyFrameToBackHelper(keyFrame, dummy);
}

template<> Result
Track<ProceduralFaceKeyFrame>::AddKeyFrameToBack(const ProceduralFaceKeyFrame& keyFrame);

  
// Specialization for BodyMotion keyframes (implemented in .cpp)
template<>
Result Track<BodyMotionKeyFrame>::AddKeyFrameToBack(const BodyMotionKeyFrame& keyFrame);

// Specialization for BackpackLights keyframes (implemented in .cpp)
template<>
Result Track<BackpackLightsKeyFrame>::AddKeyFrameToBack(const BackpackLightsKeyFrame &keyFrame);
  
  
template<typename FRAME_TYPE>
Result Track<FRAME_TYPE>::AddKeyFrameByTimeHelper(const FRAME_TYPE& keyFrame,
                                                  FRAME_TYPE* &prevKeyFrame)
{
  prevKeyFrame = nullptr;
  if(_frames.size() > ConstMaxFramesPerTrack()) {
    PRINT_NAMED_WARNING("Animation.Track.AddKeyFrameByTime.TooManyFrames",
                        "There are already %zu frames in %s track. Refusing to add more.",
                        _frames.size(), keyFrame.GetClassName().c_str());
    return RESULT_FAIL;
  }

  auto desiredTrigger = keyFrame.GetTriggerTime_ms();
  
  auto framePlaceIter = _frames.begin();
  while (framePlaceIter != _frames.end() && framePlaceIter->GetTriggerTime_ms() <= desiredTrigger)
  {
    // Don't put another key frame at the same time as an existing one
    if (framePlaceIter->GetTriggerTime_ms() == desiredTrigger)
    {
      PRINT_NAMED_ERROR("Animation.Track.AddKeyFrameByTime.DuplicateTime",
                        "There is already a frame at time %u in %s track.",
                        desiredTrigger, keyFrame.GetClassName().c_str());
      return RESULT_FAIL;
    }
    prevKeyFrame = &(*framePlaceIter); // return previous keyframe
    ++framePlaceIter;
  }
  
  _frames.insert(framePlaceIter, keyFrame);

  // If we just added the first keyframe (e.g. after deleting the last remaining
  // keyframe in a "Live" track), we need to reset the frameIter to point
  // back to the beginning.
  if(_frames.size() == 1) {
    _frameIter = _frames.begin();
  }

  return RESULT_OK;
}

template<typename FRAME_TYPE>
inline Result Track<FRAME_TYPE>::AddKeyFrameByTime(const FRAME_TYPE& keyFrame)
{
  FRAME_TYPE* dummy;
  return AddKeyFrameByTimeHelper(keyFrame, dummy);
}

// Specialization for BodyMotion keyframes (implemented in .cpp)
template<>
Result Track<BodyMotionKeyFrame>::AddKeyFrameByTime(const BodyMotionKeyFrame& keyFrame);

// Specialization for BackpackLights keyframes (implemented in .cpp)
template<>
Result Track<BackpackLightsKeyFrame>::AddKeyFrameByTime(const BackpackLightsKeyFrame &keyFrame);


template<typename FRAME_TYPE>
RobotInterface::EngineToRobot* Track<FRAME_TYPE>::GetCurrentStreamingMessage(const TimeStamp_t relativeStreamingTime_ms) const
{
  RobotInterface::EngineToRobot* msg = nullptr;
  
  if(HasFramesLeft()) {
    FRAME_TYPE& currentKeyFrame = GetCurrentKeyFrame();
    if(currentKeyFrame.IsTimeToPlay(relativeStreamingTime_ms))
    {
      msg = currentKeyFrame.GetStreamMessage(relativeStreamingTime_ms);
    }
  }
  
  return msg;
}


template<typename FRAME_TYPE>
Result Track<FRAME_TYPE>::AddKeyFrameToBack(const CozmoAnim::LiftHeight* liftKeyframe, const std::string& animNameDebug)
{
  FRAME_TYPE newKeyFrame;
  Result lastResult = newKeyFrame.DefineFromFlatBuf(liftKeyframe, animNameDebug);
  if(RESULT_OK != lastResult) {
    return lastResult;
  }
  return AddNewKeyFrameToBack(newKeyFrame);
}

template<typename FRAME_TYPE>
Result Track<FRAME_TYPE>::AddKeyFrameToBack(const CozmoAnim::ProceduralFace* procFaceKeyframe, const std::string& animNameDebug)
{
  FRAME_TYPE newKeyFrame;
  Result lastResult = newKeyFrame.DefineFromFlatBuf(procFaceKeyframe, animNameDebug);
  if(RESULT_OK != lastResult) {
    return lastResult;
  }
  const auto res =  AddNewKeyFrameToBack(newKeyFrame);
  auto& allKeyframes = GetAllKeyframes();
  if(allKeyframes.size() >= 2){
    auto backIter = allKeyframes.rbegin();
    auto oldBackIter = allKeyframes.rbegin();
    oldBackIter++;
    oldBackIter->SetKeyframeActiveDuration_ms(backIter->GetTriggerTime_ms() - oldBackIter->GetTriggerTime_ms());
  }
  
  return res;
}

template<typename FRAME_TYPE>
Result Track<FRAME_TYPE>::AddKeyFrameToBack(const CozmoAnim::HeadAngle* headKeyframe, const std::string& animNameDebug)
{
  FRAME_TYPE newKeyFrame;
  Result lastResult = newKeyFrame.DefineFromFlatBuf(headKeyframe, animNameDebug);
  if(RESULT_OK != lastResult) {
    return lastResult;
  }
  return AddNewKeyFrameToBack(newKeyFrame);
}

template<typename FRAME_TYPE>
Result Track<FRAME_TYPE>::AddKeyFrameToBack(const CozmoAnim::RobotAudio* audioKeyframe, const std::string& animNameDebug)
{
  FRAME_TYPE newKeyFrame;
  Result lastResult = newKeyFrame.DefineFromFlatBuf(audioKeyframe, animNameDebug);
  if(RESULT_OK != lastResult) {
    return lastResult;
  }
  return AddNewKeyFrameToBack(newKeyFrame);
}

template<typename FRAME_TYPE>
Result Track<FRAME_TYPE>::AddKeyFrameToBack(const CozmoAnim::Event* eventKeyframe, const std::string& animNameDebug)
{
  FRAME_TYPE newKeyFrame;
  Result lastResult = newKeyFrame.DefineFromFlatBuf(eventKeyframe, animNameDebug);
  if(RESULT_OK != lastResult) {
    return lastResult;
  }
  return AddNewKeyFrameToBack(newKeyFrame);
}

template<typename FRAME_TYPE>
Result Track<FRAME_TYPE>::AddKeyFrameToBack(const CozmoAnim::BodyMotion* bodyKeyframe, const std::string& animNameDebug)
{
  FRAME_TYPE newKeyFrame;
  Result lastResult = newKeyFrame.DefineFromFlatBuf(bodyKeyframe, animNameDebug);
  if(RESULT_OK != lastResult) {
    return lastResult;
  }
  return AddNewKeyFrameToBack(newKeyFrame);
}

template<typename FRAME_TYPE>
Result Track<FRAME_TYPE>::AddKeyFrameToBack(const CozmoAnim::RecordHeading* recordHeadingKeyframe, const std::string& animNameDebug)
{
  FRAME_TYPE newKeyFrame;
  Result lastResult = newKeyFrame.DefineFromFlatBuf(recordHeadingKeyframe, animNameDebug);
  if(RESULT_OK != lastResult) {
    return lastResult;
  }
  return AddNewKeyFrameToBack(newKeyFrame);
}

template<typename FRAME_TYPE>
Result Track<FRAME_TYPE>::AddKeyFrameToBack(const CozmoAnim::TurnToRecordedHeading* turnToRecordedHeadingKeyframe, const std::string& animNameDebug)
{
  FRAME_TYPE newKeyFrame;
  Result lastResult = newKeyFrame.DefineFromFlatBuf(turnToRecordedHeadingKeyframe, animNameDebug);
  if(RESULT_OK != lastResult) {
    return lastResult;
  }
  return AddNewKeyFrameToBack(newKeyFrame);
}

template<typename FRAME_TYPE>
Result Track<FRAME_TYPE>::AddKeyFrameToBack(const Json::Value &jsonRoot, const std::string& animNameDebug)
{
  FRAME_TYPE newKeyFrame;
  Result lastResult = newKeyFrame.DefineFromJson(jsonRoot, animNameDebug);
  if(RESULT_OK != lastResult) {
    return lastResult;
  }
  return AddNewKeyFrameToBack(newKeyFrame);
}

template<typename FRAME_TYPE>
Result Track<FRAME_TYPE>::AddNewKeyFrameToBack(const FRAME_TYPE& newKeyFrame)
{
  Result lastResult = AddKeyFrameToBack(newKeyFrame);
  if(RESULT_OK != lastResult) {
    return lastResult;
  }
  
  if(lastResult == RESULT_OK) {
    if(_frames.size() > 1) {
      auto nextToLastFrame = _frames.rbegin();
      ++nextToLastFrame;

      if(_frames.back().GetTriggerTime_ms() <= nextToLastFrame->GetTriggerTime_ms()) {
        PRINT_NAMED_WARNING("Animation.Track.AddKeyFrameToBack.BadTriggerTime",
                            "New keyframe (t=%d) must be after the last keyframe (t=%d)",
                            _frames.back().GetTriggerTime_ms(), nextToLastFrame->GetTriggerTime_ms());
        
        _frames.pop_back();
        lastResult = RESULT_FAIL;
      }
    }
  }

  return lastResult;
}

template<typename FRAME_TYPE>
void Track<FRAME_TYPE>::ClearUpToCurrent()
{
  auto iter = _frames.begin();
  while(iter != _frameIter) {
    iter = _frames.erase(iter);
  }
}

template<class FRAME_TYPE>
void Track<FRAME_TYPE>::AppendTrack(const Track<FRAME_TYPE>& appendTrack, const TimeStamp_t appendStartTime_ms)
{
  for (const FRAME_TYPE& aFrame : appendTrack._frames) {
    FRAME_TYPE newFrame(aFrame);
    TimeStamp_t triggerTime = newFrame.GetTriggerTime_ms();
    newFrame.SetTriggerTime_ms(triggerTime + appendStartTime_ms);
    if ( RESULT_OK != AddKeyFrameToBack(newFrame) ) {
      PRINT_NAMED_ERROR("Track.AppendTrack.AddKeyFrameToBack.Failure", "");
    }
  }
}

template<class FRAME_TYPE>
void Track<FRAME_TYPE>::AdvanceTrack(const TimeStamp_t toTime_ms)
{
  AdvanceTrackHelper(toTime_ms);
}


template<>
void Track<ProceduralFaceKeyFrame>::AdvanceTrack(const TimeStamp_t toTime_ms);
  
template<class FRAME_TYPE>
void Track<FRAME_TYPE>::AdvanceTrackHelper(const TimeStamp_t toTime_ms)
{
  const auto upperBound = _frames.size() + 1;
  BOUNDED_WHILE(upperBound, _frameIter != _frames.end()) {
    // Ensure tracks don't overlap
    if(ANKI_DEV_CHEATS){
      if(GetNextKeyFrame() != nullptr){
        if((_frameIter->GetTimestampActionComplete_ms() > toTime_ms) &&
           (GetNextKeyFrame()->IsTimeToPlay(toTime_ms))){
          PRINT_NAMED_ERROR("Track.AdvanceTrack.KeyframeStillActiveButTimeToPlayNextFrame",
                            "Keyframe lasts till %u, but next frame wants to start at %u",
                            _frameIter->GetTimestampActionComplete_ms(), toTime_ms);
        }
      }
    }
    
    if(_frameIter->GetTimestampActionComplete_ms() <= toTime_ms){
      _frameIter++;
    }else{
      break;
    }
  }
}
  
  
  
} // end namespace Animations
} // end namespace Vector
} // end namespace Anki


#endif //__Anki_Cozmo_Basestation_Animations_Track_H__
