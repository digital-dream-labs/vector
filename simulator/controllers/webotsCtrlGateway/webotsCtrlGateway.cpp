/*
 * File:          webotsCtrlGateway.cpp
 * Date:          07/17/2018
 * Description:   Wrapper controller which simply invokes vic-gateway in the background
 * Author:        Matt Michini
 */

#include <webots/Supervisor.hpp>

webots::Supervisor supervisor;

int main(int argc, char **argv)
{
  // Generate the full path to vic-gateway
  const std::string thisPath(argv[0]);
  const size_t pos = std::string(thisPath).rfind('/');
  const auto& thisDir = thisPath.substr(0,pos+1);
  const auto& gatewayPath = thisDir + "vic-gateway";

  // Spawn vic-gateway in the background. Note that since this is being spawned in the background,
  // it will continue to run even if the Webots simulation is paused. Also, stdout and stderr from
  // vic-gateway will still make it to the Webots console.
  printf("Spawning instance of vic-gateway\n");
  system((gatewayPath + " &").c_str());

  // Use a large time step since our loop doesn't actually do anything
  const auto timestep = 20.0 * supervisor.getBasicTimeStep();
  while (supervisor.step(timestep) != -1) {
    // nothing to do
  }
  
  // Send a SIGTERM to vic-gateway
  printf("Killing vic-gateway\n");
  system("pkill -15 vic-gateway");
  
  return 0;
}
