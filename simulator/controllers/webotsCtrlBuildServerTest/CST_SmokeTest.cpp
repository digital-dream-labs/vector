/**
 * File: CST_SmokeTest.cpp
 *
 * Author: Matt Michini
 * Created: 7/3/17
 *
 * Description: This test should always pass. It's used to make sure that the webots tests
 *              are starting properly on the build servers.
 *
 * Copyright: Anki, inc. 2017
 *
 */

#include "simulator/game/cozmoSimTestController.h"

namespace Anki {
namespace Vector {
    
// ============ Test class declaration ============
class CST_SmokeTest : public CozmoSimTestController {
  
private:
  
  virtual s32 UpdateSimInternal() override;
  
  double _startTime_s = 0.0;
};

// Register class with factory
REGISTER_COZMO_SIM_TEST_CLASS(CST_SmokeTest);

// =========== Test class implementation ===========

s32 CST_SmokeTest::UpdateSimInternal()
{
  // Simply wait a few seconds and end the test
  const double waitTime_s = 5.0;
  
  if (_startTime_s == 0.0) {
    _startTime_s = GetSupervisor().getTime();
  } else if (GetSupervisor().getTime() > _startTime_s + waitTime_s) {
    CST_EXIT();
  }
  
  return _result;
}
  
} // end namespace Vector
} // end namespace Anki

