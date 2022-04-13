/**
* File: dasLoggerProvider
*
* Author: damjan stulic
* Created: 4/27/15
*
* Description:
*
* Copyright: Anki, inc. 2015
*
*/

//
// This file is not used on victor. Contents are preserved for reference.
//
#if !defined(VICOS)

#ifndef __DasSystemLoggerProvider_H_
#define __DasSystemLoggerProvider_H_

#include "util/logging/iLoggerProvider.h"
#include "util/logging/iEventProvider.h"
#include <DAS/DAS.h>

namespace Anki {
namespace Util {

class DasLoggerProvider : public ILoggerProvider, public IEventProvider {
public:

  inline void PrintEvent(const char* eventName,
    const std::vector<std::pair<const char*, const char*>>& keyValues,
    const char* eventValue) override {
    _DAS_LogKv(DASLogLevel_Event, eventName, eventValue, keyValues);
  };
  inline void PrintLogE(const char* eventName,
    const std::vector<std::pair<const char*, const char*>>& keyValues,
    const char* eventValue) override {
    _DAS_LogKv(DASLogLevel_Error, eventName, eventValue, keyValues);
  }
  inline void PrintLogW(const char* eventName,
    const std::vector<std::pair<const char*, const char*>>& keyValues,
    const char* eventValue) override {
    _DAS_LogKv(DASLogLevel_Warn, eventName, eventValue, keyValues);
  };
  inline void PrintLogI(const char* channel,
    const char* eventName,
    const std::vector<std::pair<const char*, const char*>>& keyValues,
    const char* eventValue) override {
    // note: ignoring channel in this provider
    _DAS_LogKv(DASLogLevel_Info, eventName, eventValue, keyValues);
  };
  inline void PrintLogD(const char* channel,
    const char* eventName,
    const std::vector<std::pair<const char*, const char*>>& keyValues,
    const char* eventValue) override {
    _DAS_LogKv(DASLogLevel_Debug, eventName, eventValue, keyValues);
  }

  inline void Flush() override {
    DASForceFlushNow();
  }

  inline void SetGlobal(const char* key, const char* value) override {
    _DAS_SetGlobal(key, value);
  }
  inline void GetGlobals(std::map<std::string, std::string>& dasGlobals) override {
    _DAS_GetGlobalsForThisRun(dasGlobals);
  }


  inline void EnableNetwork(int reason) override {
    DASEnableNetwork((DASDisableNetworkReason)reason);
  }
  inline void DisableNetwork(int reason) override {
    DASDisableNetwork((DASDisableNetworkReason)reason);
  }
};

} // end namespace Util
} // end namespace Anki


#endif //__DasSystemLoggerProvider_H_

#endif
