/**
 * File: ctrlCommonInitialization.h
 *
 * Author: raul
 * Date:  07/08/16
 *
 * Description: A few functions that all webots controllers share to initialize. Originally created to refactor
 * the way we set filters for logging.
 *
 * Copyright: Anki, Inc. 2016
**/

#ifndef __Cozmo_WebotsCtrlShared_CtrlCommonInitialization_H__
#define __Cozmo_WebotsCtrlShared_CtrlCommonInitialization_H__

#include "coretech/common/engine/utils/data/dataPlatform.h"
#include "util/helpers/noncopyable.h"

// - - fwd declaration
namespace Anki {
namespace Util {
  class IFormattedLoggerProvider;
}
}

namespace Anki {
namespace WebotsCtrlShared {

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// Command line
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// structure to store the result of command line parameters
struct ParsedCommandLine {
  bool filterLog = false;
  bool colorizeStderrOutput = false;
};

// parses the parameters from a command line into ParseCommandLine struct
ParsedCommandLine ParseCommandLine(int argc, char** argv);

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// Data Platform
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

// creates a data platform with paths pointing at BaseStation defaults
  Anki::Util::Data::DataPlatform CreateDataPlatformBS(const std::string& runningPath, const std::string& platformID);
// creates a data platform with paths pointing at test defaults
Anki::Util::Data::DataPlatform CreateDataPlatformTest(const std::string& runningPath, const std::string& platformID);

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// Logging
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

// this class deletes the memory of the logger provider and sets the global pointer to null if it was pointing
// at the memory we are releasing. This is because we have destruction with no order guarantee, so potentially
// the logger could get out of scope and trying to print could cause a crash. A more complex version
// could use templates, but IFormattedLoggerProvider interface should do.
class AutoGlobalLogger : private Util::noncopyable {
public:
  // set provider as the global one, and load filter information using the platform if necessary
  AutoGlobalLogger(Util::IFormattedLoggerProvider* provider, const Util::Data::DataPlatform& dataPlatform, bool loadLoggerFilter);
  // destroy and unset provider as logger one
  virtual ~AutoGlobalLogger();
  
protected:
  // set provider as the global one, and load filter information using the platform if necessary (refactoring init)
  void Initialize(Util::IFormattedLoggerProvider* loggerProvider, const Util::Data::DataPlatform& dataPlatform, bool loadLoggerFilter);
  
  // attributes
  Util::IFormattedLoggerProvider* _provider;
};

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// AutoGlobalLogger with a default logger provider (printf)
class DefaultAutoGlobalLogger : public AutoGlobalLogger {
public:
  // constructs a provider and sets it as global, automatically cleaning it up
  DefaultAutoGlobalLogger(const Util::Data::DataPlatform& dataPlatform, bool loadLoggerFilter, bool colorizeStderrOutput);
};

}; // namespace
}; // namespace

#endif //
