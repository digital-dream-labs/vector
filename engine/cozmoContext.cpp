/**
 * File: cozmoContext.cpp
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

#include "engine/cozmoContext.h"

#include "coretech/common/engine/utils/data/dataPlatform.h"
#include "engine/externalInterface/externalInterface.h"
#include "engine/perfMetricEngine.h"
#include "engine/robotDataLoader.h"
#include "engine/robotManager.h"
#include "engine/robotTest.h"
#include "engine/utils/cozmoExperiments.h"
#include "engine/utils/cozmoFeatureGate.h"
#include "engine/viz/vizManager.h"
#include "audioEngine/multiplexer/audioMultiplexer.h"
#include "util/cpuProfiler/cpuThreadId.h"
#include "util/environment/locale.h"
#include "util/fileUtils/fileUtils.h"
#include "util/random/randomGenerator.h"
#include "webServerProcess/src/webService.h"


namespace Anki {
namespace Vector {

class ThreadIDInternal : private Util::noncopyable
{
public:
  Util::CpuThreadId _id = Util::kCpuThreadIdInvalid;
};


CozmoContext::CozmoContext(Util::Data::DataPlatform* dataPlatform, IExternalInterface* externalInterface, IGatewayInterface* gatewayInterface)
  : _externalInterface(externalInterface)
  , _gatewayInterface(gatewayInterface)
  , _dataPlatform(dataPlatform)
  , _threadIdHolder(new ThreadIDInternal)
  , _featureGate(new CozmoFeatureGate(dataPlatform))
  , _random(new Anki::Util::RandomGenerator())
  , _locale(new Util::Locale(Util::Locale::GetNativeLocale()))
  , _dataLoader(new RobotDataLoader(this))
  , _robotMgr(new RobotManager(this))
  , _vizManager(new VizManager())
  , _cozmoExperiments(new CozmoExperiments(this))
  , _perfMetric(new PerfMetricEngine(this))
  , _webService(new WebService::WebService())
  , _robotTest(new RobotTest(this))
{
}


CozmoContext::CozmoContext() : CozmoContext(nullptr, nullptr, nullptr)
{

}

CozmoContext::~CozmoContext()
{
}

void CozmoContext::Shutdown()
{
  // Order of destruction matters!  RobotManager makes calls back into context,
  // so manager must be shut down before context is destroyed.
  _robotMgr->Shutdown(ShutdownReason::SHUTDOWN_UNKNOWN);
}


void CozmoContext::SetSdkStatus(SdkStatusType statusType, std::string&& statusText) const
{
  if (_externalInterface)
  {
    _externalInterface->SetSdkStatus(statusType, std::move(statusText));
  }
}

void CozmoContext::SetRandomSeed(uint32_t seed)
{
  _random->SetSeed("CozmoContext", seed);
}


void CozmoContext::SetLocale(const std::string& localeString)
{
  // TODO: VIC-27 - Migrate Audio Local functionality to Victor
  using Locale = Anki::Util::Locale;
//  using CozmoAudioController = Anki::Vector::Audio::CozmoAudioController;

  if (!localeString.empty()) {
    Locale locale = Locale::LocaleFromString(localeString);
    _locale.reset(new Locale(locale));

    // Update audio controller to use new locale preference
//    auto * audioController = (_audioServer ? _audioServer->GetAudioController() : NULL);
//    auto * cozmoAudioController = dynamic_cast<CozmoAudioController*>(audioController);
//    if (nullptr != cozmoAudioController) {
//      cozmoAudioController->SetLocale(*_locale);
//    }
  }
}


void CozmoContext::SetEngineThread()
{
  _threadIdHolder->_id = Util::GetCurrentThreadId();
}

bool CozmoContext::IsEngineThread() const
{
  return Util::AreCpuThreadIdsEqual( _threadIdHolder->_id, Util::GetCurrentThreadId() );
}


} // namespace Vector
} // namespace Anki
