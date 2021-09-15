/**
* File: webotsCtrlCozmoSimTest.cpp
*
* Author: Kevin Yoon
* Created: 8/18/2015
*
* Description: Main for running build server tests
*
* Copyright: Anki, inc. 2015
*
*/

#include "../shared/ctrlCommonInitialization.h"
#include <stdio.h>
#include <string.h>
#include "simulator/game/cozmoSimTestController.h"

using namespace Anki;
using namespace Anki::Vector;

void QuitWebots(int status)
{
  webots::Supervisor dummySupervisor;
  dummySupervisor.simulationQuit(status);
}

int main(int argc, char **argv)
{
  // Note: we don't allow logFiltering in BuildServerTest like we do in the other controllers because this
  // controller is meant to show all logs.

  // create platform
  const Anki::Util::Data::DataPlatform& dataPlatform = WebotsCtrlShared::CreateDataPlatformTest(argv[0], "webotsCtrlBuildServer");
  
  // initialize logger
  const bool filterLog = false;
  const bool colorizeStderrOutput = false;
  WebotsCtrlShared::DefaultAutoGlobalLogger autoLogger(dataPlatform, filterLog, colorizeStderrOutput);
  
  // Create specified test controller.
  // Only a single argument is supported and it must the name of a valid test.
  if (argc < 2) {
    PRINT_NAMED_ERROR("WebotsCtrlBuildServerTest.main.NoTestSpecified","");
    QuitWebots(-1);
  }
  
  std::string testName(argv[1]);
  auto cstCtrl = CozmoSimTestFactory::getInstance()->Create(testName);
  if (nullptr == cstCtrl)
  {
    PRINT_NAMED_ERROR("WebotsCtrlBuildServerTest.main.TestNotFound", "'%s' test not found", testName.c_str());
    QuitWebots(-1);
  }
  
  // Check for flag indicating whether or not webots should continue running after the test is complete.
  const bool quitAfterTest = (argc >= 3) && (0 == strcmp(argv[2], "--quitWebotsAfterTest"));
  cstCtrl->SetQuitWebotsAfterTest(quitAfterTest);
  
  PRINT_NAMED_INFO("WebotsCtrlBuildServerTest.main.StartingTest", "%s", testName.c_str());
  
  // Init test
  cstCtrl->Init();
  // Run update loop
  while (cstCtrl->Update() == 0) {}

  return 0;
}

