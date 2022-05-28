/**
 * File: christen.h
 *
 * Author: Paul Aluri, inspired by seichert's vic-christen 
 *         which was inspired by Paul Aluri's example code
 * Created: 4/26/2018
 *
 * Description: Christen the robot with a name if it doesn't have one
 *
 * Copyright: Anki, Inc. 2018
 *
 **/

#include <string>

namespace Anki {
namespace Switchboard {

class Christen {
public:
  static std::string GenerateName();
};

} // Anki
} // Switchboard