/**
* File: messageHelpers.cpp
*
* Author: Lee Crippen
* Created: 2017-02-16
*
* Description: Helpers for message types
*
* Copyright: Anki, Inc. 2017
*
**/

#include "engine/messageHelpers.h"
#include "util/logging/logging.h"

#include <map>
#include <string>
#include <type_traits>

namespace Anki {
namespace Vector {

using namespace ExternalInterface;

// Some longer types aliased to make things a little saner
using EToGLookupMap = std::map<std::string, MessageEngineToGameTag>;
using EToGUnderlyingType = std::underlying_type<MessageEngineToGameTag>::type;
  
// Local static function used to init the stringt -> type map
static EToGLookupMap InitLookupMap()
{
  EToGLookupMap lookupMap;
  EToGUnderlyingType counter;
  for (counter = 0; counter < std::numeric_limits<EToGUnderlyingType>::max(); ++counter)
  {
    MessageEngineToGameTag nextTag = MessageEngineToGameTag(counter);
    std::string foundName = MessageEngineToGameTagToString(nextTag);
    
    // HACK to find when we reach the end of the list :(
    // This needs to be fixed by updating CLAD to generate code for going from string to
    // tagtype rather than relying on
    if (0 == foundName.compare("INVALID"))
    {
      break;
    }
    
    std::transform(foundName.begin(), foundName.end(), foundName.begin(), ::tolower);
    lookupMap[foundName] = nextTag;
  }
  
  return lookupMap;
}
  
// One global instance, created at static initialization on app launch
static EToGLookupMap _messageEtoGLookupMap = InitLookupMap();

// Function used to retrieve enum type by passed in string at runtime
MessageEngineToGameTag GetEToGMessageTypeFromString(const char* inString)
{
  // For case-insensitive lookup all strings are stored in lower case
  std::string lowerCaseString = inString;
  std::transform(lowerCaseString.begin(), lowerCaseString.end(), lowerCaseString.begin(), ::tolower);
  
  const auto& it = _messageEtoGLookupMap.find(lowerCaseString);
  if (it != _messageEtoGLookupMap.end())
  {
    return it->second;
  }
  
  PRINT_NAMED_ERROR("MessageHelpers.GetEToGMessageTypeFromString.NotFound", "No match found for '%s'", lowerCaseString.c_str());
  
  // We want to return SOMETHING, so return the max value possible from the underlying type of this enum
  return MessageEngineToGameTag(std::numeric_limits<EToGUnderlyingType>::max());
}
  
} // namespace Vector
} // namespace Anki
