/**
 * File: animContext.h
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

#ifndef __Anki_Cozmo_AnimContext_H__
#define __Anki_Cozmo_AnimContext_H__

#include "util/helpers/noncopyable.h"
#include <memory>


// ---------- BEGIN FORWARD DECLARATIONS ----------
namespace Anki {
namespace Util {
  class Locale;
  class RandomGenerator;
  namespace Data {
    class DataPlatform;
  }
}


namespace AudioEngine {
namespace Multiplexer {
class AudioMultiplexer;
}
}

namespace Vector {

class Alexa;
namespace Anim {
  class BackpackLightComponent;
}
namespace MicData {
  class MicDataSystem;
}
namespace Anim {
  class RobotDataLoader;
}
class ShowAudioStreamStateManager;
class ThreadIDInternal;
class PerfMetricAnim;

namespace Audio {
class AudioPlaybackSystem;
class CozmoAudioController;
}
namespace RobotInterface {
class MessageHandler;
}
namespace WebService {
class WebService;
}

} // namespace Vector
} // namespace Anki

// ---------- END FORWARD DECLARATIONS ----------



// Here begins the actual namespace and interface for AnimContext
namespace Anki {
namespace Vector {
namespace Anim {

class AnimContext : private Util::noncopyable
{
  using AudioMultiplexer = AudioEngine::Multiplexer::AudioMultiplexer;

public:
  AnimContext(Util::Data::DataPlatform* dataPlatform);
  AnimContext();
  virtual ~AnimContext();

  Util::Data::DataPlatform*             GetDataPlatform() const { return _dataPlatform; }
  Util::Locale*                         GetLocale() const { return _locale.get(); }
  Util::RandomGenerator*                GetRandom() const { return _random.get(); }
  Anim::RobotDataLoader*                GetDataLoader() const { return _dataLoader.get(); }
  Audio::CozmoAudioController*          GetAudioController() const; // Can return nullptr
  AudioMultiplexer*                     GetAudioMultiplexer() const { return _audioMux.get(); }
  MicData::MicDataSystem*               GetMicDataSystem() const { return _micDataSystem.get(); }
  ShowAudioStreamStateManager*          GetShowAudioStreamStateManager() const { return _showStreamStateManager.get(); }
  WebService::WebService*               GetWebService() const { return _webService.get(); }
  Audio::AudioPlaybackSystem*           GetAudioPlaybackSystem() const { return _audioPlayer.get(); }
  Alexa*                                GetAlexa() const { return _alexa.get(); }
  BackpackLightComponent*               GetBackpackLightComponent() const { return _backpackLightComponent.get(); }
  PerfMetricAnim*                       GetPerfMetric() const { return _perfMetric.get(); }

  void SetRandomSeed(uint32_t seed);

  void SetLocale(const std::string & locale);

private:
  // This is passed in and held onto, but not owned by the context (yet.
  // It really should be, and that refactoring will have to happen soon).
  Util::Data::DataPlatform*                      _dataPlatform = nullptr;

  // Context holds onto these things for everybody.
  //
  // Note that MicDataSystem calls into Alexa component, so MicDataSystem
  // must be shut down BEFORE Alexa component is destroyed!
  //
  std::unique_ptr<Util::Locale>                  _locale;
  std::unique_ptr<AudioMultiplexer>              _audioMux;
  std::unique_ptr<Util::RandomGenerator>         _random;
  std::unique_ptr<Anim::RobotDataLoader>         _dataLoader;
  std::unique_ptr<Alexa>                         _alexa;
  std::unique_ptr<MicData::MicDataSystem>        _micDataSystem;
  std::unique_ptr<ShowAudioStreamStateManager>   _showStreamStateManager;
  std::unique_ptr<WebService::WebService>        _webService;
  std::unique_ptr<Audio::AudioPlaybackSystem>    _audioPlayer;
  std::unique_ptr<BackpackLightComponent>        _backpackLightComponent;
  std::unique_ptr<PerfMetricAnim>                _perfMetric;


  void InitAudio(Util::Data::DataPlatform* dataPlatform);
};


} // namespace Anim
} // namespace Vector
} // namespace Anki

#endif // __Anki_Cozmo_AnimContext_H__
