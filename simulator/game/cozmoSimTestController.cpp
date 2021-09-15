/*
 * File:          cozmoSimTestController.cpp
 * Date:
 * Description:
 * Author:
 * Modifications:
 */

#include "simulator/game/cozmoSimTestController.h"
#include "anki/cozmo/shared/cozmoEngineConfig.h"
#include <sys/stat.h>

// Set to have StartMovie() and StopMovie() do things
// Note: Change _recordingPath to where ever you want the recordings to be saved
// Also may want/need to change starting webots world viewpoint so that you can see everything
// from a good pose
#define RECORD_TEST 0
const static std::string kBuildDirectory = "../../../build/";
//This folder is created with a build script on teamCity - make the folder locally manually for now
const static std::string kScreenShotsPath = kBuildDirectory + "mac/Debug/webots_screenshots/";

namespace Anki {
  namespace Vector {

    CozmoSimTestController::CozmoSimTestController()
    : UiGameController(BS_TIME_STEP_MS)
    , _result(0)
    , _isRecording(false)
    , _screenshotInterval(-1.f)
    , _timeOfLastScreenshot(0.)
    , _screenshotNum(0)
    {
      // Parameters for the default motion profile to be used for webots tests:
      _defaultTestMotionProfile.speed_mmps = 60.f;
      _defaultTestMotionProfile.accel_mmps2 = 200.f;
      _defaultTestMotionProfile.decel_mmps2 = 500.f;
      _defaultTestMotionProfile.pointTurnSpeed_rad_per_sec = 1.5f;
      _defaultTestMotionProfile.pointTurnAccel_rad_per_sec2 = 10.f;
      _defaultTestMotionProfile.pointTurnDecel_rad_per_sec2 = 10.f;
      _defaultTestMotionProfile.dockSpeed_mmps = 60.f;
      _defaultTestMotionProfile.dockAccel_mmps2 = 200.f;
      _defaultTestMotionProfile.dockDecel_mmps2 = 100.f;
      _defaultTestMotionProfile.reverseSpeed_mmps = 30.f;
      _defaultTestMotionProfile.isCustom = true;
    }

    CozmoSimTestController::~CozmoSimTestController()
    { }

    bool CozmoSimTestController::IsTrueBeforeTimeout(bool cond,
                                                     const char* condAsString,
                                                     double start_time,
                                                     double timeout,
                                                     const char* file,
                                                     const char* func,
                                                     int line)
    {
      if (GetSupervisor().getTime() - start_time > timeout) {
        if (!cond) {
          PRINT_STREAM_WARNING("CONDITION_WITH_TIMEOUT_ASSERT", "(" << condAsString << ") still false after " << timeout << " seconds. (" << file << "." << func << "." << line << " started at: " << start_time << ")");
          _result = 255;

          StopMovie();

          CST_EXIT();
        }
      }

      return cond;
    }


    bool CozmoSimTestController::AllTrueBeforeTimeout(const std::vector<bool>& conditionBools,
                                                      const std::vector<std::string>& conditionStrings,
                                                      double start_time,
                                                      double timeout,
                                                      const char* file,
                                                      const char* func,
                                                      int line)
    {
      const bool allTrue = !std::any_of(conditionBools.begin(), conditionBools.end(), [](bool b){ return (b==false); });

      if (allTrue) {
        return true;
      }

      // Some conditions were false. Check for timeout.
      if (GetSupervisor().getTime() - start_time > timeout) {
        DEV_ASSERT(conditionStrings.size() == conditionBools.size(), "CozmoSimTestController.AllTrueBeforeTimeout.NumberOfConditionsMismatch");

        std::ostringstream msg;
        msg << "Conditions: \n\n";
        for (int i=0 ; i < conditionBools.size() ; i++) {
          msg << (conditionBools[i] ? "<TRUE>   " : "<FALSE>  ") << conditionStrings[i] << "\n";
        }

        msg << "\nAbove conditions were still false after " << timeout << " seconds (started at " << start_time << ")\n";
        msg << "File: \"" << file << "\", line " << line << ", in function \"" << func << "()\"";

        PRINT_STREAM_WARNING("ALL_CONDITIONS_WITH_TIMEOUT_ASSERT", msg.str().c_str());
        _result = 255;

        StopMovie();

        CST_EXIT();
      }

      return false;
    }

    void CozmoSimTestController::HandleRobotConnected(const ExternalInterface::RobotConnectionResponse& msg)
    {
      // Robot has connected so make it run synchronously
      MakeSynchronous();
    }

    s32 CozmoSimTestController::UpdateInternal()
    {
      //Check if screenshots need to be taken
      if(_screenshotInterval > 0){

        // Use simulated time intervals to decide _when_ to take the screen shots
        const double simTime = GetSupervisor().getTime();

        if((simTime - _timeOfLastScreenshot) > _screenshotInterval)
        {
          // Use system time to time/date-stamp the screenshot filenames so that
          // subsequent runs (retries) don't step on each other
          time_t now;
          time(&now);
          char timeString[256];
          strftime(timeString, sizeof(timeString), "%F_%H-%M-%S", localtime(&now));

          std::stringstream ss;
          ss << kScreenShotsPath << _screenshotID << "_" << timeString << "_" << _screenshotNum << ".png";
          GetSupervisor().exportImage(ss.str(), 80);

          PRINT_NAMED_INFO("CozmoSimTestController.UpdateInternal.TookScreenshot",
                           "ID:%s Num:%d Time:%s",
                           _screenshotID.c_str(), _screenshotNum, timeString);

          _screenshotNum++;
          _timeOfLastScreenshot = simTime;
        }

      }

      return UpdateSimInternal();
    }

    void CozmoSimTestController::ExitTest()
    {
      if (_quitWebotsAfterTest) {
        QuitWebots(_result); // Terminate the Webots process
      } else {
        QuitController(_result); // Just quit the controller, but keep Webots running
      }
    }

    //Only runs if #define RECORD_TEST 1, use for local testing
    void CozmoSimTestController::StartMovieConditional(const std::string& name, int speed)
    {
      if(RECORD_TEST)
      {
        time_t t;
        time(&t);
        std::string dateStr = std::asctime(std::localtime(&t));
        dateStr.erase(std::remove(dateStr.begin(), dateStr.end(), '\n'), dateStr.end());

        std::stringstream ss;
        ss << name << dateStr;
        StartMovieAlways(name, speed);
        _isRecording = true;
      }
    }

    //Use for movies on teamcity - be sure to add to build artifacts
    void CozmoSimTestController::StartMovieAlways(const std::string& name, int speed)
    {
      time_t t;
      time(&t);
      std::stringstream ss;
      ss << kBuildDirectory << name << ".mp4";
      GetSupervisor().movieStartRecording(ss.str(), 854, 480, 0, 90, speed, false);
      _isRecording =  GetSupervisor().getMovieStatus() == webots::Supervisor::MOVIE_RECORDING;
      PRINT_NAMED_INFO("Is Movie Recording?","_isRecording:%d", _isRecording);
    }



    void CozmoSimTestController::StopMovie()
    {
      if(_isRecording && GetSupervisor().getMovieStatus() == GetSupervisor().MOVIE_RECORDING)
      {
        GetSupervisor().movieStopRecording();
        PRINT_NAMED_INFO("CozmoSimTestController.StopMovie", "Movie Stop Command issued");

        while(GetSupervisor().movieIsReady()){
        }

        if(GetSupervisor().movieFailed()){
          PRINT_NAMED_ERROR("CozmoSimTestController.StopMovie", "Movie failed to save properly");
        }

        _isRecording = false;
      }
    }


    void CozmoSimTestController::TakeScreenshotsAtInterval(const std::string& screenshotID, f32 interval)
    {
      if(interval <= 0){
        PRINT_NAMED_ERROR("CozmoSimTestController.TakeShotsAtInterval.InvalidInterval", "Interval passed in: %f", interval);
        return;
      }

      //Set up output folder
      struct stat st;
      if(stat(kScreenShotsPath.c_str(), &st) != 0){
        system(("mkdir -p " + kScreenShotsPath).c_str());
      }

      _screenshotInterval = interval;
      _screenshotID = screenshotID;

      PRINT_NAMED_INFO("CozmoSimTestController.TakeScreenshotsAtInterval.SettingInterval",
                       "Interval:%f Path:%s", _screenshotInterval, kScreenShotsPath.c_str());
    }

    Result CozmoSimTestController::SendMessage(const ExternalInterface::MessageGameToEngine& msg)
    {
      const Result res = UiGameController::SendMessage(msg);
      CST_ASSERT(res == RESULT_OK, "CozmoSimTestController.SendMessage.Fail");
      return res;
    }

    void CozmoSimTestController::MakeSynchronous()
    {
      // Set vision to synchronous
      ExternalInterface::VisionRunMode m;
      m.isSync = true;
      ExternalInterface::MessageGameToEngine message;
      message.Set_VisionRunMode(m);
      SendMessage(message);
    }

    void CozmoSimTestController::DisableRandomPathSpeeds()
    {
      SendMessage( ExternalInterface::MessageGameToEngine( ExternalInterface::SetEnableSpeedChooser( false ) ) );
    }

    void CozmoSimTestController::PrintPeriodicBlockDebug()
    {
      const double currTime_s = GetSupervisor().getTime();

      if( _nextPrintTime < 0 || currTime_s >= _nextPrintTime ) {
        _nextPrintTime = currTime_s + _printInterval_s;

        for( const auto& objPair : GetObjectPoseMap() ) {
          const auto& objId = objPair.first;
          const auto& estPose = objPair.second;

          PRINT_NAMED_INFO("CozmoSimTest.BlockDebug.Known",
                           "object %d: (%f, %f, %f) theta_z=%fdeg, upAxis=%s%s",
                           objId,
                           estPose.GetTranslation().x(),
                           estPose.GetTranslation().y(),
                           estPose.GetTranslation().z(),
                           estPose.GetRotationAngle<'Z'>().getDegrees(),
                           AxisToCString( estPose.GetRotationMatrix().GetRotatedParentAxis<'Z'>() ),
                           GetCarryingObjectID() == objId ? " CARRIED" : "");

          ObjectType objType;
          if (GetObjectType(objId, objType) != RESULT_OK) {
            PRINT_NAMED_WARNING("CozmoSimTest.BlockDebug.Actual",
                                "Could not get object type for objId = %d",
                                objId);
          } else {
            if( HasActualLightCubePose(objType) ) {
              const Pose3d pose = GetLightCubePoseActual(objType);
              PRINT_NAMED_INFO("CozmoSimTest.BlockDebug.Actual",
                               "object %d: (%f, %f, %f) theta_z=%fdeg, upAxis=%s%s",
                               objId,
                               pose.GetTranslation().x(),
                               pose.GetTranslation().y(),
                               pose.GetTranslation().z(),
                               pose.GetRotationAngle<'Z'>().getDegrees(),
                               AxisToCString( pose.GetRotationMatrix().GetRotatedParentAxis<'Z'>() ),
                               GetCarryingObjectID() == objId ? " CARRIED" : "");
            }
            else {
              // TODO:(bn) warning. Could be a non-lightcube though
            }
          }


        }
      }
    }

    bool CozmoSimTestController::IsRobotPoseCorrect(const Point3f& distThreshold, const Radians& angleThreshold, const Pose3d& transform) const
    {
      Pose3d robotPose(GetRobotPose());
      robotPose.PreComposeWith(transform); // preserves robot pose's parent

      Pose3d robotPoseActual(GetRobotPoseActual());
      robotPoseActual.SetParent(robotPose.GetParent());

      const bool isSame = robotPose.IsSameAs(robotPoseActual, distThreshold, angleThreshold);
      return isSame;
    }

    bool CozmoSimTestController::IsObjectPoseWrtRobotCorrect(s32 objectID,
                                                             const Pose3d& actualPose,
                                                             const Point3f& distThresh_mm,
                                                             const Radians& angleThresh,
                                                             const char* debugStr) const
    {
      const PoseOrigin fakeOrigin; // declare before any poses that use it as parent so it destructs last!

      Pose3d objectPoseWrtRobot;

      if(RESULT_OK != GetObjectPose(objectID, objectPoseWrtRobot))
      {
        PRINT_NAMED_WARNING("CozmoSimTestController.IsObjectPoseWrtRobotCorrect",
                            "%s: Could not get object %d's pose",
                            debugStr, objectID);
        return false;
      }

      if(false == objectPoseWrtRobot.GetWithRespectTo(GetRobotPose(), objectPoseWrtRobot))
      {
        PRINT_NAMED_WARNING("CozmoSimTestController.IsObjectPoseWrtRobotCorrect",
                            "%s: Could not get object %d's pose w.r.t. robot",
                            debugStr, objectID);
        return false;
      }

      // Assume that actual object pose and actual robot pose are in the same origin
      Pose3d robotPoseActual(GetRobotPoseActual());
      robotPoseActual.SetParent(actualPose.GetParent());

      Pose3d actualObjectPoseWrtRobot;

      if(false == actualPose.GetWithRespectTo(robotPoseActual, actualObjectPoseWrtRobot))
      {
        PRINT_NAMED_WARNING("CozmoSimTestController.IsObjectPoseWrtRobotCorrect",
                            "%s: Could not get object %d's actual pose w.r.t. actual robot",
                            debugStr, objectID);
        return false;
      }

      // Assuming the two object poses are w.r.t. the same robot, make them share
      // a common origin and see if the represent the same pose relative to that common
      // origin
      objectPoseWrtRobot.SetParent(fakeOrigin);
      actualObjectPoseWrtRobot.SetParent(fakeOrigin);

      Vec3f Tdiff(0,0,0);
      Radians angleDiff(0);
      if(!objectPoseWrtRobot.IsSameAs(actualObjectPoseWrtRobot, distThresh_mm, angleThresh, Tdiff, angleDiff))
      {
        PRINT_NAMED_WARNING("CozmoSimTestController.IsObjectPoseWrtRobotCorrect",
                            "%s: object %d's observed and actual poses do not match [Tdiff=(%.1f,%.1f,%.1f) angleDiff=%.1fdeg]",
                            debugStr, objectID,
                            Tdiff.x(), Tdiff.y(), Tdiff.z(), angleDiff.getDegrees());
        return false;
      }

      return true;
    }

    bool CozmoSimTestController::IsLocalizedToObject() const
    {
      return GetRobotState().localizedToObjectID >= 0;
    }

    // =========== CozmoSimTestFactory ============

    std::shared_ptr<CozmoSimTestController> CozmoSimTestFactory::Create(std::string name)
    {
      CozmoSimTestController * instance = nullptr;

      // find name in the registry and call factory method.
      auto it = factoryFunctionRegistry.find(name);
      if(it != factoryFunctionRegistry.end()){
        instance = it->second();
      }
      // wrap instance in a shared ptr and return
      if(instance != nullptr)
        return std::shared_ptr<CozmoSimTestController>(instance);
      else
        return nullptr;
    }

    void CozmoSimTestFactory::RegisterFactoryFunction(std::string name,
                                 std::function<CozmoSimTestController*(void)> classFactoryFunction)
    {
      // register the class factory function
      factoryFunctionRegistry[name] = classFactoryFunction;
    }



  } // namespace Vector
} // namespace Anki
