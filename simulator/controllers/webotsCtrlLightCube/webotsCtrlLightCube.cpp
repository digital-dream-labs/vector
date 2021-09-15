/*
 * File:          webotsCtrlLightCube.cpp
 * Date:
 * Description:   Webots controller for active block
 * Author:        
 * Modifications: 
 */
#include "activeBlock.h"
#include "../shared/ctrlCommonInitialization.h"
#include "coretech/common/shared/types.h"
#include <cstdio>
#include <string>

/*
 * This is the main program.
 * The arguments of the main function can be specified by the
 * "controllerArgs" field of the Robot node
 */
int main(int argc, char **argv)
{
  using namespace Anki;
  using namespace Anki::Vector;

  // parse commands
  WebotsCtrlShared::ParsedCommandLine params = WebotsCtrlShared::ParseCommandLine(argc, argv);
  // create platform
  const Anki::Util::Data::DataPlatform& dataPlatform = WebotsCtrlShared::CreateDataPlatformBS(argv[0], "webotsCtrlLightCube");
  // initialize logger
  WebotsCtrlShared::DefaultAutoGlobalLogger autoLogger(dataPlatform, params.filterLog, params.colorizeStderrOutput);
  
  if (ActiveBlock::Init() == Anki::RESULT_FAIL) {
    printf("ERROR (webotsCtrlLightCube): Failed to init block");
    return -1;
  }
  
  while(ActiveBlock::Update() == Anki::RESULT_OK)
  {
  }
  
  ActiveBlock::DeInit();
  
  return 0;
}
