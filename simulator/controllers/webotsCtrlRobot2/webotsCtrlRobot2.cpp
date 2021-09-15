/*
 * File:          webotsCtrlRobot2.cpp
 * Date:
 * Description:   Cozmo 2.0 robot process
 * Author:
 * Modifications:
 */


#include "../shared/ctrlCommonInitialization.h"
#include "anki/cozmo/robot/cozmoBot.h"
#include "simulator/robot/sim_overlayDisplay.h"
#include "anki/cozmo/robot/hal.h"
#include "anki/cozmo/shared/factory/emrHelper.h"
#include <cstdio>
#include <sstream>

#include <webots/Supervisor.hpp>

/*
 * This is the main program.
 * The arguments of the main function can be specified by the
 * "controllerArgs" field of the Robot node
 */

namespace Anki {
  namespace Vector {
    namespace Sim {
      extern webots::Supervisor* CozmoBot;
    }
  }
}


int main(int argc, char **argv)
{
  using namespace Anki;
  using namespace Anki::Vector;

  Factory::CreateFakeEMR();

  // Placeholder for SIGTERM flag
  int shutdownSignal = 0;

  // parse commands
  WebotsCtrlShared::ParsedCommandLine params = WebotsCtrlShared::ParseCommandLine(argc, argv);
  // create platform
  const Anki::Util::Data::DataPlatform& dataPlatform = WebotsCtrlShared::CreateDataPlatformBS(argv[0], "webotsCtrlRobot2");
  // initialize logger
  WebotsCtrlShared::DefaultAutoGlobalLogger autoLogger(dataPlatform, params.filterLog, params.colorizeStderrOutput);

  if(Robot::Init(&shutdownSignal) != Anki::RESULT_OK) {
    fprintf(stdout, "Failed to initialize Vector::Robot!\n");
    return -1;
  }

  Sim::OverlayDisplay::Init();

  HAL::Step();

  while(Robot::step_MainExecution() == Anki::RESULT_OK)
  {
    HAL::UpdateDisplay();
    HAL::Step();
  }

  return 0;
}
