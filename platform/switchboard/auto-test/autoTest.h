/**
 * File: autoTest.h
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

#pragma once

#include <string>

namespace Anki {
namespace Switchboard {

class AutoTest {

public:
static const std::string kTestPinFilePath;

static bool DoesTestPinFileExist();
static bool IsDisclaimerBot();
static bool IsAutoTestBot();

private:
static const std::string kProcCmdlinePath;
static const std::string kAnkiDevString;

};

} // Switchboard
} // Anki