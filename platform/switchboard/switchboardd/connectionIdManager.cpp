/**
 * File: connectionIdManager.cpp
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
#include "connectionIdManager.h"

namespace Anki {
namespace Switchboard {

void ConnectionIdManager::Clear() {
  _connectionId = "";
}

void ConnectionIdManager::SetConnectionId(std::string id) {
  _connectionId = id;
}

std::string ConnectionIdManager::GetConnectionId() {
  return _connectionId;
}

bool ConnectionIdManager::IsValidId(std::string id) {
  return (id == _connectionId) || (_connectionId == "");
}

} // Switchboard
} // Anki