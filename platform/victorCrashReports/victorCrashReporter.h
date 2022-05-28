/**
 * File: victorCrashReporter.h
 *
 * Description: Declaration of crash report API
 *
 * Copyright: Anki, Inc. 2018
 *
 **/

#ifndef __victorCrashReporter_h
#define __victorCrashReporter_h

#include <string>

namespace Anki {
namespace Vector {

//
// Install signal and exception handlers.
// FilenamePrefix may not be null.
//
void InstallCrashReporter(const char * filenamePrefix);

//
// Uninstall signal and exception handlers.
//
void UninstallCrashReporter();

//
// Write a minidump into crash directory.
// Note that path to dump is RETURNED AS OUTPUT.
// Incoming value is ignored.
//
// Returns true on success, false on error.
//
bool WriteMinidump(const std::string & prefix, std::string & out_dump_path);

//
// Stub class to manage lifetime of crash report handlers
// Handlers are automatically installed when object is constructed.
// Handlers are automatically removed when object is destroyed.
//
class CrashReporter
{
public:
  CrashReporter(const char * filenamePrefix);
  ~CrashReporter();

};

} // end namespace Vector
} // end namespace Anki


#endif // __victorCrashReporter_h
