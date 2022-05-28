/**
 * File: victorCrashReports/tombstoneHooks.h
 *
 * Description: Declaration of tombstone crash hooks
 *
 * Copyright: Anki, Inc. 2018
 *
 **/

#ifndef __victorCrashReports_tombstoneHooks_h
#define __victorCrashReports_tombstoneHooks_h

namespace Anki {
namespace Vector {

// Enable calls to debugger dump_tombstone
void InstallTombstoneHooks();

// Disable calls to debugger dump_tombstone
void UninstallTombstoneHooks();

} // end namespace Vector
} // end namespace Anki

#endif // __victorCrashReports_tombstoneHooks_h
