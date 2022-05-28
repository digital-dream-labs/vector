/**
 * File: safeHandle.h
 *
 * Author: paluri
 * Created: 8/21/2018 | HBD MARK
 *
 * Description: SafeHandle to pass along to async callbacks
 *
 * Copyright: Anki, Inc. 2018
 *
 **/
#pragma once 

namespace Anki {
namespace Switchboard {

class SafeHandle {
public:
  static std::shared_ptr<SafeHandle> Create() {
    return std::make_shared<SafeHandle>();
  }
};

}   // Switchboard
}   // Anki