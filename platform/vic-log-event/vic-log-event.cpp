/**
* File: vic-log-event.cpp
*
* Description: Victor Log Event application main
*
* Copyright: Anki, inc. 2018
*
*/

#include "util/logging/DAS.h"
#include "util/logging/logging.h"
#include "util/logging/victorLogger.h"

#include <ctype.h>

void error(const char * cmd, const char * str)
{
  fprintf(stderr, "%s: %s\n", cmd, str);
}

void usage(const char * cmd)
{
  fprintf(stderr, "Usage: %s source event [s1-s4 i1-i4]\n", cmd);
}

int main(int argc, const char * argv[])
{
  std::vector<std::string> args;

  // Process command line
  for (int i = 1; i < argc; ++i) {
    const std::string & arg = argv[i];
    if (arg == "-h" || arg == "--help") {
      usage(argv[0]);
      exit(0);
    }
    args.push_back(arg);
  }

  if (args.size() < 2) {
    error(argv[0], "Not enough arguments");
    usage(argv[0]);
    exit(1);
  }

  // Validate event source
  std::string source = args[0];
  if (source.empty()) {
    error(argv[0], "Invalid event source");
    usage(argv[0]);
    exit(1);
  }

  // Validate event name
  std::string event = args[1];
  if (event.empty()) {
    error(argv[0], "Invalid event name");
    usage(argv[0]);
    exit(1);
  }

  std::string s1 = (args.size() > 2 ? args[2] : "");
  std::string s2 = (args.size() > 3 ? args[3] : "");
  std::string s3 = (args.size() > 4 ? args[4] : "");
  std::string s4 = (args.size() > 5 ? args[5] : "");
  int64_t i1 = (args.size() > 6 ? std::stoll(args[6]) : 0);
  int64_t i2 = (args.size() > 7 ? std::stoll(args[7]) : 0);
  int64_t i3 = (args.size() > 8 ? std::stoll(args[8]) : 0);
  int64_t i4 = (args.size() > 9 ? std::stoll(args[9]) : 0);

  // If new fields are added, the code above should be updated
  static_assert(Anki::Util::DAS::FIELD_COUNT == 9, "Unexpected DAS field count");

  // Set up the logger
  Anki::Util::VictorLogger logger(source);
  Anki::Util::gLoggerProvider = &logger;
  Anki::Util::gEventProvider = &logger;

  // Log the event
  DASMSG(vic_log_event, event, "Scripted event");
  DASMSG_SET(s1, s1, "String parameter");
  DASMSG_SET(s2, s2, "String parameter");
  DASMSG_SET(s3, s3, "String parameter");
  DASMSG_SET(s4, s4, "String parameter");
  DASMSG_SET(i1, i1, "Integer parameter");
  DASMSG_SET(i2, i2, "Integer parameter");
  DASMSG_SET(i3, i3, "Integer parameter");
  DASMSG_SET(i4, i4, "Integer parameter");
  DASMSG_SEND();

  // Clean up and we're done
  Anki::Util::gEventProvider = nullptr;
  Anki::Util::gLoggerProvider = nullptr;

  exit(0);
}
