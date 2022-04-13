/**
 * File: visionProcessingResult.cpp
 *
 * Author: Andrew Stein
 * Date:   12/10/2018
 *
 * Description: See header.
 *
 * Copyright: Anki, Inc. 2018
 **/

#include "engine/vision/visionProcessingResult.h"

#include "clad/types/salientPointTypes.h"

namespace Anki {
namespace Vector {
    
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
static bool SalientPointDetectionPresent(const std::list<Vision::SalientPoint>& salientPoints,
                                         const Vision::SalientPointType salientType,
                                         const TimeStamp_t atTimestamp)
{
  for(const auto& salientPoint : salientPoints)
  {
    if( (atTimestamp == salientPoint.timestamp) && (salientType == salientPoint.salientType) )
    {
      return true;
    }
  }
  return false;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
bool VisionProcessingResult::ContainsDetectionsForMode(const VisionMode mode, const TimeStamp_t atTimestamp) const
{
  switch(mode)
  {
    case VisionMode::Markers:
    {
      for(const auto& marker: observedMarkers)
      {
        if(marker.GetTimeStamp() == atTimestamp)
        {
          return true;
        }
      }
      return false;
    }
      
    case VisionMode::Faces:
    {
      for(const auto& face : faces)
      {
        if(face.GetTimeStamp() == atTimestamp)
        {
          return true;
        }
      }
      return false;
    }
      
    case VisionMode::Hands:
      return SalientPointDetectionPresent(salientPoints, Vision::SalientPointType::Hand, atTimestamp);
      
    case VisionMode::People:
      return SalientPointDetectionPresent(salientPoints, Vision::SalientPointType::Person, atTimestamp);
      
    default:
      LOG_ERROR("VisionProcessingResult.ContainsDetectionsForMode.ModeNotSupported",
                "VisionMode:%s", EnumToString(mode));
      return false;
  }
}
  
} // namespace Vector
} // namespace Anki
