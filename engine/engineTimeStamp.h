/**
 * File: engineTimeStamp.h
 *
 * Author: ross
 * Date:   Jun 9 2018
 *
 * Description: Type definition for engine timestamps (a strongly typed TimeStamp_t)
 *
 * Copyright: Anki, Inc. 2018
 **/

#ifndef __Engine_EngineTimeStamp_H__
#define __Engine_EngineTimeStamp_H__
#pragma once

#include "coretech/common/shared/types.h"
#include "util/helpers/stronglyTyped.h"

namespace Anki {
namespace Vector {

typedef Util::StronglyTyped<TimeStamp_t, struct EngineTimeStampID> EngineTimeStamp_t;

} // namespace
} // namespace

#endif // __Engine_EngineTimeStamp_H__
