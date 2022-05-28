/**
 * File: connectionIdManager.h
 *
 * Author: paluri
 * Created: 8/22/2018
 *
 * Description: Object to hold incoming connection id and allow
 *              other objects to query the current connection id.
 *
 * Copyright: Anki, Inc. 2018
 *
 **/
#pragma once

#include <stdlib.h>
#include <string>

namespace Anki {
namespace Switchboard {

class ConnectionIdManager {
public:
  void Clear();
  void SetConnectionId(std::string id);
  std::string GetConnectionId();
  bool IsValidId(std::string id);
private:
  std::string _connectionId;
};

} // Switchboard
} // Anki