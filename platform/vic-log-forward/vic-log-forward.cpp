/**
* File: vic-log-forward.cpp
*
* Description: Victor Log Forward application main
*
* Copyright: Anki, inc. 2019
*
*/

#include "util/logging/DAS.h"
#include "util/logging/logging.h"
#include "util/logging/victorLogger.h"
#include "util/string/stringUtils.h"

#include <ctype.h>
#include <log/logger.h>

#define PROCNAME    "vic-log-forward"

// Validate DAS format declarations.
// If DAS log format changes, this code should be reviewed for correctness.
static_assert(Anki::Util::DAS::EVENT_MARKER == '@', "Unexpected event marker");
static_assert(Anki::Util::DAS::FIELD_MARKER == 0x1F, "Unexpected field marker");
static_assert(Anki::Util::DAS::FIELD_COUNT == 9, "Unexpected field count");

void Error(const char * str)
{
  fprintf(stderr, "%s: %s\n", PROCNAME, str);
}

void Usage()
{
  fprintf(stderr, "Usage: %s [--help] source file\n", PROCNAME);
}

//
// Convert numeric string to long long int.
// If string can't be parsed as numeric, return 0.
//
int64_t StringToInt64(const std::string & s)
{
  try {
    return std::stoll(s);
  } catch (const std::exception &) {
    return 0;
  }
}

int main(int argc, const char * argv[])
{

  std::vector<std::string> args;

  // Process command line
  for (int i = 1; i < argc; ++i) {
    const std::string & arg = argv[i];
    if (arg == "-h" || arg == "--help") {
      Usage();
      exit(0);
    }
    args.push_back(arg);
  }

  if (args.size() != 2) {
    Error("Invalid arguments");
    Usage();
    exit(1);
  }

  // Validate event source
  const std::string & source = args[0];
  if (source.empty()) {
    Error("Invalid event source");
    Usage();
    exit(1);
  }

  // Validate input file
  const std::string & path = args[1];

  FILE * fp = fopen(path.c_str(), "r");
  if (fp == nullptr) {
    Error("Unable to read input");
    Usage();
    exit(1);
  }

  // Forward each line from file to android log.
  // Whitespace is trimmed from each line.
  // String fields are escaped to protect json format characters.
  char buf[BUFSIZ];
  while (fgets(buf, sizeof(buf), fp) != nullptr) {

    std::string line(buf);
    Anki::Util::StringTrimWhitespace(line);
    if (line.empty()) {
      // Ignore empty lines
      continue;
    }

    // Split the line into fields
    const auto & fields = Anki::Util::StringSplit(line, Anki::Util::DAS::FIELD_MARKER);

    const std::string & event = Anki::Util::DAS::Escape(fields.size() > 0 ? fields[0] : "");
    const std::string & s1 = Anki::Util::DAS::Escape(fields.size() > 1 ? fields[1] : "");
    const std::string & s2 = Anki::Util::DAS::Escape(fields.size() > 2 ? fields[2] : "");
    const std::string & s3 = Anki::Util::DAS::Escape(fields.size() > 3 ? fields[3] : "");
    const std::string & s4 = Anki::Util::DAS::Escape(fields.size() > 4 ? fields[4] : "");

    const int64_t i1 = (fields.size() > 5 ? StringToInt64(fields[5]) : 0);
    const int64_t i2 = (fields.size() > 6 ? StringToInt64(fields[6]) : 0);
    const int64_t i3 = (fields.size() > 7 ? StringToInt64(fields[7]) : 0);
    const int64_t i4 = (fields.size() > 8 ? StringToInt64(fields[8]) : 0);
    const int64_t ts = (fields.size() > 9 ? StringToInt64(fields[9]) : 0);

    if (event.empty() || event[0] != Anki::Util::DAS::EVENT_MARKER) {
      // Ignore bogus event
      continue;
    }

    // Submit fields to logcat
    __android_log_print(ANDROID_LOG_INFO,
                        source.c_str(),
                        "%s\x1f%s\x1f%s\x1f%s\x1f%s\x1f%lld\x1f%lld\x1f%lld\x1f%lld\x1f%lld",
                        event.c_str(),
                        s1.c_str(),
                        s2.c_str(),
                        s3.c_str(),
                        s4.c_str(),
                        i1,
                        i2,
                        i3,
                        i4,
                        ts);

  }

  // Clean up and we're done
  fclose(fp);
  exit(0);
}
