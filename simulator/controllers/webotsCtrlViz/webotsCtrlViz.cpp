/*
 * File:          webotsCtrlViz.cpp
 * Date:          03-19-2014
 * Description:   Interface for basestation to all visualization functions in Webots including 
 *                cozmo_physics draw functions, display window text printing, and other custom display
 *                methods.
 * Author:        Kevin Yoon
 * Modifications: 
 */

#include "vizControllerImpl.h"
#include "../shared/ctrlCommonInitialization.h"
#include "anki/cozmo/shared/cozmoEngineConfig.h"
#include "clad/types/vizTypes.h"
#include "clad/vizInterface/messageViz.h"
#include "coretech/messaging/shared/UdpServer.h"
#include "coretech/messaging/shared/UdpClient.h"
#include <webots/Supervisor.hpp>
#include <webots/ImageRef.hpp>
#include <webots/Display.hpp>
#include <cstdio>
#include <string>


int main(int argc, char **argv)
{
  using namespace Anki;
  using namespace Anki::Vector;

  // parse commands
  WebotsCtrlShared::ParsedCommandLine params = WebotsCtrlShared::ParseCommandLine(argc, argv);
  // create platform
  const Anki::Util::Data::DataPlatform& dataPlatform = WebotsCtrlShared::CreateDataPlatformBS(argv[0], "webotsCtrlViz");
  // initialize logger
  WebotsCtrlShared::DefaultAutoGlobalLogger autoLogger(dataPlatform, params.filterLog, params.colorizeStderrOutput);

  webots::Supervisor vizSupervisor;
  VizControllerImpl vizController(vizSupervisor);
  
  // If we are using Webots R2018b or later, then openGL support is removed and we cannot use the PhysVizController to
  // draw 3D objects. Instead the VizController should draw such objects.
  const std::string webotsVer = WEBOTS_VER;
  const bool isOldVersion = (webotsVer.find("R2018a") != std::string::npos);
  const bool vizShouldDrawObjects = !isOldVersion;
  vizController.EnableDrawingObjects(vizShouldDrawObjects);
  
  const size_t maxPacketSize{(size_t)VizConstants::MaxMessageSize};
  uint8_t data[maxPacketSize]{0};
  ssize_t numBytesRecvd;
  
  // Setup server to listen for commands
  UdpServer server("webotsCtrlViz");
  server.StartListening((uint16_t)VizConstants::VIZ_SERVER_PORT);
  
  // Setup client to forward relevant commands to cozmo_physics plugin (but only if we need it for drawing objects)
  UdpClient physicsClient;
  if (!vizShouldDrawObjects) {
    physicsClient.Connect("127.0.0.1", (uint16_t)VizConstants::PHYSICS_PLUGIN_SERVER_PORT);
  }
  
  vizController.Init();
  
  //
  // Main Execution loop
  //
  while (vizSupervisor.step(Anki::Vector::BS_TIME_STEP_MS) != -1)
  {
    // Any messages received?
    while ((numBytesRecvd = server.Recv((char*)data, maxPacketSize)) > 0) {
      if (!vizShouldDrawObjects) {
        physicsClient.Send((char*)data, numBytesRecvd);
      }
      vizController.ProcessMessage(VizInterface::MessageViz(data, numBytesRecvd));
    } // while server.Recv
    
    vizController.Update();
  } // while step

  
  return 0;
}

