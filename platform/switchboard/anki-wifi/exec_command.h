/**
 * File: exec_command.h
 *
 * Author: seichert
 * Created: 1/19/2018
 *
 * Description: Execute a command and return results
 *
 * Copyright: Anki, Inc. 2018
 *
 **/


#pragma once
#include <functional>
#include <string>
#include <vector>

namespace Anki {

using ExecCommandCallback = std::function<void (int rc)>;

void ExecCommandInBackground(const std::vector<std::string>& args,
                             ExecCommandCallback callback,
                             long delayMillis = 0L);

int ExecCommand(const std::vector<std::string>& args);

void CancelBackgroundCommands();

} // namespace Anki
