/**
 * File: cozmoContext.h
 *
 * Author: Lee Crippen
 * Created: 1/29/2016
 *
 * Description: Holds references to components and systems that are used often by all different parts of code,
 *              where it is unclear who the appropriate owner of that system would be.
 *              NOT intended to be a container to hold ALL systems and components, which would simply be lazy.
 *
 * Copyright: Anki, Inc. 2016
 *
 **/

#ifndef __Cozmo_Basestation_CozmoContext_H__
#define __Cozmo_Basestation_CozmoContext_H__


#include "util/helpers/noncopyable.h"
#include <memory>


// ---------- BEGIN FORWARD DECLARATIONS ----------
namespace Anki {
namespace Util {
  class RandomGenerator;
  class Locale;
  namespace AnkiLab {
    class AnkiLab;
  }
  namespace Data {
    class DataPlatform;
  }
}

namespace Comms {
  class AdvertisementService;
}

namespace Vector {

class CozmoAudienceTags;
class CozmoExperiments;
class CozmoFeatureGate;
class IExternalInterface;
class IGatewayInterface;
class RobotDataLoader;
class RobotManager;
class VizManager;
class PerfMetricEngine;
class RobotTest;

namespace WebService {
  class WebService;
}

class ThreadIDInternal;

enum class SdkStatusType : uint8_t;

namespace RobotInterface {
  class MessageHandler;
}

} // namespace Vector
} // namespace Anki

// ---------- END FORWARD DECLARATIONS ----------



// Here begins the actual namespace and interface for CozmoContext
namespace Anki {
namespace Vector {

class CozmoContext : private Util::noncopyable
{

public:
  CozmoContext(Util::Data::DataPlatform* dataPlatform, IExternalInterface* externalInterface, IGatewayInterface* gatewayInterface);
  CozmoContext();
  virtual ~CozmoContext();

  IExternalInterface*                   GetExternalInterface() const { return _externalInterface; }
  IGatewayInterface*                    GetGatewayInterface() const { return _gatewayInterface; }
  Util::Data::DataPlatform*             GetDataPlatform() const { return _dataPlatform; }

  CozmoFeatureGate*                     GetFeatureGate() const { return _featureGate.get(); }
  Util::RandomGenerator*                GetRandom() const { return _random.get(); }
  Util::Locale*                         GetLocale() const { return _locale.get(); }
  RobotDataLoader*                      GetDataLoader() const { return _dataLoader.get(); }
  RobotManager*                         GetRobotManager() const { return _robotMgr.get(); }
  VizManager*                           GetVizManager() const { return _vizManager.get(); }
  CozmoExperiments*                     GetExperiments() const { return _cozmoExperiments.get(); }
  PerfMetricEngine*                     GetPerfMetric() const { return _perfMetric.get(); }
  WebService::WebService*               GetWebService() const { return _webService.get(); }
  RobotTest*                            GetRobotTest() const { return _robotTest.get(); }

  void SetSdkStatus(SdkStatusType statusType, std::string&& statusText) const;

  void SetRandomSeed(uint32_t seed);

  void SetLocale(const std::string& locale);

  // Designate current thread as the "main thread" used for engine updates.
  void SetEngineThread();

  // Returns true if the current thread is the "main thread" used for engine updates.
  // Requires SetEngineThread (above) to have been called.
  bool IsEngineThread() const;

  // Perform orderly shutdown of components
  void Shutdown();

private:
  // This is passed in and held onto, but not owned by the context (yet.
  // It really should be, and that refactoring will have to happen soon).
  IExternalInterface*                                   _externalInterface = nullptr;
  IGatewayInterface*                                    _gatewayInterface = nullptr;
  Util::Data::DataPlatform*                             _dataPlatform = nullptr;

  // for holding the thread id (and avoiding need to include cpuThreadId.h here)
  std::unique_ptr<ThreadIDInternal> _threadIdHolder;

  // Context holds onto these things for everybody:
  std::unique_ptr<CozmoFeatureGate>                     _featureGate;
  std::unique_ptr<Util::RandomGenerator>                _random;
  std::unique_ptr<Util::Locale>                         _locale;
  std::unique_ptr<RobotDataLoader>                      _dataLoader;
  std::unique_ptr<RobotManager>                         _robotMgr;
  std::unique_ptr<VizManager>                           _vizManager;
  std::unique_ptr<CozmoExperiments>                     _cozmoExperiments;
  std::unique_ptr<PerfMetricEngine>                     _perfMetric;
  std::unique_ptr<WebService::WebService>               _webService;
  std::unique_ptr<RobotTest>                            _robotTest;
};


} // namespace Vector
} // namespace Anki

#endif // __Cozmo_Basestation_CozmoContext_H__
