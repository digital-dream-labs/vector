/**
 * File: animationStreamer.cpp
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

#include "coretech/common/shared/array2d.h"
#include "coretech/common/engine/utils/timer.h"
#include "coretech/common/engine/utils/data/dataPlatform.h"
#include "cozmoAnim/animation/animationStreamer.h"
#include "coretech/vision/shared/compositeImage/compositeImage.h"

#include "cannedAnimLib/cannedAnims/animationInterpolator.h"
#include "cannedAnimLib/proceduralFace/proceduralFaceDrawer.h"
#include "coretech/vision/shared/spriteSequence/spriteSequence.h"
#include "coretech/vision/shared/spriteSequence/spriteSequenceContainer.h"

#include "cozmoAnim/audio/animationAudioClient.h"
#include "cozmoAnim/audio/proceduralAudioClient.h"
#include "cozmoAnim/faceDisplay/faceDisplay.h"
#include "cozmoAnim/faceDisplay/faceInfoScreenManager.h"
#include "cozmoAnim/animContext.h"
#include "cozmoAnim/animProcessMessages.h"
#include "cozmoAnim/robotDataLoader.h"
#include "osState/osState.h"
#include "util/console/consoleInterface.h"
#include "util/cpuProfiler/cpuProfiler.h"
#include "util/fileUtils/fileUtils.h"
#include "util/logging/logging.h"
#include "util/string/stringUtils.h"
#include "webServerProcess/src/webService.h"

#include "clad/robotInterface/messageRobotToEngine.h"
#include "clad/robotInterface/messageEngineToRobot.h"
#include "clad/robotInterface/messageRobotToEngine_sendAnimToEngine_helper.h"
#include "clad/robotInterface/messageEngineToRobot_sendAnimToRobot_helper.h"

#include "jo_gif/jo_gif.h"
#include "gif-h/gif.h"

#define LOG_CHANNEL "Animations"

#define DEBUG_ANIMATION_STREAMING 0
#define DEBUG_ANIMATION_STREAMING_AUDIO 0

namespace Anki {
namespace Vector {
namespace Anim {
#define CONSOLE_GROUP "Face.ParameterizedFace"

  enum class FaceDisplayType {
    Normal,
    Test,                 // Displays pattern showcasing RGB at different intensities
    FullWhite,            // Displays a completely white image
    OverrideIndividually, // each eyes parameters operate on their respective eye
    OverrideTogether      // left eye parameters drive both left and right eyes
  };

  // Overrides whatever faces we're sending with a 3-stripe test pattern
  // (seems more related to the other ProceduralFace console vars, so putting it in that group instead)
  CONSOLE_VAR_ENUM(int, kProcFace_Display,              CONSOLE_GROUP, 0, "Normal,Test,FullWhite,Override individually,Override together"); // Override procedural face with ConsoleVars edited version
#if PROCEDURALFACE_NOISE_FEATURE
  CONSOLE_VAR_EXTERN(s32, kProcFace_NoiseNumFrames);
#endif
  CONSOLE_VAR_ENUM(int, kProcFace_GammaType,            CONSOLE_GROUP, 0, "None,FromLinear,ToLinear,AddGamma,RemoveGamma,Custom");
  CONSOLE_VAR_RANGED(f32, kProcFace_Gamma,              CONSOLE_GROUP, 1.f, 1.f, 4.f);

  enum class FaceGammaType {
    None,
    FromLinear,
    ToLinear,
    AddGamma,    // Use value of kProcFace_Gamma
    RemoveGamma, // Use value of kProcFace_Gamma
    Custom
  };

  static ProceduralFace s_faceDataOverride; // incoming values from console var system
  static ProceduralFace s_faceDataBaseline; // baseline to compare against, differences mean override the incoming animation
  static bool s_faceDataReset = false;

  uint16_t AnimationStreamer::_numLayersRendered = 0;

#if ANKI_DEV_CHEATS
  static const Anim::AnimContext* s_context; // copy of AnimContext in first constructed AnimationStreamer, needed for GetDataPlatform
  static bool s_faceDataOverrideRegistered = false;
  static uint8_t s_gammaLUT[3][256];// RGB x 256 entries

  void ResetFace(ConsoleFunctionContextRef context)
  {
    s_faceDataReset = true;
  }

  CONSOLE_FUNC(ResetFace, CONSOLE_GROUP);

  static void LoadFaceGammaLUT(ConsoleFunctionContextRef context)
  {
    const std::string filename = ConsoleArg_GetOptional_String(context, "filename", "screenshot.tga");

    const Util::Data::DataPlatform* dataPlatform = s_context->GetDataPlatform();
    const std::string cacheFilename = dataPlatform->pathToResource(Util::Data::Scope::Cache, filename);

    Vision::Image tga;
    Result result = tga.Load(cacheFilename);
    if (result == RESULT_OK) {
      const int width = tga.GetNumCols();
      const int height = tga.GetNumRows();
      const int channels = tga.GetNumChannels();
      if (width != 256 || height != 1) {
        const std::string html = std::string("<html>\n") +filename+" must be either a 256x1 image file\n" + "</html>\n";
        context->channel->WriteLog("%s", html.c_str());
      } else {
        for (int channel = 0; channel < 3; ++channel) {
          // greyscale: offset = 0 always, RGB and RGBA: offset is channel, A is ignored
          uint8_t* srcPtr = tga.GetRawDataPointer() + (channel % channels);
          for (int x = 0; x < width; ++x) {
            s_gammaLUT[channel][x] = *srcPtr;
            srcPtr += channels;
          }
        }

        kProcFace_GammaType = (int)FaceGammaType::Custom;
      }
    } else {
      // see https://ankiinc.atlassian.net/browse/VIC-1646 to productize .tga loading
      std::vector<uint8_t> tga = Anki::Util::FileUtils::ReadFileAsBinary(cacheFilename);
      if (tga.size() < 18) {
        const std::string html = std::string("<html>\n") +filename+" is not a .tga file\n" + "</html>\n";
        context->channel->WriteLog("%s", html.c_str());
      } else {
        const int width = tga[12]+tga[13]*256;
        const int height = tga[14]+tga[15]*256;
        const int bytesPerPixel = tga[16] / 8;
        if (tga[2] != 2 && tga[2] != 3) {
          const std::string html = std::string("<html>\n") +filename+" is not an uncompressed, true-color or grayscale .tga file\n" + "</html>\n";
          context->channel->WriteLog("%s", html.c_str());
        } else if (width != 256 || height != 1) {
          const std::string html = std::string("<html>\n") +filename+" must be a 256x1 .tga file\n" + "</html>\n";
          context->channel->WriteLog("%s", html.c_str());
        } else {
          for (int channel = 0; channel < 3; ++channel) {
            // greyscale: offset = 0 always, RGB and RGBA: offset is channel, A is ignored
            uint8_t* srcPtr = &tga[18 + (channel % bytesPerPixel)];
            for (int x = 0; x < width; ++x) {
              s_gammaLUT[channel][x] = *srcPtr;
              srcPtr += bytesPerPixel;
            }
          }

          kProcFace_GammaType = (int)FaceGammaType::Custom;
        }
      }
    }
  }

  CONSOLE_FUNC(LoadFaceGammaLUT, CONSOLE_GROUP, const char* filename);
#endif // ANKI_DEV_CHEATS

  namespace{

  static const std::string kWebVizModuleName = "animations";

  // Specifies how often to send AnimState message
  static const u32 kAnimStateReportingPeriod_tics = 2;

  // Minimum amount of time that must expire after the last non-procedural face
  // is drawn and the next procedural face can be drawn
  static const u32 kMinTimeBetweenLastNonProcFaceAndNextProcFace_ms = 2 * ANIM_TIME_STEP_MS;

  // Default time to wait before forcing KeepFaceAlive() after the latest stream has stopped
  const f32 kDefaultLongEnoughSinceLastStreamTimeout_s = 0.5f;

  bool kIsInManualUpdateMode    = false;
  u32 kCurrentManualFrameNumber = 0;
  CONSOLE_VAR(bool, kShouldDisplayKeyframeNumber, "ManualAnimationPlayback", false);

#if ANKI_DEV_CHEATS
  // Whether or not to display high temperature indicator on face
  CONSOLE_VAR(bool, kDisplayHighTemperature, "AnimationStreamer.System", true);

  // Whether or not to display CPU throttling
  // 2018-06-21: Disabled by default since current OS doesn't throttle for thermal
  //             reasons and sporadic idle throttling is not worth alerting the dev about.
  CONSOLE_VAR(bool, kDisplayCPUThrottling, "AnimationStreamer.System", false);

  // Temperature beyond which the thermal indicator is displayed on face
  CONSOLE_VAR(u32,  kThermalAlertTemp_C, "AnimationStreamer.System", 90);

  CONSOLE_VAR(bool, kDisplayMemoryPressure, "AnimationStreamer.System", true);

  //////////
  /// Manual Playback Console Vars - allow user to play back/hold single frames within an animation
  /////////
  static TimeStamp_t* sDevRelativeTimePtr       = nullptr;
  static Vision::ImageRGB565* sDevBufferFacePtr = nullptr;
  static Animation** sStreamingAnimationPtrPtr  = nullptr;

  void ToggleManualControlOfAnimStreamer(ConsoleFunctionContextRef context)
  {
    kIsInManualUpdateMode = !kIsInManualUpdateMode;
    if (kIsInManualUpdateMode && (sDevRelativeTimePtr != nullptr)){
      kCurrentManualFrameNumber = *sDevRelativeTimePtr/ANIM_TIME_STEP_MS;
    }
  }

  CONSOLE_FUNC(ToggleManualControlOfAnimStreamer, "ManualAnimationPlayback");
  CONSOLE_VAR(u32,  kNumberOfFramesToIncrement,   "ManualAnimationPlayback", 1);

  void IncrementPlaybackFrame(ConsoleFunctionContextRef context)
  {
    kCurrentManualFrameNumber += kNumberOfFramesToIncrement;
  }

  CONSOLE_FUNC(IncrementPlaybackFrame, "ManualAnimationPlayback");

  std::string DevGetFaceImagFolder() {
    const auto faceImgsFolder = "dev_face_imgs";
    const Util::Data::DataPlatform* platform = s_context->GetDataPlatform();
    auto folder = platform->pathToResource( Util::Data::Scope::Cache, faceImgsFolder );
    folder = Util::FileUtils::AddTrailingFileSeparator( folder );
    return folder;
  }

  void CaptureFaceImage(ConsoleFunctionContextRef context)
  {
    if (sDevBufferFacePtr != nullptr){
      auto folder = DevGetFaceImagFolder();

      // make sure our folder structure exists
      if ( Util::FileUtils::DirectoryDoesNotExist( folder ) )
      {
        Util::FileUtils::CreateDirectory( folder, false, true );
      }

      std::string animName;
      if ( (sStreamingAnimationPtrPtr != nullptr) &&
          (*sStreamingAnimationPtrPtr != nullptr)){
        animName = (*sStreamingAnimationPtrPtr)->GetName();
      }

      const std::string filename = ( folder + animName + "_" + std::to_string(kCurrentManualFrameNumber) + ".png" );
      if (Util::FileUtils::FileExists(filename)) {
        Util::FileUtils::DeleteFile(filename);
      }

      sDevBufferFacePtr->Save(filename);
    }
  }

  CONSOLE_FUNC(CaptureFaceImage, "ManualAnimationPlayback");

  void ClearCapturedFaces(ConsoleFunctionContextRef context)
  {
    auto folder = DevGetFaceImagFolder();
    if ( Util::FileUtils::DirectoryExists( folder ) )
    {
      Util::FileUtils::RemoveDirectory( folder );
    }
  }

  CONSOLE_FUNC(ClearCapturedFaces, "ManualAnimationPlayback");
 #endif // ANKI_DEV_CHEATS



  //////////
  /// End Manual Playback Console Vars
  /////////



  // Allows easy disabling of KeepFaceAlive using the console system (i.e., without a message interface)
  // This is useful for the animators to disable KeepFaceAlive while testing eye shapes, etc.
  static bool s_enableKeepFaceAlive = true;

#if ANKI_DEV_CHEATS
  void ToggleKeepFaceAlive(ConsoleFunctionContextRef context)
  {
    s_enableKeepFaceAlive = !s_enableKeepFaceAlive;
    LOG_INFO("ConsoleFunc.ToggleKeepFaceAlive", "KeepFaceAlive now %s",
             (s_enableKeepFaceAlive ? "ON" : "OFF"));
  }

  CONSOLE_FUNC(ToggleKeepFaceAlive, CONSOLE_GROUP);

  static std::string s_frameFilename;
  static int s_frame = 0;
  static int s_framesToCapture = 0;
  static jo_gif_t s_gif1;
  static struct GifWriter s_gif2;
  static int s_gifVersion = 1;
  static clock_t s_frameStart;
  static FILE* s_tga = nullptr;

  static void CaptureFace(ConsoleFunctionContextRef context)
  {
    std::string html;

    if (s_framesToCapture == 0) {
      s_frameFilename = ConsoleArg_GetOptional_String(context, "filename", "screenshot.tga");
      const int numFrames = ConsoleArg_GetOptional_Int(context, "numFrames", 1);

      const Util::Data::DataPlatform* dataPlatform = s_context->GetDataPlatform();
      const std::string cacheFilename = dataPlatform->pathToResource(Util::Data::Scope::Cache, s_frameFilename);

      if (s_frameFilename.find(".gif") != std::string::npos) {
        s_gifVersion = 1;
        s_gif1 = jo_gif_start(cacheFilename.c_str(), FACE_DISPLAY_WIDTH, FACE_DISPLAY_HEIGHT, 0, 256);
        if (s_gif1.fp != nullptr) {
          s_framesToCapture = numFrames;
        }

      } else if (s_frameFilename.find(".GIF") != std::string::npos) {
        s_gifVersion = 2;
        if (GifBegin(&s_gif2, cacheFilename.c_str(), FACE_DISPLAY_WIDTH, FACE_DISPLAY_HEIGHT, 0, 8)) {
          s_framesToCapture = numFrames;
        }

      } else {
        s_tga = fopen(cacheFilename.c_str(), "wb");
        if (s_tga != nullptr) {
          uint8_t head[18] = {0};
          head[ 2] = 2; // uncompressed, true-color image
          head[12] = FACE_DISPLAY_WIDTH & 0xff;
          head[13] = (FACE_DISPLAY_WIDTH >> 8) & 0xff;
          head[14] = FACE_DISPLAY_HEIGHT & 0xff;
          head[15] = (FACE_DISPLAY_HEIGHT >> 8) & 0xff;
          head[16] = 32;   /** 32 bits depth **/
          head[17] = 0x28; /** top-down flag, 8 bits alpha **/
          fwrite(head, sizeof(uint8_t), 18, s_tga);

          s_framesToCapture = 1;
        }
      }

      if (s_framesToCapture > 0) {
        s_frameStart = clock();
        s_frame = 0;

        html = std::string("<html>\nCapturing frames as <a href=\"/cache/")+s_frameFilename+"\">"+s_frameFilename+"\n" + "</html>\n";
      } else {
        html = std::string("<html>\nError: unable to open file <a href=\"/cache/")+s_frameFilename+"\">"+s_frameFilename+"\n" + "</html>\n";
      }
    } else {
      html = std::string("Capture already in progress as <a href=\"/cache/")+s_frameFilename+"\">"+s_frameFilename+"\n" + "</html>\n";
    }

    context->channel->WriteLog("%s", html.c_str());
  }

  CONSOLE_FUNC(CaptureFace, "Face", optional const char* filename, optional int numFrames);
#endif // ANKI_DEV_CHEATS

  CONSOLE_VAR(bool, kShouldDisplayPlaybackTime, "AnimationStreamer", false);

  // Disable streaming of backpack lights keyframes by default
  CONSOLE_VAR(bool, kEnableBackpackLightsTrack, "AnimationStreamer", false);

  } // namespace

#undef CONSOLE_GROUP

  AnimationStreamer::AnimationStreamer(const Anim::AnimContext* context)
  : _context(context)
  , _proceduralTrackComponent(new TrackLayerComponent(context))
  , _lockedTracks((u8)AnimTrackFlag::BACKPACK_LIGHTS_TRACK)
  , _tracksInUse(0)
  , _animAudioClient( new Audio::AnimationAudioClient(context->GetAudioController()) )
  , _proceduralAudioClient( new Audio::ProceduralAudioClient(context->GetAudioController()) )
  , _longEnoughSinceLastStreamTimeout_s(kDefaultLongEnoughSinceLastStreamTimeout_s)
  , _numTicsToSendAnimState(kAnimStateReportingPeriod_tics)
  {
    CopyIntoProceduralAnimation();

  #if ANKI_DEV_CHEATS
    {
      _lockedTracks = 0;
      sDevRelativeTimePtr = &_relativeStreamTime_ms;
      sDevBufferFacePtr = &_faceDrawBuf;
      sStreamingAnimationPtrPtr = &_streamingAnimation;
      if (!s_faceDataOverrideRegistered) {
        s_context = context;
        s_faceDataOverride.RegisterFaceWithConsoleVars();
        s_faceDataOverrideRegistered = true;
      }
    }
  #endif // ANKI_DEV_CHEATS
  }

  Result AnimationStreamer::Init(TextToSpeechComponent* ttsComponent)
  {
    // TODO: Restore ability to subscribe to messages here?
    //       It's currently hard to do with CPPlite messages.
    // SetupHandlers(_context->GetExternalInterface());

    // Set neutral face
    DEV_ASSERT(nullptr != _context, "AnimationStreamer.Init.NullContext");
    DEV_ASSERT(nullptr != _context->GetDataLoader(), "AnimationStreamer.Init.NullRobotDataLoader");
    const std::string neutralFaceAnimName = "anim_neutral_eyes_01";
    _neutralFaceAnimation = _context->GetDataLoader()->GetCannedAnimation(neutralFaceAnimName);
    if (nullptr != _neutralFaceAnimation)
    {
      auto frame = _neutralFaceAnimation->GetTrack<ProceduralFaceKeyFrame>().GetFirstKeyFrame();
      ProceduralFace::SetResetData(frame->GetFace());
    }
    else
    {
      LOG_ERROR("AnimationStreamer.Constructor.NeutralFaceDataNotFound",
                "Could not find expected neutral face animation file called %s",
                neutralFaceAnimName.c_str());
    }

    // Do this after the ProceduralFace class has set to use the right neutral face
    _proceduralTrackComponent->Init(*this);

    _faceDrawBuf.Allocate(FACE_DISPLAY_HEIGHT, FACE_DISPLAY_WIDTH);
    _procFaceImg.Allocate(FACE_DISPLAY_HEIGHT, FACE_DISPLAY_WIDTH);
    _faceImageRGB565.Allocate(FACE_DISPLAY_HEIGHT, FACE_DISPLAY_WIDTH);
    _faceImageGrayscale.Allocate(FACE_DISPLAY_HEIGHT, FACE_DISPLAY_WIDTH);

    // Start with a blank face (face scale == 0) until the engine has initialized and sent an animation
    {
      ProceduralFace blankFace;
      const f32 zeroScale = 0.0f;
      const std::vector<f32> arbitraryEyes((int)ProceduralEyeParameter::NumParameters, 0.5f);
      blankFace.SetFromValues(arbitraryEyes, arbitraryEyes, 0.0f, 0.0f, 0.0f, zeroScale, zeroScale, 0.0f);

      SetProceduralFace(blankFace, std::numeric_limits<u32>::max());

      ProceduralFace::SetBlankFaceData(blankFace);
    }

    if (_animAudioClient != nullptr) {
      _animAudioClient->SetTextToSpeechComponent(ttsComponent);
    }

    return RESULT_OK;
  }

  AnimationStreamer::~AnimationStreamer()
  {
  #if ANKI_DEV_CHEATS
    sDevRelativeTimePtr       = nullptr;
    sDevBufferFacePtr         = nullptr;
    sStreamingAnimationPtrPtr = nullptr;
  #endif // ANKI_DEV_CHEATS
    Util::SafeDelete(_proceduralAnimation);

    FaceDisplay::removeInstance();
  }

  Result AnimationStreamer::SetStreamingAnimation(const std::string& name, Tag tag, u32 numLoops, u32 startAt_ms,
                                                  bool interruptRunning, bool overrideAllSpritesToEyeHue)
  {
    // Special case: stop streaming the current animation
    if (name.empty()) {
      if (DEBUG_ANIMATION_STREAMING) {
        LOG_DEBUG("AnimationStreamer.SetStreamingAnimation.StoppingCurrent",
                  "Stopping streaming of animation '%s'.",
                  GetStreamingAnimationName().c_str());
      }

      Abort();
      return RESULT_OK;
    }
    auto* anim = _context->GetDataLoader()->GetCannedAnimation(name);
    return SetStreamingAnimation(anim, tag, numLoops, startAt_ms, interruptRunning,
                                 overrideAllSpritesToEyeHue, false);
  }

  void AnimationStreamer::SetPendingStreamingAnimation(const std::string& name, u32 numLoops)
  {
    std::lock_guard<std::mutex> lock(_pendingAnimationMutex);
    _pendingAnimation = name;
    _pendingNumLoops = numLoops;
  }

  Result AnimationStreamer::SetStreamingAnimation(Animation* anim, Tag tag, u32 numLoops, u32 startAt_ms,
                                                  bool interruptRunning,
                                                  bool overrideAllSpritesToEyeHue,
                                                  bool isInternalAnim, bool shouldClearProceduralAnim)
  {
    if (DEBUG_ANIMATION_STREAMING)
    {
      LOG_DEBUG("AnimationStreamer.SetStreamingAnimation", "Name:%s Tag:%d NumLoops:%d",
                anim != nullptr ? anim->GetName().c_str() : "NULL", tag, numLoops);
    }

    const bool wasStreamingSomething = (nullptr != _streamingAnimation);

    if (wasStreamingSomething)
    {
      if (nullptr != anim && !interruptRunning) {
        LOG_INFO("AnimationStreamer.SetStreamingAnimation.NotInterrupting",
                 "Already streaming %s, will not interrupt with %s",
                 _streamingAnimation->GetName().c_str(),
                 anim->GetName().c_str());
        return RESULT_FAIL;
      }

      LOG_INFO("AnimationStreamer.SetStreamingAnimation.Aborting",
               "Animation %s is interrupting animation %s",
               anim != nullptr ? anim->GetName().c_str() : "NULL",
               _streamingAnimation->GetName().c_str());

      Abort(kNotAnimatingTag, shouldClearProceduralAnim);
    }

    _streamingAnimation = anim;

    if (_streamingAnimation == nullptr) {
      // Perform New Animation Callbacks to prepare for procedural animations
      for (const auto& callback: _newAnimationCallbacks) {
        callback();
      }
      return RESULT_OK;
    }

    _wasAnimationInterruptedWithNothing = false;

    _loopCtr = 0;
    _numLoops = numLoops;
    // Get the animation ready to play
    InitStreamingAnimation(tag, startAt_ms, overrideAllSpritesToEyeHue);

    _playingInternalAnim = isInternalAnim;

    if (DEBUG_ANIMATION_STREAMING) {
      LOG_DEBUG("AnimationStreamer.SetStreamingAnimation",
                "Will start streaming '%s' animation %d times with tag=%d.",
                _streamingAnimation->GetName().c_str(), numLoops, tag);
    }

    return RESULT_OK;
  }

  Result AnimationStreamer::SetProceduralFace(const ProceduralFace& face, u32 duration_ms)
  {
    DEV_ASSERT(nullptr != _proceduralAnimation, "AnimationStreamer.SetProceduralFace.NullProceduralAnimation");

    // Always add one keyframe
    ProceduralFaceKeyFrame keyframe(face);
    Result result = _proceduralAnimation->AddKeyFrameToBack(keyframe);

    // Add a second one later to interpolate to, if duration is longer than one keyframe
    if (RESULT_OK == result && duration_ms > ANIM_TIME_STEP_MS)
    {
      keyframe.SetTriggerTime_ms(duration_ms-ANIM_TIME_STEP_MS);
      result = _proceduralAnimation->AddKeyFrameToBack(keyframe);
    }

    if (!(ANKI_VERIFY(RESULT_OK == result, "AnimationStreamer.SetProceduralFace.FailedToCreateAnim", "")))
    {
      return result;
    }

    // ProceduralFace is always played as an "internal" animation since it's not considered
    // a regular animation by the engine so we don't need to send AnimStarted and AnimEnded
    // messages for it.
    result = SetStreamingAnimation(_proceduralAnimation, 0);

    return result;
  }

  void AnimationStreamer::SendAnimationMessages(AnimationMessageWrapper& stateToSend)
  {

#     if DEBUG_ANIMATION_STREAMING
#       define DEBUG_STREAM_KEYFRAME_MESSAGE(__KF_NAME__) \
                LOG_INFO("AnimationStreamer.SendAnimationMessages", \
                         "Streaming %sKeyFrame at t=%dms.", __KF_NAME__, \
                         _relativeStreamTime_ms)
#     else
#       define DEBUG_STREAM_KEYFRAME_MESSAGE(__KF_NAME__)
#     endif

    if (SendIfTrackUnlocked(stateToSend.moveHeadMessage, AnimTrackFlag::HEAD_TRACK)) {
      DEBUG_STREAM_KEYFRAME_MESSAGE("HeadAngle");
    }

    if (SendIfTrackUnlocked(stateToSend.moveLiftMessage, AnimTrackFlag::LIFT_TRACK)) {
      DEBUG_STREAM_KEYFRAME_MESSAGE("LiftHeight");
    }

    if (SendIfTrackUnlocked(stateToSend.bodyMotionMessage, AnimTrackFlag::BODY_TRACK)) {
      DEBUG_STREAM_KEYFRAME_MESSAGE("BodyMotion");
    }

    if (SendIfTrackUnlocked(stateToSend.recHeadMessage, AnimTrackFlag::BODY_TRACK)) {
      DEBUG_STREAM_KEYFRAME_MESSAGE("RecordHeading");
    }

    if (SendIfTrackUnlocked(stateToSend.turnToRecHeadMessage, AnimTrackFlag::BODY_TRACK)) {
      DEBUG_STREAM_KEYFRAME_MESSAGE("TurnToRecordedHeading");
    }

    if (SendIfTrackUnlocked(stateToSend.backpackLightsMessage, AnimTrackFlag::BACKPACK_LIGHTS_TRACK)) {
      EnableBackpackAnimationLayer(true);
    }

    if (stateToSend.audioKeyFrameMessage != nullptr) {
      _animAudioClient->PlayAudioKeyFrame( *stateToSend.audioKeyFrameMessage, _context->GetRandom() );
      Util::SafeDelete(stateToSend.audioKeyFrameMessage);
    }

    // Send AnimationEvent directly up to engine if it's time to play one
    if (stateToSend.eventMessage != nullptr) {
      DEBUG_STREAM_KEYFRAME_MESSAGE("Event");
      RobotInterface::SendAnimToEngine(*stateToSend.eventMessage);
      Util::SafeDelete(stateToSend.eventMessage);
    }

    if (stateToSend.haveFaceToSend) {
      DEBUG_STREAM_KEYFRAME_MESSAGE("FaceAnimation");
      BufferFaceToSend(stateToSend.faceImg);
    }

#     undef DEBUG_STREAM_KEYFRAME_MESSAGE

  }


  void AnimationStreamer::Process_displayFaceImageChunk(const RobotInterface::DisplayFaceImageBinaryChunk& msg)
  {
    // Since binary images and grayscale images both use the same underlying image, ensure that
    // only one type is being sent at a time
    DEV_ASSERT(_faceImageGrayscaleChunksReceivedBitMask == 0,
               "AnimationStreamer.Process_displayFaceImageChunk.AlreadyReceivingGrayscaleImage");

    // Expand the bit-packed msg.faceData (every bit == 1 pixel) to byte array (every byte == 1 pixel)
    static const u32 kExpectedNumPixels = FACE_DISPLAY_NUM_PIXELS/2;
    static const u32 kDataLength = sizeof(msg.faceData);
    static_assert(8 * kDataLength == kExpectedNumPixels, "Mismatched face image and bit image sizes");

    if (msg.imageId != _faceImageId) {
      if (_faceImageChunksReceivedBitMask != 0) {
        LOG_WARNING("AnimationStreamer.Process_displayFaceImageChunk.UnfinishedFace",
                    "Overwriting ID %d with ID %d",
                    _faceImageId, msg.imageId);
      }
      _faceImageId = msg.imageId;
      _faceImageChunksReceivedBitMask = 1 << msg.chunkIndex;
    } else {
      _faceImageChunksReceivedBitMask |= 1 << msg.chunkIndex;
    }

    uint8_t* imageData_i = _faceImageGrayscale.GetDataPointer();

    uint32_t destI = msg.chunkIndex * kExpectedNumPixels;

    for (int i = 0; i < kDataLength; ++i)
    {
      uint8_t currentByte = msg.faceData[i];

      for (uint8_t bit = 0; bit < 8; ++bit)
      {
        imageData_i[destI] = ((currentByte & 0x80) > 0) ? 255 : 0;
        ++destI;
        currentByte = (uint8_t)(currentByte << 1);
      }
    }
    assert(destI == kExpectedNumPixels * (1+msg.chunkIndex));

    if (_faceImageChunksReceivedBitMask == kAllFaceImageChunksReceivedMask) {
      auto* img = new Vision::ImageRGBA(FACE_DISPLAY_HEIGHT, FACE_DISPLAY_WIDTH);
      img->SetFromGray(_faceImageGrayscale);
      auto handle = std::make_shared<Vision::SpriteWrapper>(img);
      //LOG_DEBUG("AnimationStreamer.Process_displayFaceImageChunk.CompleteFaceReceived", "");
      EnableKeepFaceAlive(false, msg.duration_ms);
      const bool overrideAllSpritesToEyeHue = true;
      SetFaceImage(handle, overrideAllSpritesToEyeHue, msg.duration_ms);
      _wasAnimationInterruptedWithNothing = true;
      _faceImageId = 0;
      _faceImageChunksReceivedBitMask = 0;
    }
  }

  void AnimationStreamer::Process_displayFaceImageChunk(const RobotInterface::DisplayFaceImageGrayscaleChunk& msg)
  {
    // Since binary images and grayscale images both use the same underlying image, ensure that
    // only one type is being sent at a time
    DEV_ASSERT(_faceImageChunksReceivedBitMask == 0,
               "AnimationStreamer.Process_displayFaceImageChunk.AlreadyReceivingBinaryImage");

    if (msg.imageId != _faceImageGrayscaleId) {
      if (_faceImageGrayscaleChunksReceivedBitMask != 0) {
        LOG_WARNING("AnimationStreamer.Process_displayFaceImageGrayscaleChunk.UnfinishedFace",
                    "Overwriting ID %d with ID %d",
                    _faceImageGrayscaleId, msg.imageId);
      }
      _faceImageGrayscaleId = msg.imageId;
      _faceImageGrayscaleChunksReceivedBitMask = 1 << msg.chunkIndex;
    } else {
      _faceImageGrayscaleChunksReceivedBitMask |= 1 << msg.chunkIndex;
    }

    static const u16 kMaxNumPixelsPerChunk = sizeof(msg.faceData) / sizeof(msg.faceData[0]);
    const auto numPixels = std::min(msg.numPixels, kMaxNumPixelsPerChunk);
    uint8_t* imageData_i = _faceImageGrayscale.GetDataPointer();
    std::copy_n(msg.faceData, numPixels, imageData_i + (msg.chunkIndex * kMaxNumPixelsPerChunk) );

    if (_faceImageGrayscaleChunksReceivedBitMask == kAllFaceImageGrayscaleChunksReceivedMask) {
      auto* img = new Vision::ImageRGBA(FACE_DISPLAY_HEIGHT, FACE_DISPLAY_WIDTH);
      img->SetFromGray(_faceImageGrayscale);
      auto handle = std::make_shared<Vision::SpriteWrapper>(img);
      //LOG_DEBUG("AnimationStreamer.Process_displayFaceImageGrayscaleChunk.CompleteFaceReceived", "");
      EnableKeepFaceAlive(false, msg.duration_ms);
      const bool overrideAllSpritesToEyeHue = true;
      SetFaceImage(handle, overrideAllSpritesToEyeHue, msg.duration_ms);
      _wasAnimationInterruptedWithNothing = true;
      _faceImageGrayscaleId = 0;
      _faceImageGrayscaleChunksReceivedBitMask = 0;
    }
  }

  void AnimationStreamer::Process_displayFaceImageChunk(const RobotInterface::DisplayFaceImageRGBChunk& msg)
  {
    if (msg.imageId != _faceImageRGBId) {
      if (_faceImageRGBChunksReceivedBitMask != 0) {
        LOG_WARNING("AnimationStreamer.Process_displayFaceImageRGBChunk.UnfinishedFace",
                    "Overwriting ID %d with ID %d",
                    _faceImageRGBId, msg.imageId);
      }
      _faceImageRGBId = msg.imageId;
      _faceImageRGBChunksReceivedBitMask = 1 << msg.chunkIndex;
    } else {
      _faceImageRGBChunksReceivedBitMask |= 1 << msg.chunkIndex;
    }

    static const u16 kMaxNumPixelsPerChunk = sizeof(msg.faceData) / sizeof(msg.faceData[0]);
    const auto numPixels = std::min(msg.numPixels, kMaxNumPixelsPerChunk);
    std::copy_n(msg.faceData, numPixels, _faceImageRGB565.GetRawDataPointer() + (msg.chunkIndex * kMaxNumPixelsPerChunk) );

    if (_faceImageRGBChunksReceivedBitMask == kAllFaceImageRGBChunksReceivedMask) {
      auto* img = new Vision::ImageRGBA(FACE_DISPLAY_HEIGHT, FACE_DISPLAY_WIDTH);
      img->SetFromRGB565(_faceImageRGB565);
      auto handle = std::make_shared<Vision::SpriteWrapper>(img);
      //LOG_DEBUG("AnimationStreamer.Process_displayFaceImageRGBChunk.CompleteFaceReceived", "");
      EnableKeepFaceAlive(false, msg.duration_ms);
      const bool overrideAllSpritesToEyeHue = false;
      SetFaceImage(handle, overrideAllSpritesToEyeHue, msg.duration_ms);
      _wasAnimationInterruptedWithNothing = true;
      _faceImageRGBId = 0;
      _faceImageRGBChunksReceivedBitMask = 0;
    }
  }

  void AnimationStreamer::Process_playAnimWithSpriteBoxRemaps(const RobotInterface::PlayAnimWithSpriteBoxRemaps& msg)
  {
    const u32 numLoops = 1;
    const u32 startAtTime_ms = 0;
    const bool interruptRunning = true;
    const bool overrideAllSpritesToEyeHue = false;
    const bool isInternalAnim = false;

    // Hack: if _streamingAnimation == _proceduralAnimation, the subsequent CopyIntoProceduralAnimation call
    // will delete *_streamingAnimation without assigning it to nullptr. This assignment prevents associated
    // undefined behavior
    _streamingAnimation = _neutralFaceAnimation;
    const std::string animName(msg.animName, msg.animName_length);
    CopyIntoProceduralAnimation(_context->GetDataLoader()->GetCannedAnimation(animName));
    for (int i = 0; i < msg.numRemaps; ++i)
    {
      _proceduralAnimation->AddSpriteBoxRemap(msg.spriteBoxRemaps[i].spriteBoxName,
                                              msg.spriteBoxRemaps[i].remappedAssetID);
    }
    SetStreamingAnimation(_proceduralAnimation, msg.tag, numLoops, startAtTime_ms, interruptRunning,
                          overrideAllSpritesToEyeHue, isInternalAnim);
    _lockFaceTrackAtEndOfStreamingAnimation = msg.lockFaceAtEndOfAnim;
  }

  void AnimationStreamer::Process_playAnimWithSpriteBoxKeyFrames(
    const RobotInterface::PlayAnimWithSpriteBoxKeyFrames& msg)
  {
    const u32 numLoops = 1;
    const u32 startAtTime_ms = 0;
    const bool interruptRunning = true;
    const bool overrideAllSpritesToEyeHue = false;
    const bool isInternalAnim = false;

    _streamingAnimation = _neutralFaceAnimation;
    const std::string animName(msg.animName, msg.animName_length);
    if(animName.empty())
    {
      _proceduralAnimation->Clear();
    }
    else
    {
      CopyIntoProceduralAnimation(_context->GetDataLoader()->GetCannedAnimation(animName));
    }

    for(int i = 0; i < msg.numKeyFrames; ++i)
    {
      Vision::SpriteBoxKeyFrame keyFrame(msg.spriteBoxKeyFrames[i]);
      _proceduralAnimation->AddSpriteBoxKeyFrame(std::move(keyFrame));
    }
    
    SetStreamingAnimation(_proceduralAnimation, msg.tag, numLoops, startAtTime_ms, interruptRunning,
                          overrideAllSpritesToEyeHue, isInternalAnim);
  }

  void AnimationStreamer::Process_addSpriteBoxKeyFrames(const RobotInterface::AddSpriteBoxKeyFrames& msg)
  {
    if(_streamingAnimation != _proceduralAnimation){
      LOG_ERROR("AnimationStreamer.AddSpriteBoxKeyFrames.NoAnimation",
                "Attempted to add keyframes to running anim, but no _proceduralAnim was running");
      return;
    }

    for(int i = 0; i < msg.numKeyFrames; ++i)
    {
      Vision::SpriteBoxKeyFrame keyFrame(msg.spriteBoxKeyFrames[i]);
      _proceduralAnimation->AddSpriteBoxKeyFrame(std::move(keyFrame));
    }
  }

  Result AnimationStreamer::SetFaceImage(Vision::SpriteHandle spriteHandle,
                                         bool overrideAllSpritesToEyeHue,
                                         u32 duration_ms)
  {
    if (_redirectFaceImagesToDebugScreen)
    {
      Vision::ImageRGB565 debugImg;
      debugImg.SetFromImageRGB(spriteHandle->GetSpriteContentsRGBA());
      FaceInfoScreenManager::getInstance()->DrawCameraImage(debugImg);

      return RESULT_OK;
    }

    DEV_ASSERT(nullptr != _proceduralAnimation, "AnimationStreamer.SetFaceImage.NullProceduralAnimation");

    _proceduralAnimation->SetFaceImageOverride(spriteHandle, _relativeStreamTime_ms, duration_ms);
    if (_streamingAnimation != _proceduralAnimation)
    {
      return SetStreamingAnimation(_proceduralAnimation, 0, 0, 0, true, overrideAllSpritesToEyeHue);
    } 
    else if(overrideAllSpritesToEyeHue)
    {
      _proceduralAnimation->SetOverrideAllSpritesToEyeHue();
    }
    return RESULT_OK;
  }

  void AnimationStreamer::Abort(Tag tag, bool shouldClearProceduralAnim)
  {
    // Only abort if the tag matches the currently playing animation or
    // the tag is kNotAnimatingTag
    if (nullptr != _streamingAnimation &&
        (tag == _tag || tag == kNotAnimatingTag) )
    {
      LOG_INFO("AnimationStreamer.Abort",
               "Tag=%d %s hasFramesLeft=%d startSent=%d endSent=%d",
               _tag,
               _streamingAnimation->GetName().c_str(),
               _streamingAnimation->HasFramesLeft(),
               _startOfAnimationSent,
               _endOfAnimationSent);

      StopTracksInUse();

      if (_startOfAnimationSent) {
        SendEndOfAnimation(true);
      }

      EnableBackpackAnimationLayer(false);

      _animAudioClient->AbortAnimation();

      if ((_streamingAnimation == _proceduralAnimation) &&
          shouldClearProceduralAnim) {
        _proceduralAnimation->Clear();
      } else {
        _streamingAnimation->ClearOverrides();
      }

      // Reset animation pointer
      _streamingAnimation = nullptr;

      // If we get to KeepFaceAlive with this flag set, we'll stream neutral face for safety.
      _wasAnimationInterruptedWithNothing = true;
    }
    _relativeStreamTime_ms = 0;
    _lockFaceTrackAtEndOfStreamingAnimation = false;
  } // Abort()



  Result AnimationStreamer::InitStreamingAnimation(Tag withTag, u32 startAt_ms,
                                                   bool overrideAllSpritesToEyeHue)
  {
    // Perform new animation callbacks
    for (const auto& callback: _newAnimationCallbacks)
    {
      callback();
    }

    kCurrentManualFrameNumber = 0;
    auto* spriteCache = _context->GetDataLoader()->GetSpriteCache();
    Result lastResult = _streamingAnimation->Init(spriteCache);
    if (lastResult == RESULT_OK)
    {
      if (overrideAllSpritesToEyeHue)
      {
        _streamingAnimation->SetOverrideAllSpritesToEyeHue();
      }

      _tag = withTag;

      _startTime_ms = BaseStationTimer::getInstance()->GetCurrentTimeStamp();
      _relativeStreamTime_ms = startAt_ms;

      _endOfAnimationSent = false;
      _startOfAnimationSent = false;

      // If we are initializing ANY animation at all, we don't want keepalive's mucking with the eye 
      // display state. If we eventually decide we want to have an animation screen run with keepalive eyes,
      // this will need to be addressed across the entire keepalive system
      _proceduralTrackComponent->RemoveKeepFaceAlive(_relativeStreamTime_ms, (3 * ANIM_TIME_STEP_MS));

      if (!s_enableKeepFaceAlive)
      {
        // If the animation doesn't have a procedural face track, and Face Keep-alive is false (i.e.,
        // the "last procedural face should persist") then the last procedural face should be blank.
        // Set the last procedural face to blank
        auto& faceTrack = _streamingAnimation->GetTrack<ProceduralFaceKeyFrame>();
        if (faceTrack.IsEmpty())
        {
          _proceduralTrackComponent->SetLastProceduralFaceAsBlank();
        }
      }

    }
    return lastResult;
  }

  // Sends message to robot if track is unlocked and deletes it.
  //
  // TODO: Take in EngineToRobot& instead of ptr?
  //       i.e. Why doesn't KeyFrame::GetStreamMessage() return a ref?
  bool AnimationStreamer::SendIfTrackUnlocked(RobotInterface::EngineToRobot*& msg, AnimTrackFlag track)
  {
    bool res = false;
    if (msg != nullptr)
    {
      if (!IsTrackLocked(_lockedTracks, (u8)track))
      {
        switch(track)
        {
          case AnimTrackFlag::BACKPACK_LIGHTS_TRACK:
          {
            if (!kEnableBackpackLightsTrack)
            {
              break;
            } // else fall through
          }
          case AnimTrackFlag::HEAD_TRACK:
          case AnimTrackFlag::LIFT_TRACK:
          case AnimTrackFlag::BODY_TRACK:
            res = AnimProcessMessages::SendAnimToRobot(*msg);
            _tracksInUse |= (u8)track;
            break;
          default:
            // Audio, face, and event frames are handled separately since they don't actually result in a EngineToRobot message
            LOG_WARNING("AnimationStreamer.SendIfTrackUnlocked.InvalidTrack", "%s", EnumToString(track));
            break;
        }
      }
      Util::SafeDelete(msg);
    }
    return res;
  }

  void AnimationStreamer::GetStreamableFace(const Anim::AnimContext* context, const ProceduralFace& procFace, Vision::ImageRGB565& outImage)
  {
    ANKI_CPU_PROFILE("AnimationStreamer::GetStreamableFace");
    if (kProcFace_Display == (int)FaceDisplayType::Test)
    {
      // Display three color strips increasing in brightness from left to right
      for (int i = 0; i < FACE_DISPLAY_HEIGHT / 3; ++i)
      {
        Vision::PixelRGB565* red_i   = outImage.GetRow(i);
        Vision::PixelRGB565* green_i = outImage.GetRow(i + FACE_DISPLAY_HEIGHT / 3);
        Vision::PixelRGB565* blue_i  = outImage.GetRow(i + 2 * FACE_DISPLAY_HEIGHT / 3);
        for (int j = 0; j < FACE_DISPLAY_WIDTH; ++j)
        {
          const u8 value = Util::numeric_cast_clamped<u8>(std::round((f32)j/(f32)FACE_DISPLAY_WIDTH * 255.f));
          red_i[j]   = Vision::PixelRGB565(value, 0, 0);
          green_i[j] = Vision::PixelRGB565(0, value, 0);
          blue_i[j]  = Vision::PixelRGB565(0, 0, value);
        }
      }
    }
    else if (kProcFace_Display == (int)FaceDisplayType::FullWhite)
    {
      const Vision::PixelRGB565 white(255,255,255);
      outImage.FillWith(white);
    }
    else
    {
      DEV_ASSERT(context != nullptr, "AnimationStreamer.BufferFaceToSend.NoContext");
      DEV_ASSERT(context->GetRandom() != nullptr, "AnimationStreamer.BufferFaceToSend.NoRNGinContext");

      if (s_faceDataReset)
      {
        s_faceDataOverride = s_faceDataBaseline = procFace;
        ProceduralFace::SetHue(ProceduralFace::DefaultHue);

        // animationStreamer.cpp

        NativeAnkiUtilConsoleResetValueToDefault("ProcFace_OverrideEyeParams");
        NativeAnkiUtilConsoleResetValueToDefault("ProcFace_OverrideRightEyeParams");
        NativeAnkiUtilConsoleResetValueToDefault("ProcFace_Gamma");
        NativeAnkiUtilConsoleResetValueToDefault("ProcFace_FromLinear");
        NativeAnkiUtilConsoleResetValueToDefault("ProcFace_ToLinear");

        NativeAnkiUtilConsoleResetValueToDefault("ProcFace_DefaultScanlineOpacity");
        NativeAnkiUtilConsoleResetValueToDefault("ProcFace_NominalEyeSpacing");

        // proceduralFace.cpp

        NativeAnkiUtilConsoleResetValueToDefault("ProcFace_DefaultScanlineOpacity");
        NativeAnkiUtilConsoleResetValueToDefault("ProcFace_NominalEyeSpacing");

        // proceduralFaceDrawer.cpp

        NativeAnkiUtilConsoleResetValueToDefault("ProcFace_NoiseNumFrames");
        NativeAnkiUtilConsoleResetValueToDefault("ProcFace_NoiseMinLightness");
        NativeAnkiUtilConsoleResetValueToDefault("ProcFace_NoiseMaxLightness");
        NativeAnkiUtilConsoleResetValueToDefault("ProcFace_NoiseFraction");
        NativeAnkiUtilConsoleResetValueToDefault("ProcFace_NoiseFraction");

        NativeAnkiUtilConsoleResetValueToDefault("ProcFace_UseAntiAliasedLines");
        NativeAnkiUtilConsoleResetValueToDefault("ProcFace_EyeLightnessMultiplier");

        NativeAnkiUtilConsoleResetValueToDefault("ProcFace_HotspotRender");
        NativeAnkiUtilConsoleResetValueToDefault("ProcFace_HotspotFalloff");
        NativeAnkiUtilConsoleResetValueToDefault("ProcFace_GlowRender");
        NativeAnkiUtilConsoleResetValueToDefault("ProcFace_GlowSizeMultiplier");
        NativeAnkiUtilConsoleResetValueToDefault("ProcFace_GlowLightnessMultiplier");
        NativeAnkiUtilConsoleResetValueToDefault("ProcFace_GlowGaussianFilter");

        NativeAnkiUtilConsoleResetValueToDefault("ProcFace_AntiAliasingSize");
        NativeAnkiUtilConsoleResetValueToDefault("ProcFace_AntiAliasingGaussianFilter");

        s_faceDataReset = false;
      }

      if (kProcFace_Display == (int)FaceDisplayType::OverrideIndividually || kProcFace_Display == (int)FaceDisplayType::OverrideTogether)
      {
        // compare override face data with baseline, if different update the rendered face

        ProceduralFace newProcFace = procFace;

        // for each eye parameter
        if (kProcFace_Display == (int)FaceDisplayType::OverrideTogether)
        {
          s_faceDataOverride.SetParameters(ProceduralFace::WhichEye::Right, s_faceDataOverride.GetParameters(ProceduralFace::WhichEye::Left));
        }
        for (auto whichEye : {ProceduralFace::WhichEye::Left, ProceduralFace::WhichEye::Right})
        {
          for (std::underlying_type<ProceduralFace::Parameter>::type iParam = 0;
               iParam < Util::EnumToUnderlying(ProceduralFace::Parameter::NumParameters);
               ++iParam)
          {
            if (s_faceDataOverride.GetParameter(whichEye, (ProceduralFace::Parameter)iParam) !=
               s_faceDataBaseline.GetParameter(whichEye, (ProceduralFace::Parameter)iParam))
            {
              newProcFace.SetParameter(whichEye,
                                       (ProceduralFace::Parameter)iParam,
                                       s_faceDataOverride.GetParameter(whichEye, (ProceduralFace::Parameter)iParam));
            }
          }
        }

        // for each face parameter
        if (s_faceDataOverride.GetFaceAngle() != s_faceDataBaseline.GetFaceAngle())
        {
          newProcFace.SetFaceAngle(s_faceDataOverride.GetFaceAngle());
        }
        if (s_faceDataOverride.GetFaceScale()[0] != s_faceDataBaseline.GetFaceScale()[0] ||
           s_faceDataOverride.GetFaceScale()[1] != s_faceDataBaseline.GetFaceScale()[1])
        {
          newProcFace.SetFaceScale(s_faceDataOverride.GetFaceScale());
        }
        if (s_faceDataOverride.GetFacePosition()[0] != s_faceDataBaseline.GetFacePosition()[0] ||
           s_faceDataOverride.GetFacePosition()[1] != s_faceDataBaseline.GetFacePosition()[1])
        {
          newProcFace.SetFacePosition(s_faceDataOverride.GetFacePosition());
        }
        if (s_faceDataOverride.GetScanlineOpacity() != s_faceDataBaseline.GetScanlineOpacity())
        {
          newProcFace.SetScanlineOpacity(s_faceDataOverride.GetScanlineOpacity());
        }

        ProceduralFaceDrawer::DrawFace(newProcFace, *context->GetRandom(), outImage);
      }
      else
      {
        ProceduralFaceDrawer::DrawFace(procFace, *context->GetRandom(), outImage);
      }
    }
  }

#if ANKI_DEV_CHEATS
  // Conversions to and from linear space i.e. sRGB to linear and linear to sRGB
  // used when populating the lookup tables for gamma correction.
  // https://github.com/hsluv/hsluv/releases/tag/_legacyjs6.0.4

  static inline float fromLinear(float c)
  {
    if (c <= 0.0031308f) {
      return 12.92f * c;
    } else {
      return (float)(1.055f * pow(c, 1.f / 2.4f) - 0.055f);
    }
  }

  static inline float toLinear(float c)
  {
    float a = 0.055f;

    if (c > 0.04045f) {
      return (float)(powf((c + a) / (1.f + a), 2.4f));
    } else {
      return (float)(c / 12.92f);
    }
  }
  void AnimationStreamer::UpdateCaptureFace(const Vision::ImageRGB565& faceImg565)
  {
    if (s_framesToCapture > 0)
    {
      clock_t end = clock();
      int elapsed = (end - s_frameStart)/float(CLOCKS_PER_SEC);
      s_frameStart = end;

      Vision::ImageRGBA frame(FACE_DISPLAY_HEIGHT, FACE_DISPLAY_WIDTH);
      frame.SetFromRGB565(faceImg565);

      if (s_tga != NULL) {
        fwrite(frame.GetDataPointer(), sizeof(uint8_t), FACE_DISPLAY_WIDTH*FACE_DISPLAY_HEIGHT*4, s_tga);
      } else {
        if (s_gifVersion == 1) {
          jo_gif_frame(&s_gif1, (uint8_t*)frame.GetDataPointer(), 4, false);
        } else {
          GifWriteFrame(&s_gif2, (uint8_t*)frame.GetDataPointer(), FACE_DISPLAY_WIDTH, FACE_DISPLAY_HEIGHT, elapsed*100);
        }
      }

      ++s_frame;
      if (s_frame == s_framesToCapture) {
        if (s_tga != NULL) {
          fclose(s_tga);
        } else {
          if (s_gifVersion == 1) {
            jo_gif_end(&s_gif1);
          } else {
            GifEnd(&s_gif2);
          }
        }

        s_framesToCapture = 0;
      }
    }
  }
#endif // ANKI_DEV_CHEATS

  void AnimationStreamer::BufferFaceToSend(Vision::ImageRGB565& faceImg565)
  {
    DEV_ASSERT_MSG(faceImg565.GetNumCols() == FACE_DISPLAY_WIDTH &&
                   faceImg565.GetNumRows() == FACE_DISPLAY_HEIGHT,
                   "AnimationStreamer.BufferFaceToSend.InvalidImageSize",
                   "Got %d x %d. Expected %d x %d",
                   faceImg565.GetNumCols(), faceImg565.GetNumRows(),
                   FACE_DISPLAY_WIDTH, FACE_DISPLAY_HEIGHT);

#if ANKI_DEV_CHEATS
    static int kProcFace_GammaType_old = (int)FaceGammaType::None;
    static f32 kProcFace_Gamma_old = -1.f;

    if ((kProcFace_GammaType != kProcFace_GammaType_old) || (kProcFace_Gamma_old != kProcFace_Gamma)) {
      switch(kProcFace_GammaType) {
      case (int)FaceGammaType::FromLinear:
        for (int i = 0; i < 256; i++) {
          s_gammaLUT[0][i] = s_gammaLUT[1][i] = s_gammaLUT[2][i] = cv::saturate_cast<uchar>(fromLinear(i / 255.f) * 255.0f);
        }
        break;
      case (int)FaceGammaType::ToLinear:
        for (int i = 0; i < 256; i++) {
          s_gammaLUT[0][i] = s_gammaLUT[1][i] = s_gammaLUT[2][i] = cv::saturate_cast<uchar>(toLinear(i / 255.f) * 255.0f);
        }
        break;
      case (int)FaceGammaType::AddGamma:
        for (int i = 0; i < 256; i++) {
          s_gammaLUT[0][i] = s_gammaLUT[1][i] = s_gammaLUT[2][i] = cv::saturate_cast<uchar>(powf((float)(i / 255.f), 1.f/kProcFace_Gamma) * 255.0f);
        }
        break;
      case (int)FaceGammaType::RemoveGamma:
        for (int i = 0; i < 256; i++) {
          s_gammaLUT[0][i] = s_gammaLUT[1][i] = s_gammaLUT[2][i] = cv::saturate_cast<uchar>(powf((float)(i / 255.f), kProcFace_Gamma) * 255.0f);
        }
        break;
      }

      kProcFace_GammaType_old = kProcFace_GammaType;
      kProcFace_Gamma_old = kProcFace_Gamma;
    }

    if (kProcFace_GammaType != (int)FaceGammaType::None)
    {
      int nrows = faceImg565.GetNumRows();
      int ncols = faceImg565.GetNumCols();
      if (faceImg565.IsContinuous())
      {
        ncols *= nrows;
        nrows = 1;
      }

      for (int i = 0; i < nrows; ++i)
      {
        Vision::PixelRGB565* img_i = faceImg565.GetRow(i);
        for (int j = 0; j < ncols; ++j)
        {
          img_i[j].SetValue(Vision::PixelRGB565(s_gammaLUT[0][img_i[j].r()], s_gammaLUT[1][img_i[j].g()], s_gammaLUT[2][img_i[j].b()]).GetValue());
        }
      }
    }

    UpdateCaptureFace(faceImg565);

    // Display temperature if exceeds threshold
    if (kDisplayHighTemperature)
    {
      auto tempC = OSState::getInstance()->GetTemperature_C();
      if (tempC >= kThermalAlertTemp_C)
      {
        const ColorRGBA alertColor(1.f, 0.f, 0.f);
        const std::string tempStr = std::to_string(tempC) + "C";
        const Point2f position(25, 25);
        faceImg565.DrawText(position, tempStr, alertColor, 1.f);
      }
    }

    // Draw red square in corner of face if CPU throttling
    if (kDisplayCPUThrottling)
    {
      if (OSState::getInstance()->IsCPUThrottling())
      {
        const ColorRGBA squareColor(1.f, 0.f, 0.f);
        const Rectangle<f32> rect( 0, 0, 20, 20);
        faceImg565.DrawFilledRect(rect, squareColor);
      }
    }

    // Draw a colored square in the upper right corner if there's memory pressure
    if (kDisplayMemoryPressure)
    {
      OSState::MemoryInfo info;
      OSState::getInstance()->GetMemoryInfo(info);
      if (info.alert > OSState::Alert::None)
      {
        const ColorRGBA& memAlertColor = (info.alert >= OSState::Alert::Red ? NamedColors::RED : NamedColors::YELLOW);
        const Rectangle<s32> rect(FACE_DISPLAY_WIDTH-30, 0, 30, 25);
        faceImg565.DrawFilledRect(rect, memAlertColor);
        faceImg565.DrawText({FACE_DISPLAY_WIDTH-15, 20},
                            std::to_string(info.availMem_kB/1024),
                            NamedColors::BLACK, 0.55, false, 1, true);
      }
    }

#endif // ANKI_DEV_CHEATS

    if (SHOULD_SEND_DISPLAYED_FACE_TO_ENGINE)
    {
      // Send the final buffered face back over to engine
      ASSERT_NAMED(faceImg565.IsContinuous(), "AnimationComponent.DisplayFaceImage.NotContinuous");
      static int imageID = 0;
      static const int kMaxPixelsPerMsg = 600; // TODO - fix

      int chunkCount = 0;
      int pixelsLeftToSend = FACE_DISPLAY_NUM_PIXELS;
      const u16* startIt = faceImg565.GetRawDataPointer();
      while (pixelsLeftToSend > 0)
      {
        RobotInterface::DisplayedFaceImageRGBChunk msg;
        msg.imageId = imageID;
        msg.chunkIndex = chunkCount++;
        msg.numPixels = std::min(kMaxPixelsPerMsg, pixelsLeftToSend);

        std::copy_n(startIt, msg.numPixels, std::begin(msg.faceData));

        pixelsLeftToSend -= msg.numPixels;
        std::advance(startIt, msg.numPixels);
        RobotInterface::SendAnimToEngine(msg);
      }
      imageID++;

      static const int kExpectedNumChunks = static_cast<int>(std::ceilf( (f32)FACE_DISPLAY_NUM_PIXELS / kMaxPixelsPerMsg ));
      DEV_ASSERT_MSG(chunkCount == kExpectedNumChunks, "AnimationComponent.DisplayFaceImage.UnexpectedNumChunks", "%d", chunkCount);
    }

    if (kShouldDisplayPlaybackTime)
    {
      // Build display str secs:ms
      const auto secs = _relativeStreamTime_ms/1000;
      const auto ms = _relativeStreamTime_ms % 1000;
      std::string playbackTime = std::to_string(secs) + ":" + std::to_string(ms);

      // Estimate if animation process is running slowly and display this on the screen
      const AnimTimeStamp_t estimatedRealTime = BaseStationTimer::getInstance()->GetCurrentTimeStamp() - _startTime_ms;
      const auto timeDrift = estimatedRealTime - _relativeStreamTime_ms;

      ColorRGBA color = NamedColors::GREEN;
      if (timeDrift > (2 * ANIM_TIME_STEP_MS))
      {
        color = NamedColors::RED;

        const auto realSecs = estimatedRealTime/1000;
        const auto realMS = estimatedRealTime % 1000;
        playbackTime += ("/" + std::to_string((TimeStamp_t)realSecs) + ":" + std::to_string((TimeStamp_t)realMS));
      }

      const Point2f pos(20,20);
      const float scale = 0.5f;
      faceImg565.DrawText(pos, playbackTime, color, scale);
    }

    FaceDisplay::getInstance()->DrawToFace(faceImg565);
  }

  Result AnimationStreamer::EnableBackpackAnimationLayer(bool enable)
  {
    RobotInterface::BackpackSetLayer msg;

    if (enable && !_backpackAnimationLayerEnabled) {
      msg.layer = 1; // 1 == BPL_ANIMATION
      _backpackAnimationLayerEnabled = true;
    } else if (!enable && _backpackAnimationLayerEnabled) {
      msg.layer = 0; // 1 == BPL_USER
      _backpackAnimationLayerEnabled = false;
    } else {
      // Do nothing
      return RESULT_OK;
    }

    if (!RobotInterface::SendAnimToRobot(msg)) {
      return RESULT_FAIL;
    }

    return RESULT_OK;
  }

  Result AnimationStreamer::SendStartOfAnimation()
  {
    DEV_ASSERT(!_startOfAnimationSent, "AnimationStreamer.SendStartOfAnimation.AlreadySent");
    DEV_ASSERT(_streamingAnimation != nullptr, "AnimationStreamer.SendStartOfAnimation.NullAnim");
    const std::string& streamingAnimName = _streamingAnimation->GetName();

    if (DEBUG_ANIMATION_STREAMING)
    {
      LOG_DEBUG("AnimationStreamer.SendStartOfAnimation", "Tag=%d, Name=%s, loopCtr=%d",
                _tag, streamingAnimName.c_str(), _loopCtr);
    }

    if (_loopCtr == 0) {
      // Don't actually send start message for proceduralFace or neutralFace anims since
      // they weren't requested by engine
      if (!_playingInternalAnim) {
        AnimationStarted startMsg;
        memcpy(startMsg.animName, streamingAnimName.c_str(), streamingAnimName.length());
        startMsg.animName_length = streamingAnimName.length();
        startMsg.tag = _tag;
        if (!RobotInterface::SendAnimToEngine(startMsg)) {
          return RESULT_FAIL;
        }
      }
    }

    _startOfAnimationSent = true;
    _endOfAnimationSent = false;

    if (ANKI_DEV_CHEATS)
    {
      SendAnimationToWebViz( true );
    }

    return RESULT_OK;
  }

  // TODO: Is this actually being called at the right time?
  //       Need to call this after triggerTime+durationTime of last keyframe has expired.
  Result AnimationStreamer::SendEndOfAnimation(bool abortingAnim)
  {
    DEV_ASSERT(_startOfAnimationSent && !_endOfAnimationSent, "AnimationStreamer.SendEndOfAnimation.StartNotSentOrEndAlreadySent");
    DEV_ASSERT(_streamingAnimation != nullptr, "AnimationStreamer.SendStartOfAnimation.NullAnim");
    const std::string& streamingAnimName = _streamingAnimation->GetName();

    if (DEBUG_ANIMATION_STREAMING)
    {
      LOG_INFO("AnimationStreamer.SendEndOfAnimation", "Tag=%d, Name=%s, t=%dms, loopCtr=%d, numLoops=%d",
               _tag, streamingAnimName.c_str(), _relativeStreamTime_ms, _loopCtr, _numLoops);
    }

    if (abortingAnim || (_loopCtr == _numLoops - 1)) {
      // Don't actually send end message for proceduralFace or neutralFace anims since
      // they weren't requested by engine
      if (!_playingInternalAnim) {
        AnimationEnded endMsg;
        memcpy(endMsg.animName, streamingAnimName.c_str(), streamingAnimName.length());
        endMsg.animName_length = streamingAnimName.length();
        endMsg.tag = _tag;
        endMsg.wasAborted = abortingAnim;
        endMsg.streamTimeAnimEnded = _relativeStreamTime_ms;
        if (!RobotInterface::SendAnimToEngine(endMsg)) {
          return RESULT_FAIL;
        }
      }
    }

    _endOfAnimationSent = true;
    _startOfAnimationSent = false;

    if (ANKI_DEV_CHEATS)
    {
      SendAnimationToWebViz( false );
    }

    // Every time we end an animation we should also re-enable BPL_USER layer on robot
    EnableBackpackAnimationLayer(false);

    return RESULT_OK;
  } // SendEndOfAnimation()


  // TODO(str): VIC-13519 Linearize Face Rendering 
  // This side-loop of rendering logic is a workaround used only if there is no running animation. It
  // should be possible to remove this given a more linear, single-image rendering pipeline. 
  Result AnimationStreamer::ExtractMessagesFromProceduralTracks(AnimationMessageWrapper& stateToSend)
  {
    Result lastResult = RESULT_OK;

    // We don't have an animation but we still have procedural layers to so
    // apply them
    if (_proceduralTrackComponent->HaveLayersToSend())
    {
      // Lock the face track if it's not time for a new procedural face
      u8 lockedTracks = _lockedTracks;
      const bool isFaceTrackAlreadyLocked = IsTrackLocked(lockedTracks, (u8)AnimTrackFlag::FACE_TRACK);
      const bool isTimeForProceduralFace = BaseStationTimer::getInstance()->GetCurrentTimeStamp() >= _nextProceduralFaceAllowedTime_ms;
      if (!isFaceTrackAlreadyLocked && !isTimeForProceduralFace)
      {
        lockedTracks |= (u8)AnimTrackFlag::FACE_TRACK;
      }

      ExtractMessagesRelatedToProceduralTrackComponent(_context, nullptr, _proceduralTrackComponent.get(),
                                                       lockedTracks, _relativeStreamTime_ms, false, stateToSend);

    }

    return lastResult;
  }// ExtractMessagesFromProceduralTracks()


  Result AnimationStreamer::ExtractMessagesFromStreamingAnim(AnimationMessageWrapper& stateToSend)
  {
    ANKI_CPU_PROFILE("AnimationStreamer::ExtractMessagesFromStreamingAnim");
    Result lastResult = RESULT_OK;

    if (!_streamingAnimation->IsInitialized())
    {
      LOG_ERROR("Animation.Update", "%s: Animation must be initialized before it can be played/updated.",
                _streamingAnimation != nullptr ? _streamingAnimation->GetName().c_str() : "<NULL>");
      return RESULT_FAIL;
    }

    if (!_streamingAnimation->HasFramesLeft())
    {
      return RESULT_OK;
    }

    if (!_startOfAnimationSent)
    {
      SendStartOfAnimation();
      _animAudioClient->InitAnimation();
    }

    if (DEBUG_ANIMATION_STREAMING)
    {
      // Very verbose!
      //LOG_INFO("Animation.Update", "%d bytes left to send this Update.",
      //         umBytesToSend);
    }

    // Tracks which have no procedural alterations - grab any messages directly
    {
      auto & headTrack                  = _streamingAnimation->GetTrack<HeadAngleKeyFrame>();
      auto & liftTrack                  = _streamingAnimation->GetTrack<LiftHeightKeyFrame>();
      auto & bodyTrack                  = _streamingAnimation->GetTrack<BodyMotionKeyFrame>();
      auto & recordHeadingTrack         = _streamingAnimation->GetTrack<RecordHeadingKeyFrame>();
      auto & turnToRecordedHeadingTrack = _streamingAnimation->GetTrack<TurnToRecordedHeadingKeyFrame>();

      stateToSend.moveHeadMessage      = headTrack.GetCurrentStreamingMessage(_relativeStreamTime_ms);
      stateToSend.moveLiftMessage      = liftTrack.GetCurrentStreamingMessage(_relativeStreamTime_ms);
      stateToSend.bodyMotionMessage    = bodyTrack.GetCurrentStreamingMessage(_relativeStreamTime_ms);
      stateToSend.recHeadMessage       = recordHeadingTrack.GetCurrentStreamingMessage(_relativeStreamTime_ms);
      stateToSend.turnToRecHeadMessage = turnToRecordedHeadingTrack.GetCurrentStreamingMessage(_relativeStreamTime_ms);

      auto & eventTrack = _streamingAnimation->GetTrack<EventKeyFrame>();
      // Send AnimationEvent directly up to engine if it's time to play one
      if (eventTrack.HasFramesLeft() &&
          eventTrack.GetCurrentKeyFrame().IsTimeToPlay(_relativeStreamTime_ms))
      {
        // Get keyframe and send contents to engine
        const AnimTimeStamp_t currTime_ms = BaseStationTimer::getInstance()->GetCurrentTimeStamp();
        auto& eventKeyFrame = eventTrack.GetCurrentKeyFrame();

        stateToSend.eventMessage = new AnimationEvent();
        stateToSend.eventMessage->event_id = eventKeyFrame.GetAnimEvent();
        stateToSend.eventMessage->timestamp = (TimeStamp_t)currTime_ms;
        stateToSend.eventMessage->tag = _tag;
      }
    }

    // TODO(str): VIC-13519 Linearize Face Rendering 
    // The remainder of this logic is not "ExtractingMessagesFromStreamingAnimation"... this is confusing.
    // This logic could/should be moved up to Update so that building the desired animation is a visibly linear
    // process instead of burying procedural content under canned animation oriented function calls.
    // Apply any track layers to the animation
    static const bool kStoreFace = true;
    ExtractMessagesRelatedToProceduralTrackComponent(_context, _streamingAnimation, _proceduralTrackComponent.get(),
                                                     _lockedTracks, _relativeStreamTime_ms, kStoreFace, stateToSend);

    return lastResult;
  } // ExtractMessagesFromStreamingAnim()


  Result AnimationStreamer::ExtractMessagesRelatedToProceduralTrackComponent(const Anim::AnimContext* context,
                                                                             Animation* anim,
                                                                             TrackLayerComponent* trackComp,
                                                                             const u8 tracksCurrentlyLocked,
                                                                             const TimeStamp_t timeSinceAnimStart_ms,
                                                                             const bool storeFace,
                                                                             AnimationMessageWrapper& stateToSend)
  {
    ANKI_CPU_PROFILE("AnimationStreamer::ExtractMessagesRelatedToProceduralTrackComponent");

    TrackLayerComponent::LayeredKeyFrames layeredKeyFrames;
    trackComp->ApplyLayersToAnim(anim,
                                 timeSinceAnimStart_ms,
                                 layeredKeyFrames,
                                 storeFace);

    if (layeredKeyFrames.haveBackpackKeyFrame &&
        !IsTrackLocked(tracksCurrentlyLocked, (u8)AnimTrackFlag::BACKPACK_LIGHTS_TRACK))
    {
      stateToSend.backpackLightsMessage = layeredKeyFrames.backpackKeyFrame.GetStreamMessage(timeSinceAnimStart_ms);
    }

    if (layeredKeyFrames.haveAudioKeyFrame &&
        !IsTrackLocked(tracksCurrentlyLocked, (u8)AnimTrackFlag::AUDIO_TRACK))
    {
      // TODO: Kevin K. - Avoid this copy w/ restructuring
      stateToSend.audioKeyFrameMessage = new RobotAudioKeyFrame(layeredKeyFrames.audioKeyFrame);
    }

    // TODO(str): VIC-13519 Linearize Face Rendering 
    // Again... it makes precious little sense to be handling all this face rendering logic at the bottom
    // of a huge, apparently unrelated call stack. Takes forever to discover this stuff
    // ----- Face Rendering Code -----

    bool haveEyesToRender = layeredKeyFrames.haveFaceKeyFrame &&
                            !IsTrackLocked(tracksCurrentlyLocked, (u8)AnimTrackFlag::FACE_TRACK);
    

    // Two conflicting refactors cause the eyes to be superimposed under the timer
    // when it goes off. There should be a configuration option to set, but for now
    // we do this hack for 1.7.0
    if (anim != nullptr && anim->GetName() == "anim_timersup_beep_01" )
    {
      haveEyesToRender = false;
    }

    if (haveEyesToRender)
    {
      GetStreamableFace(context, layeredKeyFrames.faceKeyFrame.GetFace(), stateToSend.faceImg);
      stateToSend.haveFaceToSend = true;
    }

    if (anim != nullptr)
    {
      Vision::CompositeImage compImg(ProceduralFace::GetHueSatWrapper());

      // Get the data from the SpriteBoxCompositor
      bool renderFromCompImage = anim->PopulateCompositeImage(*context->GetDataLoader()->GetSpriteCache(),
                                                              *context->GetDataLoader()->GetSpriteSequenceContainer(),
                                                              timeSinceAnimStart_ms, compImg);

      if (renderFromCompImage)
      {
        if (haveEyesToRender)
        {
          // TODO(str): VIC-13519 Linearize Face Rendering 
          // We should just always be overlaying the procedural face onto the comp image above after rendering
          // the layers below the face layer, then finish rendering the rest of the layers in the image.
          // Right now we waste a lot of optimization drawing the face image then copying it into the 
          // CompImage, then pixel-by-pixel blitting it as a full-screen overlay from within the CompImage.
          InsertStreamableFaceIntoCompImg(stateToSend.faceImg, compImg);
        }

        Vision::ImageRGBA img(FACE_DISPLAY_HEIGHT, FACE_DISPLAY_WIDTH);
        compImg.DrawIntoImage(img);
        stateToSend.faceImg.SetFromImageRGB(img);

        stateToSend.haveFaceToSend = true;
      }

      if(renderFromCompImage || haveEyesToRender)
      {
        const AnimTimeStamp_t currTime_ms = BaseStationTimer::getInstance()->GetCurrentTimeStamp();
        _nextProceduralFaceAllowedTime_ms = currTime_ms + kMinTimeBetweenLastNonProcFaceAndNextProcFace_ms;
      }
    }

    // ----- Face Rendering Code -----

    return RESULT_OK;
  }


  Result AnimationStreamer::ExtractAnimationMessages(AnimationMessageWrapper& stateToSend)
  {
    Result lastResult = RESULT_OK;

    bool streamUpdated = false;

    if (_streamingAnimation != nullptr)
    {
      if (IsStreamingAnimFinished())
      {
        ++_loopCtr;

        if (_numLoops == 0 || _loopCtr < _numLoops)
        {
          if (DEBUG_ANIMATION_STREAMING)
          {
            LOG_INFO("AnimationStreamer.Update.Looping",
                    "Finished loop %d of %d of '%s' animation. Restarting.",
                    _loopCtr, _numLoops,
                    _streamingAnimation->GetName().c_str());
          }

          // Reset the animation so it can be played again:
          InitStreamingAnimation(_tag);
          _incrementTimeThisTick = false;

          // To avoid streaming faceLayers set true and start streaming animation next Update() tick.
          streamUpdated = true;
        }
        else
        {
          if (DEBUG_ANIMATION_STREAMING)
          {
            LOG_INFO("AnimationStreamer.Update.FinishedStreaming",
                     "Finished streaming '%s' animation.",
                     _streamingAnimation->GetName().c_str());
          }

          if (_streamingAnimation == _proceduralAnimation)
          {
            _proceduralAnimation->Clear();
          } else {
            _streamingAnimation->ClearOverrides();
          }

          _streamingAnimation = nullptr;
        }

      } // if (IsStreamingAnimFinished())
      else
      {
        // We do want to store this face to the robot since it's coming from an actual animation
        lastResult = ExtractMessagesFromStreamingAnim(stateToSend);
        streamUpdated = true;
        _lastAnimationStreamTime = BaseStationTimer::getInstance()->GetCurrentTimeInSeconds();
        // Send an end-of-animation keyframe when done
        if (!_streamingAnimation->HasFramesLeft() &&
            _relativeStreamTime_ms >= _streamingAnimation->GetLastKeyFrameEndTime_ms() &&
            _startOfAnimationSent &&
            !_endOfAnimationSent)
        {
          StopTracksInUse(false);
          lastResult = SendEndOfAnimation();
          if(_lockFaceTrackAtEndOfStreamingAnimation){
            LockTrack(AnimTrackFlag::FACE_TRACK);
            _lockFaceTrackAtEndOfStreamingAnimation = false;
          }
          if (_animAudioClient->HasActiveEvents())
          {
            LOG_WARNING("AnimationStreamer.ExtractMessagesFromStreamingAnim.EndOfAnimation.ActiveAudioEvent",
                        "AnimName: '%s'", _streamingAnimation->GetName().c_str());
          }
        }
      }
    } // if (_streamingAnimation != nullptr)


    // If we didn't do any streaming above, but we've still got layers to stream
    if (!streamUpdated)
    {
      lastResult = ExtractMessagesFromProceduralTracks(stateToSend);
    }

    return lastResult;
  } // ExtractAnimationMessages()

  void AnimationStreamer::SetKeepAliveIfAppropriate()
  {
    // Always keep face alive, unless we have a streaming animation, since we rely on it
    // to do all face updating and we don't want to step on it's hand-designed toes.
    // Wait a 1/2 second before running after we finish the last streaming animation
    // to help reduce stepping on the next animation's toes when we have things
    // sequenced.
    // NOTE: lastStreamTime>0 check so that we don't start keeping face alive before
    //       first animation of any kind is sent.
    const bool haveStreamingAnimation = _streamingAnimation != nullptr;
    const bool haveStreamedAnything   = _lastAnimationStreamTime > 0.f;
    const bool longEnoughSinceStream  = (BaseStationTimer::getInstance()->GetCurrentTimeInSeconds() - _lastAnimationStreamTime) > _longEnoughSinceLastStreamTimeout_s;
    if (!haveStreamingAnimation &&
         haveStreamedAnything &&
         longEnoughSinceStream)
    {
      if (!FACTORY_TEST)
      {
        if (s_enableKeepFaceAlive)
        {
          // If we were interrupted from streaming an animation and we've met all the
          // conditions to even be in this function, then we should make sure we've
          // got neutral face back on the screen
          if (_wasAnimationInterruptedWithNothing)
          {
            SetStreamingAnimation(_neutralFaceAnimation, kNotAnimatingTag);
            _wasAnimationInterruptedWithNothing = false;
          }

          _proceduralTrackComponent->KeepFaceAlive(_relativeStreamTime_ms);
        }
        else
        {
          _proceduralTrackComponent->KeepFaceTheSame();
        }
      }
      else
      {
        if (_wasAnimationInterruptedWithNothing)
        {
          SetStreamingAnimation(_neutralFaceAnimation, kNotAnimatingTag);
          _wasAnimationInterruptedWithNothing = false;
        }
      }
    }
  }

  bool AnimationStreamer::IsKeepAlivePlaying() const
  {
    return _streamingAnimation == _neutralFaceAnimation;
  }



  Result AnimationStreamer::Update()
  {
    _numLayersRendered = 0;

    {
      std::lock_guard<std::mutex> lock(_pendingAnimationMutex);
      if (!_pendingAnimation.empty()) {
        SetStreamingAnimation(_pendingAnimation, /*tag*/ 1, _pendingNumLoops, 0, /*interruptRunning*/ true);

        _pendingAnimation.clear();
        _pendingNumLoops = 0;
      }
    }

    if (kIsInManualUpdateMode)
    {
      _relativeStreamTime_ms = kCurrentManualFrameNumber * ANIM_TIME_STEP_MS;
    }

    Result lastResult = RESULT_OK;
    AnimationMessageWrapper messageWrapper(_faceDrawBuf);


    // Make sure the proceduralTrackLayers and streaming animation
    // are advanced to the appropriate keyframe
    _proceduralTrackComponent->AdvanceTracks(_relativeStreamTime_ms);
    if (_streamingAnimation != nullptr)
    {
      _streamingAnimation->AdvanceTracks(_relativeStreamTime_ms);

      // Procedural animation is not presistent
      if (_streamingAnimation == _proceduralAnimation)
      {
        _proceduralAnimation->ClearUpToCurrent();
      }
    }

    if (!kIsInManualUpdateMode)
    {
      // Check to see if we're not streaming anything and a keep alive should take over
      SetKeepAliveIfAppropriate();

      // Get the data to send to the robot
      lastResult = ExtractAnimationMessages(messageWrapper);

      if (_incrementTimeThisTick)
      {
        _relativeStreamTime_ms += ANIM_TIME_STEP_MS;
      }
      _incrementTimeThisTick = true;

    }
    else if (_streamingAnimation != nullptr)
    {
      // TODO(str): VIC-13519 Linearize Face Rendering 
      // IMHO Interpolation doesn't belong here. We already have a concept of tracks for 
      // specialized KeyFrame handling, the tracks should be responsible for returning 
      // appropriate content given a relativeStreamTime and interpolate internally.
      // AnimationStreamer should not be concerned.
      static const bool kStoreFace = true;
      ExtractMessagesRelatedToProceduralTrackComponent(_context,
                                                       _streamingAnimation,
                                                       _proceduralTrackComponent.get(),
                                                       _lockedTracks,
                                                       _relativeStreamTime_ms,
                                                       kStoreFace,
                                                       messageWrapper);

      // AnimationInterpolator is unimplemented and does nothing at all in this call.
      AnimationInterpolator::GetInterpolationMessages(_streamingAnimation,
                                                      kCurrentManualFrameNumber,
                                                      messageWrapper);
      if (kShouldDisplayKeyframeNumber && messageWrapper.haveFaceToSend)
      {
        // Build display str secs:ms
        std::string frameNum = std::to_string(kCurrentManualFrameNumber);

        ColorRGBA color = NamedColors::GREEN;

        const Point2f pos(20,20);
        const float scale = 0.5f;

        messageWrapper.faceImg.DrawText(pos, frameNum, color, scale);
      }
    }

    // A workaround to remove tracks that escaped through the engine process' track locking. This currently
    // happens only for composite weather animations and wake word animations, both of which bypass
    // action system's track locking.
    if (_streamingAnimation != nullptr)
    {
      InvalidateBannedTracks(_streamingAnimation->GetName(), messageWrapper);
    }

    // Send the data
    SendAnimationMessages(messageWrapper);

    // Send animState message
    if (--_numTicsToSendAnimState == 0)
    {
      AnimationState msg;
      msg.lockedTracks             = _lockedTracks;
      msg.tracksInUse              = _tracksInUse;

      RobotInterface::SendAnimToEngine(msg);
      _numTicsToSendAnimState = kAnimStateReportingPeriod_tics;
    }

    return lastResult;
  } // AnimationStreamer::Update()


  void AnimationStreamer::EnableKeepFaceAlive(bool enable, u32 disableTimeout_ms)
  {
    if (s_enableKeepFaceAlive && !enable)
    {
      _proceduralTrackComponent->RemoveKeepFaceAlive(_relativeStreamTime_ms, disableTimeout_ms);

    }
    else if (enable && !s_enableKeepFaceAlive)
    {
      if (_wasAnimationInterruptedWithNothing)
      {
        // The last animation ended without a replacement, but neutral eyes weren't inserted because
        // keepalive was disabled. Now that they're re-enabled, set the neutral eyes.
        SetStreamingAnimation(_neutralFaceAnimation, kNotAnimatingTag);
        _wasAnimationInterruptedWithNothing = false;
      }
    }
    s_enableKeepFaceAlive = enable;
  }
  
  void AnimationStreamer::SetKeepFaceAliveFocus(bool enable)
  {
    _proceduralTrackComponent->SetKeepFaceAliveFocus(enable);
  }

  const std::string AnimationStreamer::GetStreamingAnimationName() const
  {
    return _streamingAnimation ? _streamingAnimation->GetName() : "";
  }

  bool AnimationStreamer::IsStreamingAnimFinished() const
  {
    return _endOfAnimationSent &&
           (_streamingAnimation != nullptr) &&
           !_streamingAnimation->HasFramesLeft();
  }

  void AnimationStreamer::ResetKeepFaceAliveLastStreamTimeout()
  {
    _longEnoughSinceLastStreamTimeout_s = kDefaultLongEnoughSinceLastStreamTimeout_s;
  }

  void AnimationStreamer::ProcessAddOrUpdateEyeShift(const RobotInterface::AddOrUpdateEyeShift& msg)
  {
    const std::string layerName(msg.name, msg.name_length);
    _proceduralTrackComponent->AddOrUpdateEyeShift(layerName,
                                                   msg.xPix,
                                                   msg.yPix,
                                                   msg.duration_ms,
                                                   _relativeStreamTime_ms,
                                                   msg.xMax,
                                                   msg.yMax,
                                                   msg.lookUpMaxScale,
                                                   msg.lookDownMinScale,
                                                   msg.outerEyeScaleIncrease);
  }

  void AnimationStreamer::ProcessRemoveEyeShift(const RobotInterface::RemoveEyeShift& msg)
  {
    const std::string layerName(msg.name, msg.name_length);
    _proceduralTrackComponent->RemoveEyeShift(layerName, _relativeStreamTime_ms, msg.disableTimeout_ms);
  }

  void AnimationStreamer::ProcessAddSquint(const RobotInterface::AddSquint& msg)
  {
    const std::string layerName(msg.name, msg.name_length);
    _proceduralTrackComponent->AddSquint(layerName,
                                         msg.squintScaleX,
                                         msg.squintScaleY,
                                         msg.upperLidAngle,
                                         _relativeStreamTime_ms);
  }

  void AnimationStreamer::ProcessRemoveSquint(const RobotInterface::RemoveSquint& msg)
  {
    const std::string layerName(msg.name, msg.name_length);
    _proceduralTrackComponent->RemoveSquint(layerName, _relativeStreamTime_ms, msg.disableTimeout_ms);
  }

  void AnimationStreamer::StopTracks(const u8 whichTracks)
  {
    if (whichTracks)
    {
      if (whichTracks & (u8)AnimTrackFlag::HEAD_TRACK)
      {
        RobotInterface::MoveHead msg;
        msg.speed_rad_per_sec = 0;
        RobotInterface::SendAnimToRobot(std::move(msg));
      }

      if (whichTracks & (u8)AnimTrackFlag::LIFT_TRACK)
      {
        RobotInterface::MoveLift msg;
        msg.speed_rad_per_sec = 0;
        RobotInterface::SendAnimToRobot(std::move(msg));
      }

      if (whichTracks & (u8)AnimTrackFlag::BODY_TRACK)
      {
        RobotInterface::DriveWheels msg;
        msg.lwheel_speed_mmps = 0;
        msg.rwheel_speed_mmps = 0;
        msg.lwheel_accel_mmps2 = 0;
        msg.rwheel_accel_mmps2 = 0;
        RobotInterface::SendAnimToRobot(std::move(msg));
      }

      _tracksInUse &= ~whichTracks;
    }
  }

  void AnimationStreamer::StopTracksInUse(bool aborting) {
      if (!aborting)
      {
        // The anim has terminated normally so just let head and lift settle to final positions
        _tracksInUse &= ~((u8)AnimTrackFlag::LIFT_TRACK | (u8)AnimTrackFlag::HEAD_TRACK);
      }
      StopTracks(_tracksInUse);
    }

  void AnimationStreamer::SendAnimationToWebViz( bool starting ) const
  {
    if (_context != nullptr)
    {
      auto* webService = _context->GetWebService();
      if ((webService != nullptr) && (_streamingAnimation != nullptr))
      {
        if (webService->IsWebVizClientSubscribed(kWebVizModuleName))
        {
          Json::Value data;
          data["type"] = starting ? "start" : "stop";
          data["animation"] = _streamingAnimation->GetName();
          webService->SendToWebViz(kWebVizModuleName, data);
        }
      }
    }
  }

  void AnimationStreamer::CopyIntoProceduralAnimation(Animation* desiredAnim)
  {
    Util::SafeDelete(_proceduralAnimation);
    if (desiredAnim != nullptr)
    {
      _proceduralAnimation = new Animation(*desiredAnim);
    }
    else
    {
      _proceduralAnimation = new Animation();
    }
    _proceduralAnimation->SetName(EnumToString(AnimConstants::PROCEDURAL_ANIM));
  }

  // TODO(str): VIC-13519 Linearize Face Rendering 
  // This function should be replaced by directly rendering the procedural face. Treating it like a
  // full-screen image here after its been built is very wasteful
  void AnimationStreamer::InsertStreamableFaceIntoCompImg(Vision::ImageRGB565& streamableFace,
                                                          Vision::CompositeImage& image)
  {
    ANKI_CPU_PROFILE("AnimationStreamer::InsertStreamableFaceIntoCompImg");
    auto* rgbaImg = new Vision::ImageRGBA(streamableFace.GetNumRows(), streamableFace.GetNumCols());
    rgbaImg->SetFromRGB565(streamableFace);
    auto handle = std::make_shared<Vision::SpriteWrapper>(rgbaImg);

    static const float kEyeSpriteBoxAlpha = 100.0f;
    static const uint16_t kEyeSpriteBoxXpos = 0;
    static const uint16_t kEyeSpriteBoxYpos = 0;
    static const uint8_t kEyeSpriteBoxPadding = 0;
    static const Vision::SpriteBox kEyeSpriteBox {
      kEyeSpriteBoxAlpha,
      kEyeSpriteBoxXpos,
      kEyeSpriteBoxYpos,
      FACE_DISPLAY_WIDTH,
      FACE_DISPLAY_HEIGHT,
      Vision::SpriteBoxName::SpriteBox_40,
      Vision::LayerName::Procedural_Eyes,
      Vision::SpriteRenderMethod::RGBA,
      kEyeSpriteBoxPadding
    };

    image.AddImage(kEyeSpriteBox, handle);
  }

  void AnimationStreamer::InvalidateBannedTracks(const std::string& animName,
                                                 AnimationMessageWrapper& messageWrapper) const
  {
    const bool needToCheckWhitelist = _onCharger &&
                                      ((_lockedTracks & (u8)AnimTrackFlag::BODY_TRACK) == 0);
    
    // note: this duplicates engine's animation_whitelist.json, but hopefully InvalidateBannedTracks is removed soon
    //
    // Note: Any animation name that begins with or is equal to the provided string will be considered. E.g. if
    // "anim_test_" appears in the list, it will match "anim_test_01", "anim_test_02", "anim_test_drive_01", etc.
    static const std::vector<std::string> whitelisted = {
      "anim_chargerdocking_comeoff_",
      "anim_chargerdocking_settle_",
      "anim_movement_forward_",
      "anim_onboarding_driveoff_charger_",
      "anim_onboarding_wakeup_",
    };
    
    bool animWhitelisted = false;
    if (needToCheckWhitelist) {
      for (const auto& listEntry : whitelisted) {
        if (Util::StringStartsWith(animName, listEntry)) {
          animWhitelisted = true;
          break;
        }
      }
    }

    if (_onCharger && _frozenOnCharger)
    {
      // When on charger, don't move or play audio! (This could be alexa acoustic test mode)
      // Don't lock tracks so as to not disturb any other functionality, simply drop the messages
      // on the floor.
      Anki::Util::SafeDelete(messageWrapper.bodyMotionMessage);
      Anki::Util::SafeDelete(messageWrapper.moveLiftMessage);
      Anki::Util::SafeDelete(messageWrapper.moveHeadMessage);
      Anki::Util::SafeDelete(messageWrapper.audioKeyFrameMessage);
    }
    else if (needToCheckWhitelist && !animWhitelisted)
    {
      Anki::Util::SafeDelete(messageWrapper.bodyMotionMessage);

      if (ANKI_DEV_CHEATS)
      {
        // A list of known issues where animations are used without locking tracks on the charger
        static const std::set<std::string> knownIssues = {
          "anim_lookatphone_loop_01",
          "anim_onboarding_wakeword_getin_01",
          "anim_avs_l_getin_03",
          "anim_avs_suddenspeak_03",
          "PROCEDURAL_ANIM",
        };
        if (knownIssues.find(animName) == knownIssues.end())
        {
          LOG_WARNING("AnimationStreamer.InvalidateBannedTracks.UnknownIssue",
                      "Animation '%s' did not have its body track locked when on the charger",
                      animName.c_str());
        }
      }
    }
  }

  void AnimationStreamer::SetFrozenOnCharger(bool enabled)
  {
    const bool wasFrozen = _onCharger && _frozenOnCharger;
    _frozenOnCharger = enabled;
    const bool isFrozen = _onCharger && _frozenOnCharger;
    if (wasFrozen != isFrozen)
    {
      _proceduralTrackComponent->EnableProceduralAudio(!isFrozen);
    }
  }

  void AnimationStreamer::SetOnCharger(bool onCharger)
  {
    const bool wasFrozen = _onCharger && _frozenOnCharger;
    _onCharger = onCharger;
    const bool isFrozen = _onCharger && _frozenOnCharger;
    if (wasFrozen != isFrozen)
    {
      _proceduralTrackComponent->EnableProceduralAudio(!isFrozen);
    }
  }

} // namespace Anim
} // namespace Vector
} // namespace Anki
