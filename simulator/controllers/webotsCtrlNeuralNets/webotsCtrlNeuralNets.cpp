/**
 * File: webotsCtrlNeuralNets.cpp
 *
 * Author: Andrew Stein
 * Date:   1/29/2018
 *
 * Description: Implements Webots-specific INeuralNetMain interface and main() to create the WebotsCtrlNeuralNets
 *              "controller" for use in the simulator.
 *
 * Copyright: Anki, Inc. 2018
 **/

#ifndef SIMULATOR
# error Expecting SIMULATOR to be defined when building this file
#endif

#include "coretech/neuralnets/iNeuralNetMain.h"

#include "util/logging/logging.h"
#include "util/logging/printfLoggerProvider.h"
#include "util/logging/multiFormattedLoggerProvider.h"

#include <webots/Supervisor.hpp>

#define LOG_CHANNEL "NeuralNets"

namespace Anki {
  
class WebotsCtrlNeuralNet : public NeuralNets::INeuralNetMain
{
public:
  WebotsCtrlNeuralNet() { }
  
protected:
  
  virtual bool ShouldShutdown() override
  {
    return _shouldStop;
  }
  
  virtual Util::ILoggerProvider* GetLoggerProvider() override
  {
    const bool colorizeStderrOutput = false; // TODO: Get from Webots proto in simulation?
    _logger.reset(new Util::PrintfLoggerProvider(Util::LOG_LEVEL_DEBUG, colorizeStderrOutput));
    return _logger.get();
  }
  
  virtual int GetPollPeriod_ms(const Json::Value& config) const override
  {
    return _webotsSupervisor.getSelf()->getField("pollingPeriod_ms")->getSFInt32();
  }
  
  virtual void Step(int pollPeriod_ms) override
  {
    const int rc = _webotsSupervisor.step(pollPeriod_ms);
    _shouldStop = (rc == -1);
  }
  
private:
  webots::Supervisor _webotsSupervisor;
  std::unique_ptr<Util::PrintfLoggerProvider> _logger;
  bool _shouldStop = false;
};

} // namespace Anki

int main(int argc, char** argv)
{
  if(argc < 3)
  {
    LOG_ERROR("WebotsCtrlNeuralNets.Main.UnexpectedArguments", "");
    std::cout << std::endl << "Usage: " << argv[0] << " <configFile>.json modelPath cachePath" << std::endl;
    std::cout << std::endl << " Will poll cachePath for neuralNetImage.png to process" << std::endl;
    return -1;
  }
  
  Anki::WebotsCtrlNeuralNet vicNeuralNetMain;
  
  Anki::Result result = Anki::RESULT_OK;
  
  result = vicNeuralNetMain.Init(argv[1], argv[2], argv[3]);
  
  if(Anki::RESULT_OK == result)
  {
    result = vicNeuralNetMain.Run();
  }
  
  if(Anki::RESULT_OK == result)
  {
    LOG_INFO("WebotsCtrlNeuralNets.Completed.Success", "");
    return 0;
  }
  else
  {
    LOG_ERROR("WebotsCtrlNeuralNets.Completed.Failure", "Result:%d", result);
    return -1;
  }
}


