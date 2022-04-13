/**
 * File: animationTransfer.h
 *
 * Author: Molly Jameson
 * Created: 09/22/16
 *
 * Description: Container for chunked uploads for the SDK uploading animation files at runtime
 *
 * Copyright: Anki, Inc. 2016
 *
 **/


#ifndef __Cozmo_Basestation_Animations_AnimationTransfer_H__
#define __Cozmo_Basestation_Animations_AnimationTransfer_H__

#include "util/helpers/noncopyable.h"
#include "util/signals/simpleSignal_fwd.h"
#include <string>


namespace Anki {
  
  namespace Util {
    namespace Data {
      class DataPlatform;
    }
  }
  
namespace Vector {
  
  template <typename Type>
  class AnkiEvent;
  class IExternalInterface;
  
  namespace ExternalInterface {
    class MessageGameToEngine;
  }
  
  class AnimationTransfer  : Util::noncopyable
  {
  public:
    AnimationTransfer(Anki::Vector::IExternalInterface* externalInterface, Anki::Util::Data::DataPlatform* dataPlatform);
    virtual ~AnimationTransfer();
    
    static const std::string kCacheAnimFileName;
    static const std::string kCacheFaceAnimsDir;
    
  private:
    void HandleGameEvents(const AnkiEvent<ExternalInterface::MessageGameToEngine>& event);
    void CleanUp(bool removeFaceImgDir = true);
    
    Signal::SmartHandle _signalHandle;
    IExternalInterface* _externalInterface = nullptr;
    Anki::Util::Data::DataPlatform* _dataPlatform = nullptr;
    std::string _lastFaceAnimDir;
    int _expectedNextChunk = 0;
    
  };


} // namespace Vector
} // namespace Anki


#endif // __Cozmo_Basestation_Animations_AnimationTransfer_H__

