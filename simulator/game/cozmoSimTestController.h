/*
 * File:          cozmoSimTestController.h
 * Date:
 * Description:   Any UI/Game to be run as a Webots controller should be derived from this class.
 * Author:
 * Modifications:
 */

#ifndef __COZMO_SIM_TEST_CONTROLLER__H__
#define __COZMO_SIM_TEST_CONTROLLER__H__


#include "simulator/game/uiGameController.h"
#include "util/helpers/variadicMacroHelpers.h"

namespace Anki {
namespace Vector {


// Registration of test controller derived from CozmoSimTestController
#define REGISTER_COZMO_SIM_TEST_CLASS(CLASS) static CozmoSimTestRegistrar<CLASS> registrar(#CLASS);


////////// Macros for condition checking and exiting ////////

#define CST_EXIT()  ExitTest();

#define DEFAULT_TIMEOUT 10

#define CST_EXPECT(x, errorStreamOutput) \
if (!(x)) { \
  PRINT_STREAM_WARNING("CST_EXPECT", "(" << #x << "): " << errorStreamOutput << "(" << __FILE__ << "." << __FUNCTION__ << "." << __LINE__ << ")"); \
  _result = -1; \
}

#define CST_ASSERT(x, errorStreamOutput) \
if (!(x)) { \
  PRINT_STREAM_WARNING("CST_ASSERT", "(" << #x << "): " << errorStreamOutput << "(" << __FILE__ << "." << __FUNCTION__ << "." << __LINE__ << ")"); \
  _result = -1; \
  CST_EXIT(); \
}

// Returns evaluation of condition until timeout seconds past sinceTime
// at which point it asserts on the condition.
#define CONDITION_WITH_TIMEOUT_ASSERT(cond, start_time, timeout) (IsTrueBeforeTimeout(cond, #cond, start_time, timeout, __FILE__, __FUNCTION__, __LINE__))

// Start of if block which is entered if condition evaluates to true
// until timeout seconds past the first time this line is reached
// at which point it asserts on the condition.
#define COMBINE1(X,Y) X##Y  // helper macro
#define COMBINE(X,Y) COMBINE1(X,Y)

#define IF_CONDITION_WITH_TIMEOUT_ASSERT(cond, timeout) static double COMBINE(startTime,__LINE__) = GetSupervisor().getTime(); if (IsTrueBeforeTimeout(cond, #cond, COMBINE(startTime,__LINE__), timeout, __FILE__, __FUNCTION__, __LINE__))

#define IF_ALL_CONDITIONS_WITH_TIMEOUT_ASSERT(timeout, ...) static double COMBINE(startTime,__LINE__) = GetSupervisor().getTime(); \
                                                            if(AllTrueBeforeTimeout({__VA_ARGS__}, {PP_STRINGIZE_X(__VA_ARGS__)}, COMBINE(startTime,__LINE__), timeout, __FILE__, __FUNCTION__, __LINE__))



// Derived classes should create an enum class called TestState, and a variable called _testState.
// They should follow the pattern of modifying test state via this macro instead of directly.
#define SET_TEST_STATE(s) { PRINT_NAMED_INFO("CozmoSimTestController.TransitionTestState", "%s", #s); _testState = TestState::s; }


/////////////// CozmoSimTestController /////////////////

// Base class from which all cozmo simulation tests should be derived
class CozmoSimTestController : public UiGameController {

public:
  CozmoSimTestController();
  virtual ~CozmoSimTestController();

  void SetQuitWebotsAfterTest(bool b=true) { _quitWebotsAfterTest = b; }

protected:

  virtual s32 UpdateInternal() override final;
  virtual s32 UpdateSimInternal() = 0;
  virtual void InitInternal() override final { };

  void ExitTest();

  u8 _result = RESULT_OK;
  bool _isRecording;

  // If set to true, Webots will automatically exit after the test is complete.
  bool _quitWebotsAfterTest = false;

  //Variables for taking screenshots
  f32 _screenshotInterval;
  double _timeOfLastScreenshot;
  std::string _screenshotID;
  int _screenshotNum;

  PathMotionProfile _defaultTestMotionProfile;

  bool IsTrueBeforeTimeout(bool cond,
                           const char* condAsString,
                           double start_time,
                           double timeout,
                           const char* file,
                           const char* func,
                           int line);

  bool AllTrueBeforeTimeout(const std::vector<bool>& conditionBools,
                            const std::vector<std::string>& conditionStrings,
                            double start_time,
                            double timeout,
                            const char* file,
                            const char* func,
                            int line);

  //Only runs if #define RECORD_TEST 1, use for local testing
  void StartMovieConditional(const std::string& name, int speed = 1);

  //Use for movies on teamcity - be sure to add to build artifacts
  void StartMovieAlways(const std::string& name, int speed = 1);
  void StopMovie();

  //Use to take regular screenshots - on the build server this is preferable to recording movies
  void TakeScreenshotsAtInterval(const std::string& screenshotID, f32 interval);

  void MakeSynchronous();

  void DisableRandomPathSpeeds();

  // call in the update loop to occasionally print info about blocks
  void PrintPeriodicBlockDebug();
  void SetBlockDebugPrintInterval(double interval_s) { _printInterval_s = interval_s; }

  double _nextPrintTime = -1.0f;
  double _printInterval_s = 1.0;

  bool IsRobotPoseCorrect(const Point3f& distThreshold, const Radians& angleThreshold,
                          const Pose3d& transform = Pose3d()) const;

  bool IsObjectPoseWrtRobotCorrect(s32 objectID,
                                   const Pose3d& actualPose,
                                   const Point3f& distThresh_mm,
                                   const Radians& angleThresh,
                                   const char* debugStr) const;

  // Send a message to force the robot to delov
  bool IsLocalizedToObject() const;

  // Hiding UiGameController's implementation in order to add asserts on send failure
  Result SendMessage(const ExternalInterface::MessageGameToEngine& msg);

  virtual void HandleRobotConnected(const ExternalInterface::RobotConnectionResponse& msg) override;

}; // class CozmoSimTestController



/////////////// CozmoSimTestFactory /////////////////

// Factory for creating and registering tests derived from CozmoSimTestController
class CozmoSimTestFactory
{
public:

  static CozmoSimTestFactory * getInstance()
  {
    static CozmoSimTestFactory factory;
    return &factory;
  }

  std::shared_ptr<CozmoSimTestController> Create(std::string name);

  void RegisterFactoryFunction(std::string name,
                               std::function<CozmoSimTestController*(void)> classFactoryFunction);

protected:
  std::map<std::string, std::function<CozmoSimTestController*(void)>> factoryFunctionRegistry;
};


template<class T>
class CozmoSimTestRegistrar {
public:
  CozmoSimTestRegistrar(std::string className)
  {
    // register the class factory function
    CozmoSimTestFactory::getInstance()->RegisterFactoryFunction(className,
                                                                [](void) -> CozmoSimTestController * { return new T();});
  }

};


} // namespace Vector
} // namespace Anki

#endif // __COZMO_SIM_TEST_CONTROLLER__H__
