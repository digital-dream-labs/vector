/**
 * File: cozmoFeatureGate
 *
 * Author: baustin
 * Created: 6/15/16
 *
 * Description: Light wrapper for FeatureGate to initialize it with Cozmo-specific configuration
 *
 * Copyright: Anki, Inc. 2016
 *
 **/

#ifndef ANKI_COZMO_BASESTATION_COZMO_FEATURE_GATE_H
#define ANKI_COZMO_BASESTATION_COZMO_FEATURE_GATE_H

#include "util/featureGate/featureGate.h"
#include "util/signals/simpleSignal_fwd.h"

namespace Json {
  class Value;
}

namespace Anki {

namespace Util {
namespace Data {
class DataPlatform;
}
}

namespace Vector {

class CozmoContext;
enum class FeatureType : uint8_t;

class CozmoFeatureGate : public Util::FeatureGate
{
  using Base = Util::FeatureGate;
public:
  CozmoFeatureGate( Util::Data::DataPlatform* platform );
  
  void Init(const CozmoContext* context, const std::string& jsonContents);

  bool IsFeatureEnabled(FeatureType feature) const;
  void SetFeatureEnabled(FeatureType feature, bool enabled);
private:
  
  void SendFeaturesToWebViz(const std::function<void(const Json::Value&)>& sendFunc) const;
  
  std::vector<::Signal::SmartHandle> _signalHandles;
};

}
}

#endif
