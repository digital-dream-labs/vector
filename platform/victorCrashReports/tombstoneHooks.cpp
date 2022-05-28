/**
 * File: victorCrashReports/tombstoneHooks.cpp
 *
 * Description: Implementation of tombstone crash hooks
 *
 * Copyright: Anki, Inc. 2018
 *
 **/

#include "tombstoneHooks.h"
#include "debugger.h"

#include <list>
#include <unordered_map>

#include <signal.h>
#include <stdio.h>
#include <sys/syscall.h>
#include <unistd.h>

namespace {

  // Which signals do we hook for intercept?
  const std::list<int> gHookSignals = { SIGILL, SIGABRT, SIGBUS, SIGFPE, SIGSEGV, SIGQUIT };

  // Keep a stash of original signal actions so they can be restored
  std::unordered_map<int, struct sigaction> gHookStash;
}

// Return OS thread ID.  Note this is not the same as POSIX thread ID or pthread_self()!
// http://man7.org/linux/man-pages/man2/gettid.2.html
static pid_t gettid()
{
  return (pid_t) syscall(SYS_gettid);
}

// Deliver a signal to a specific thread
// http://man7.org/linux/man-pages/man2/tgkill.2.html
static int tgkill(pid_t tgid, pid_t tid, int signum)
{
  return syscall(SYS_tgkill, tgid, tid, signum);
}

//
// Ask debuggerd to create tombstone for this process,
// then set up a call to default handler.
//
static void DebuggerHook(int signum, siginfo_t * info, void * ctx)
{
  const auto pid = getpid();
  const auto tid = gettid();

  // Call MODIFIED VERSION of libcutils dump_tombstone_timeout()
  // to create a tombstone for this process.  Modified version
  // will return without waiting for dump to complete.
  victor_dump_tombstone_timeout(tid, nullptr, 0, -1);

  /* Restore original signal handler, but force SA_RESTART so signal will be rethrown */
  struct sigaction action = gHookStash[signum];
  action.sa_flags |= SA_RESTART;
  sigaction(signum, &action, nullptr);

  //
  // <anki>
  // SA_RESTART doesn't seem to work reliably for all signals on vicos.
  // Workaround is to signal ourselves again, even if it screws up
  // the return address for gdb or whatever.  This is a change from
  // the handler used in bionic:
  // https://github.com/01org/android-bluez-bionic/blob/master/linker/debugger.cpp
  // </anki>
  //
  (void) tgkill(pid, tid, signum);

}

//
// Install signal handler for a given signal
//
static void InstallTombstoneHook(int signum)
{
  struct sigaction newAction;
  struct sigaction oldAction;
  memset(&newAction, 0, sizeof(newAction));
  memset(&oldAction, 0, sizeof(oldAction));

  newAction.sa_flags = (SA_SIGINFO | SA_RESTART | SA_ONSTACK);
  newAction.sa_sigaction = DebuggerHook;

  if (sigaction(signum, &newAction, &oldAction) == 0) {
    gHookStash[signum] = std::move(oldAction);
  }

}

//
// Restore original handler for a given signal
//
static void UninstallTombstoneHook(int signum)
{
  const auto pos = gHookStash.find(signum);
  if (pos != gHookStash.end()) {
    sigaction(signum, &pos->second, nullptr);
  }
}

namespace Anki {
namespace Vector {

// Install handler for each signal we want to intercept
void InstallTombstoneHooks()
{
  for (auto signum : gHookSignals) {
    InstallTombstoneHook(signum);
  }
}

// Restore handlers to original state
void UninstallTombstoneHooks()
{
  for (auto signum : gHookSignals) {
    UninstallTombstoneHook(signum);
  }
}

} // end namespace Vector
} // end namespace Anki
