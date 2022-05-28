/**
* File: robotLogDumper.h
*
* Description: Robot Log Dumper
*
* Copyright: Anki, inc. 2018
*
*/

#ifndef __anki_vector_robotLogDumper_h
#define __anki_vector_robotLogDumper_h

#include "coretech/common/shared/types.h"
#include <string>

namespace Anki {
namespace Vector {

class RobotLogDumper
{
public:
  Result Dump(const std::string & gzpath);

};

} // end namespace Vector
} // end namespace Anki

#endif
