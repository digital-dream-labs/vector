/**
 * File: animTimeStamp.h
 *
 * Author: ross
 * Date:   Jun 9 2018
 *
 * Description: Type definition for animProcess timestamps (a strongly typed TimeStamp_t)
 *
 * Copyright: Anki, Inc. 2018
 **/

#ifndef __AnimProcess_CozmoAnim_AnimTimeStamp_H_
#define __AnimProcess_CozmoAnim_AnimTimeStamp_H_
#pragma once

#include "coretech/common/shared/types.h"
#include "util/helpers/stronglyTyped.h"

namespace Anki {
namespace Vector {

typedef Util::StronglyTyped<TimeStamp_t, struct AnimTimeStampID> AnimTimeStamp_t;

} // namespace
} // namespace

#endif // __AnimProcess_CozmoAnim_AnimTimeStamp_H_
