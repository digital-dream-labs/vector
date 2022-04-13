/**
 * File: cozmoAudienceTags
 *
 * Author: baustin
 * Created: 7/24/17
 *
 * Description: Light wrapper for AudienceTags to initialize it with Cozmo-specific configuration
 *
 * Copyright: Anki, Inc. 2017
 *
 **/

#ifndef ANKI_COZMO_BASESTATION_COZMO_AUDIENCE_TAGS_H
#define ANKI_COZMO_BASESTATION_COZMO_AUDIENCE_TAGS_H

#include "util/audienceTags/audienceTags.h"

#include "util/helpers/noncopyable.h"

namespace Anki {
namespace Util {
class AudienceTags;
}

namespace Vector {

class CozmoContext;

class CozmoAudienceTags : public Util::AudienceTags, private Util::noncopyable
{
public:
  CozmoAudienceTags(const CozmoContext* context);
};

}
}

#endif
