/**
 * File: animationStreamer.h
 *
 * Authors: Andrew Stein
 * Created: 2015-06-25
 *
 * Description:
 *
 *   Handles streaming a given animation from a CannedAnimationContainer
 *   to a robot.
 *
 * Copyright: Anki, Inc. 2015
 *
 **/

#ifndef __Anki_Vector_AnimationStreamer_H__
#define __Anki_Vector_AnimationStreamer_H__

#include "coretech/common/shared/types.h"
#include "coretech/vision/engine/image.h"
#include "cozmoAnim/animation/trackLayerComponent.h"
#include "cozmoAnim/animTimeStamp.h"
#include "cannedAnimLib/cannedAnims/animation.h"
#include "cannedAnimLib/cannedAnims/animationMessageWrapper.h"
#include "cannedAnimLib/baseTypes/track.h"

namespace Anki {
namespace Vision{
}

namespace Vector {

  // Forward declaration
  class ProceduralFace;
  namespace Anim {
    class AnimContext;
  }
  class TextToSpeechComponent;
  class TrackLayerComponent;

  namespace Audio {
    class AnimationAudioClient;
    class ProceduralAudioClient;
  }

  namespace RobotInterface {
    struct AddOrUpdateEyeShift;
    struct RemoveEyeShift;
    struct AddSquint;
    struct RemoveSquint;
  }

namespace Anim {
  class AnimationStreamer
  {
  public:
    using NewAnimationCallback = std::function<void()>;

    using Tag = AnimationTag;
    using FaceTrack = Animations::Track<ProceduralFaceKeyFrame>;

    // TODO: This could be removed in favor of just referring to ::Anki::Cozmo, but avoiding touching too much code now.
    static const Tag kNotAnimatingTag = ::Anki::Vector::kNotAnimatingTag;

    AnimationStreamer(const Anim::AnimContext* context);

    ~AnimationStreamer();

    Result Init(TextToSpeechComponent* ttsComponent);

    // Sets an animation to be streamed and how many times to stream it.
    // Use numLoops = 0 to play the animation indefinitely.
    //
    // If interruptRunning == true, any currently-streaming animation will be aborted.
    // Actual streaming occurs on calls to Update().
    //
    // If name == "" or anim == nullptr, it is equivalent to calling Abort()
    // if there is an animation currently playing, or no-op if there's no
    // animation playing
    //
    // If overrideAllSpritesToEyeHue is true the SpriteBoxKeyFrames will be treated as grayscale
    // and rendered in the robot's eye hue - if it's false the keyframes will be rendered as RGB images
    Result SetStreamingAnimation(const std::string& name,
                                 Tag tag,
                                 u32 numLoops = 1,
                                 u32 startAt_ms = 0,
                                 bool interruptRunning = true,
                                 bool overrideAllSpritesToEyeHue = false);

    // Subset of the function above that is applied in the ::Update function and called from PlayAnimation
    void SetPendingStreamingAnimation(const std::string& name, u32 numLoops);

    Result SetProceduralFace(const ProceduralFace& face, u32 duration_ms);

    void Process_displayFaceImageChunk(const RobotInterface::DisplayFaceImageBinaryChunk& msg);
    void Process_displayFaceImageChunk(const RobotInterface::DisplayFaceImageGrayscaleChunk& msg);
    void Process_displayFaceImageChunk(const RobotInterface::DisplayFaceImageRGBChunk& msg);

    void Process_playAnimWithSpriteBoxRemaps(const RobotInterface::PlayAnimWithSpriteBoxRemaps& msg);

    void Process_playAnimWithSpriteBoxKeyFrames(const RobotInterface::PlayAnimWithSpriteBoxKeyFrames& msg);
    void Process_addSpriteBoxKeyFrames(const RobotInterface::AddSpriteBoxKeyFrames& msg);

    Result SetFaceImage(Vision::SpriteHandle spriteHandle, bool overrideAllSpritesToEyeHue, u32 duration_ms);

    Audio::ProceduralAudioClient* GetProceduralAudioClient() const { return _proceduralAudioClient.get(); }

    // If any animation is set for streaming and isn't done yet, stream it.
    Result Update();

    // If tag == kNotAnimatingTag, it stops whatever animation may currently be playing.
    // Otherwise, it stops the currently running animation only if it matches the specified tag.
    void Abort(Tag tag = kNotAnimatingTag, bool shouldClearProceduralAnim = true);

    const std::string GetStreamingAnimationName() const;
    const Animation* GetStreamingAnimation() const { return _streamingAnimation; }

    void EnableKeepFaceAlive(bool enable, u32 disableTimeout_ms);
    void SetKeepFaceAliveFocus(bool enable);

    // Functions passed in here will be called each time a new animation is set to streaming
    void AddNewAnimationCallback(NewAnimationCallback callback) {
      _newAnimationCallbacks.push_back(callback);
    }

    // Returns the time in ms that the animation streamer will use to get animation frames
    // NOTE: This value generally updated at the end of the Update tick, so checks before streamer update
    // will tell you the stream time used this tick, checks after will show the value for the next call to update
    TimeStamp_t GetRelativeStreamTime_ms() const { return _relativeStreamTime_ms; }


    // Set/Reset the amount of time to wait before forcing KeepFaceAlive() after the last stream has stopped
    void SetKeepFaceAliveLastStreamTimeout(const f32 time_s)
      { _longEnoughSinceLastStreamTimeout_s = time_s; }
    void ResetKeepFaceAliveLastStreamTimeout();

    TrackLayerComponent* GetProceduralTrackComponent() { return _proceduralTrackComponent.get(); }
    const TrackLayerComponent* GetProceduralTrackComponent() const { return _proceduralTrackComponent.get(); }

    // Sets all tracks that should be locked
    void SetLockedTracks(u8 whichTracks)
    {
      if(whichTracks & (u8)AnimTrackFlag::BACKPACK_LIGHTS_TRACK)
      {
        PRINT_NAMED_ERROR("AnimationStreamer.SetLockedTracks.BackpackLightTrack",
                          "Backpack light track is always locked, why are you trying to lock it");
      }
      // Always keep the backpack light track locked in shipping
      #if !ANKI_DEV_CHEATS
      whichTracks |= (u8)AnimTrackFlag::BACKPACK_LIGHTS_TRACK;
      #endif
      _lockedTracks = whichTracks;
    }


    // Lock or unlock an individual track
    void LockTrack(AnimTrackFlag track)
    {
      if(track == AnimTrackFlag::BACKPACK_LIGHTS_TRACK)
      {
        PRINT_NAMED_ERROR("AnimationStreamer.LockTrack.BackpackLightTrack",
                          "Backpack light track is always locked why are you trying to unlock it");
      }

      _lockedTracks |= (u8)track;
    }
    void UnlockTrack(AnimTrackFlag track)
    {
      if(track == AnimTrackFlag::BACKPACK_LIGHTS_TRACK)
      {
        PRINT_NAMED_ERROR("AnimationStreamer.UnlockTrack.BackpackLightTrack",
                          "Backpack light track is always locked why are you trying to unlock it");
      }

      _lockedTracks &= ~(u8)track;
      // Always keep the backpack light track locked in shipping
      #if !ANKI_DEV_CHEATS
      _lockedTracks |= (u8)AnimTrackFlag::BACKPACK_LIGHTS_TRACK;
      #endif
    }

    void DrawToFace(const Vision::ImageRGB& img, Array2d<u16>& img565_out);

    // Whether or not to redirect a face image to the FaceInfoScreenManager
    // for display on a debug screen
    void RedirectFaceImagesToDebugScreen(bool redirect) { _redirectFaceImagesToDebugScreen = redirect; }
    
    void SetOnCharger(bool onCharger);
    
    // When on the charger, the robot won't play any motion or audio frames, irrespective of locked tracks
    void SetFrozenOnCharger(bool enabled);

    // Procedural Eye
    void ProcessAddOrUpdateEyeShift(const RobotInterface::AddOrUpdateEyeShift& msg);
    void ProcessRemoveEyeShift(const RobotInterface::RemoveEyeShift& msg);
    void ProcessAddSquint(const RobotInterface::AddSquint& msg);
    void ProcessRemoveSquint(const RobotInterface::RemoveSquint& msg);

    uint16_t GetNumLayersRendered() { return _numLayersRendered; }

  private:
    const Anim::AnimContext* _context = nullptr;

    Animation*  _streamingAnimation = nullptr;
    Animation*  _neutralFaceAnimation = nullptr;

     // for creating animations "live" or dynamically
    Animation*  _proceduralAnimation = nullptr;

    std::unique_ptr<TrackLayerComponent>  _proceduralTrackComponent;

    u32 _numLoops = 1;
    u32 _loopCtr  = 0;

    // Next animation, used by PlayAnimation and called from a thread

    std::mutex _pendingAnimationMutex;
    std::string _pendingAnimation = "";
    u32 _pendingNumLoops = 0;

    // Start and end messages sent to engine
    bool _startOfAnimationSent = false;
    bool _endOfAnimationSent   = false;

    bool _wasAnimationInterruptedWithNothing = false;

    bool _backpackAnimationLayerEnabled = false;

    // Whether or not the streaming animation was commanded internally
    // from within this class (as opposed to by an engine message)
    bool _playingInternalAnim = false;

    // When this animation started playing (was initialized) in milliseconds, in
    // "real" basestation time
    AnimTimeStamp_t _startTime_ms;

    // Where we are in the animation in terms of what has been streamed out, since
    // we don't stream in real time. Each time we send an audio frame to the
    // robot (silence or actual audio), this increments by one audio sample
    // length, since that's what keeps time for streaming animations (not a
    // clock)
    TimeStamp_t _relativeStreamTime_ms;
    // There are a few special cases where time should not be incremented for a tick
    // e.g. looping animations which are initialized one tick, but don't get their first
    // update call until the next tick
    bool _incrementTimeThisTick = true;

    // Time when procedural face layer can next be applied.
    // There's a minimum amount of time that must pass since the last
    // non-procedural face (which has higher priority) was drawn in order
    // to smooth over gaps in between non-procedural frames that can occur
    // when trying to render them at near real-time. Otherwise, procedural
    // face layers like eye darts could play during these gaps.
    AnimTimeStamp_t _nextProceduralFaceAllowedTime_ms = 0;

    // Last time we streamed anything
    f32 _lastAnimationStreamTime = std::numeric_limits<f32>::lowest();

    Tag _tag;

    // For track locking
    u8 _lockedTracks;

    // Which tracks are currently playing
    u8 _tracksInUse;

    std::unique_ptr<Audio::AnimationAudioClient> _animAudioClient;
    std::unique_ptr<Audio::ProceduralAudioClient> _proceduralAudioClient;

    // Time to wait before forcing KeepFaceAlive() after the latest stream has stopped
    f32 _longEnoughSinceLastStreamTimeout_s;

    // Image buffer that is fed directly to face display (in RGB565 format)
    Vision::ImageRGB565 _faceDrawBuf;

    // Image buffer for ProceduralFace
    Vision::ImageRGB _procFaceImg;

    // Storage and chunk tracking for faceImage data received from engine

    // Image used for both binary and grayscale images
    Vision::Image    _faceImageGrayscale;

    // Binary images
    u32              _faceImageId                       = 0;          // Used only for tracking chunks of the same image as they are received
    u8               _faceImageChunksReceivedBitMask    = 0;
    const u8         kAllFaceImageChunksReceivedMask    = 0x3;        // 2 bits for 2 expected chunks

    // Grayscale images
    u32                 _faceImageGrayscaleId                    = 0;      // Used only for tracking chunks of the same image as they are received
    u32                 _faceImageGrayscaleChunksReceivedBitMask = 0;
    const u32           kAllFaceImageGrayscaleChunksReceivedMask = 0x7fff; // 15 bits for 15 expected chunks (FACE_DISPLAY_NUM_PIXELS / 1200 pixels_per_msg ~= 15)

    // RGB images
    Vision::ImageRGB565 _faceImageRGB565;
    u32                 _faceImageRGBId                    = 0;          // Used only for tracking chunks of the same image as they are received
    u32                 _faceImageRGBChunksReceivedBitMask = 0;
    const u32           kAllFaceImageRGBChunksReceivedMask = 0x3fffffff; // 30 bits for 30 expected chunks (FACE_DISPLAY_NUM_PIXELS / 600 pixels_per_msg ~= 30)

    // Tic counter for sending animState message
    u32           _numTicsToSendAnimState            = 0;

    bool _redirectFaceImagesToDebugScreen = false;
    bool _lockFaceTrackAtEndOfStreamingAnimation = false;

    std::vector<NewAnimationCallback> _newAnimationCallbacks;
    
    bool _onCharger = false;
    
    bool _frozenOnCharger = false;
    
    static uint16_t _numLayersRendered;

    static bool IsTrackLocked(u8 lockedTracks, u8 trackFlagToCheck) {
      return ((lockedTracks & trackFlagToCheck) == trackFlagToCheck);
    }

    void SendAnimationMessages(AnimationMessageWrapper& stateToSend);

    Result SetStreamingAnimation(Animation* anim,
                                 Tag tag,
                                 u32 numLoops = 1,
                                 u32 startAt_ms = 0,
                                 bool interruptRunning = true,
                                 bool overrideAllSpritesToEyeHue = false,
                                 bool isInternalAnim = true,
                                 bool shouldClearProceduralAnim = true);

    // Initialize the streaming of an animation with a given tag
    // (This will call anim->Init())
    Result InitStreamingAnimation(Tag withTag,
                                  u32 startAt_ms = 0,
                                  bool overrideAllSpritesToEyeHue = false);

    // Update Stream of either the streaming animation or procedural tracks
    Result ExtractAnimationMessages(AnimationMessageWrapper& stateToSend);
    // Actually stream the animation (called each tick)
    Result ExtractMessagesFromStreamingAnim(AnimationMessageWrapper& stateToSend);

    // Used to stream _just_ the stuff left in the various layers (all procedural stuff)
    Result ExtractMessagesFromProceduralTracks(AnimationMessageWrapper& stateToSend);

    // Combine the tracks inside of the specified animations with the tracks in the track layer component
    // specified, and then assign the output to stateToSend
    Result ExtractMessagesRelatedToProceduralTrackComponent(const Anim::AnimContext* context,
                                                                   Animation* anim,
                                                                   TrackLayerComponent* trackComp,
                                                                   const u8 tracksCurrentlyLocked,
                                                                   const TimeStamp_t timeSinceAnimStart_ms,
                                                                   const bool storeFace,
                                                                   AnimationMessageWrapper& stateToSend);


    void SetKeepAliveIfAppropriate();
    // Indicates if keep alive is currently playing
    bool IsKeepAlivePlaying() const;

    // This performs the test cases for the animation while loop
    bool ShouldProcessAnimationFrame( Animation* anim, TimeStamp_t startTime_ms, TimeStamp_t streamingTime_ms );

    // Sends the start of animation message to engine
    Result SendStartOfAnimation();

    // Sends the end of animation message to engine if the
    // number of commanded loops of the animation has completed.
    // If abortingAnim == true, then the message is sent even if all loops were not completed.
    Result SendEndOfAnimation(bool abortingAnim = false);

    // Enables/Disables the backpack lights animation layer on the robot
    // if it hasn't already been enabled/disabled
    Result EnableBackpackAnimationLayer(bool enable);

    // Check whether the animation is done
    bool IsStreamingAnimFinished() const;

    void StopTracks(const u8 whichTracks);

    // In case we are aborting an animation, stop any tracks that were in use
    // (For now, this just means motor-based tracks.) Note that we don't
    // stop tracks we weren't using, in case we were, for example, playing
    // a head animation while driving a path.
    // If we're just calling this at the normal end of an animation then
    // head and lift tracks are not stopped so that they settle at the last
    // commanded keyframe.
    void StopTracksInUse(bool aborting = true);

    // pass the started/stopped animation name to webviz
    void SendAnimationToWebViz( bool starting ) const;

    // Copy the contents of the animation into the procedural animation
    // while maintaining expected properties of the procedural anim
    void CopyIntoProceduralAnimation(Animation* desiredAnim = nullptr);

    static void InsertStreamableFaceIntoCompImg(Vision::ImageRGB565& streamableFace,
                                                Vision::CompositeImage& image);
    
    void InvalidateBannedTracks(const std::string& animName,
                                AnimationMessageWrapper& messageWrapper) const;

    static void GetStreamableFace(const Anim::AnimContext* context, const ProceduralFace& procFace, Vision::ImageRGB565& outImage);
    void BufferFaceToSend(Vision::ImageRGB565& image);

  #if ANKI_DEV_CHEATS
    void UpdateCaptureFace(const Vision::ImageRGB565& faceImg565);
  #endif // ANKI_DEV_CHEATS

    // Sends msg to appropriate destination as long as the specified track is unlocked
    bool SendIfTrackUnlocked(RobotInterface::EngineToRobot*& msg, AnimTrackFlag track);

  }; // class AnimationStreamer

} // namespace Anim
} // namespace Vector
} // namespace Anki

#endif /* __Anki_Vector_AnimationStreamer_H__ */
