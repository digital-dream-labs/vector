/**
 * File: google_breakpad.h
 *
 * Author: chapados
 * Created: 10/08/2014
 *
 * Description: Google breakpad platform-specific methods
 *
 * Copyright: Anki, Inc. 2014
 *
 **/

#ifndef __GOOGLE_BREAKPAD_H__
#define __GOOGLE_BREAKPAD_H__

#include <string>

namespace GoogleBreakpad {

// Install signal handlers to generate minidump
void InstallGoogleBreakpad(const char* filenamePrefix);

// Remove signal handlers installed by above
void UnInstallGoogleBreakpad();

// Generate a minidump in crash directory.
// Returns path to minidump as OUTPUT PARAMETER.
// Returns true on success, else false.
bool WriteMinidump(const std::string & prefix, std::string & out_dump_path);

} // end namespace

#endif // #ifndef __GOOGLE_BREAKPAD_H__
