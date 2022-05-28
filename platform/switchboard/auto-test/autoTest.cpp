/**
 * File: autoTest.cpp
 *
 * Author: paluri
 * Created: 3/22/2019
 *
 * Description: Header for describing simple class which 
 *              provides means of determing if test pin
 *              file exists, and whether robot is a dev
 *              disclaimer bot.
 *
 * Copyright: Anki, Inc. 2019
 *
 **/

#include "util/fileUtils/fileUtils.h"
#include "auto-test/autoTest.h"

namespace Anki {
namespace Switchboard {

const std::string AutoTest::kTestPinFilePath { "/factory/ble_pairing_pin" };
const std::string AutoTest::kProcCmdlinePath { "/proc/cmdline" };
const std::string AutoTest::kAnkiDevString { "anki.dev" };

bool AutoTest::DoesTestPinFileExist()
{
  return Anki::Util::FileUtils::FileExists(kTestPinFilePath);
}

bool AutoTest::IsDisclaimerBot()
{
  return Anki::Util::FileUtils::ReadFile(kProcCmdlinePath).find(kAnkiDevString) != std::string::npos;
}

bool AutoTest::IsAutoTestBot()
{
  return AutoTest::DoesTestPinFileExist() && IsDisclaimerBot();
}

} // Switchboard
} // Anki