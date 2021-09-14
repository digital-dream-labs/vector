/**
 * File: exec_command.cpp
 *
 * Author: seichert
 * Created: 1/19/2018
 *
 * Description: Execute a command and return results
 *
 * Copyright: Anki, Inc. 2018
 *
 **/

#include "cozmoAnim/execCommand/exec_command.h"
#include "cozmoAnim/execCommand/taskExecutor.h"
#include "util/threading/fork_and_exec.h"

#include <sstream>

namespace Anki {

static TaskExecutor* sBackgroundTaskExecutor;
static bool sBackgroundCommandsCancelled = false;

int ExecCommand(const std::vector<std::string>& args)
{
  int rc = ForkAndExec(args);

  return rc;
}

void ExecCommandInBackground(const std::vector<std::string>& args,
                             ExecCommandCallback callback,
                             long delayMillis)
{
  sBackgroundCommandsCancelled = false;
  auto f = [args, callback]() {
    std::string output;
    int rc = sBackgroundCommandsCancelled ? -1 : ExecCommand(args);
    if (callback) {
      callback(rc);
    }
  };
  if (!sBackgroundTaskExecutor) {
    sBackgroundTaskExecutor = new TaskExecutor();
  }
  if (delayMillis > 0L) {
    auto when = std::chrono::steady_clock::now() + std::chrono::milliseconds(delayMillis);
    sBackgroundTaskExecutor->WakeAfter(f, when);
  } else {
    sBackgroundTaskExecutor->Wake(f);
  }
}

void CancelBackgroundCommands()
{
  sBackgroundCommandsCancelled = true;
  KillChildProcess();
  if (sBackgroundTaskExecutor) {
    delete sBackgroundTaskExecutor; sBackgroundTaskExecutor = nullptr;
  }
}

} // namespace Anki
