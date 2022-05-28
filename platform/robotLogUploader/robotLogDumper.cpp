/**
* File: robotLogDumper.cpp
*
* Description: Robot Log Dumper
*
* Copyright: Anki, inc. 2018
*
*/

#include "robotLogDumper.h"

#include "util/fileUtils/fileUtils.h"
#include "util/logging/logging.h"

#define LOG_CHANNEL "RobotLogDumper"

namespace Anki {
namespace Vector {

Result RobotLogDumper::Dump(const std::string & gzpath)
{
  // What command do we use to create the result?
  const std::string & command = "/usr/bin/sudo /anki/bin/vic-log-cat | /bin/gzip > " + gzpath;

  // Run command to completion
  LOG_INFO("RobotLogDumper.Dump", "%s", command.c_str());

  FILE * fp = popen(command.c_str(), "r");
  char buf[BUFSIZ];
  while (fgets(buf, sizeof(buf), fp) != nullptr) {
    LOG_INFO("RobotLogDumper.Dump", "%s", buf);
  }

  // Clean up command
  int status = pclose(fp);
  if (status != 0) {
    LOG_ERROR("RobotLogDumper.Dump", "Dump process exit status %d", status);
    Anki::Util::FileUtils::DeleteFile(gzpath);
    return RESULT_FAIL;
  }

  return RESULT_OK;
}

} // end namespace Vector
} // end namespace Anki
