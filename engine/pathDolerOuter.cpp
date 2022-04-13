/**
 * File: pathDolerOuter.cpp
 *
 * Author: Brad Neuman
 * Created: 2014-06-09
 *
 * Description: This object keeps track of the full-length path on the
 * basestation, and send it out bit by bit to the robot in chunks that
 * it can handle
 *
 * Copyright: Anki, Inc. 2014
 *
 **/

#include "util/logging/logging.h"
#include "clad/robotInterface/messageEngineToRobot.h"
#include "engine/robotInterface/messageHandler.h"
#include "util/math/numericCast.h"
#include "pathDolerOuter.h"

#define LOG_CHANNEL "Planner"

namespace Anki {
namespace Vector {

PathDolerOuter::PathDolerOuter(RobotInterface::MessageHandler* msgHandler)
  : pathSizeOnBasestation_(0)
  , lastDoledSegmentIdx_(-1)
  , msgHandler_(msgHandler)
{
}

void PathDolerOuter::SetPath(const Planning::Path& path)
{
  path_ = path;
  lastDoledSegmentIdx_ = -1;
  pathSizeOnBasestation_ = path.GetNumSegments();

  Dole(MAX_NUM_PATH_SEGMENTS_ROBOT);
}

void PathDolerOuter::ReplacePath(const Planning::Path& newPath)
{
  // TODO: (mrw) we probably want to check that the already doled segments are equal for safety
  path_ = newPath;
  pathSizeOnBasestation_ = newPath.GetNumSegments();
}

void PathDolerOuter::ClearPath()
{
  path_.Clear();
  pathSizeOnBasestation_ = 0;
  lastDoledSegmentIdx_ = -1;
}

void PathDolerOuter::Dole(size_t numToDole)
{
  DEV_ASSERT(msgHandler_ != nullptr, "PathDolerOuter.Dole.InvalidMessageHandler");

  size_t endIdx = lastDoledSegmentIdx_ + numToDole;
  if (endIdx >= pathSizeOnBasestation_) {
    endIdx = pathSizeOnBasestation_ - 1;
  }

  LOG_DEBUG("PathDolerOuter.Dole", "Should dole from %d to %zu (totalSegments = %zu)",
            lastDoledSegmentIdx_ + 1,
            endIdx,
            pathSizeOnBasestation_);

  for (size_t i = (size_t)lastDoledSegmentIdx_ + 1; i <= endIdx; ++i) {
    // Get reference to current segment
    const auto & segment = path_.GetSegmentConstRef(Util::numeric_cast<uint8_t>(i));

    LOG_DEBUG("PathDolerOuter.Dole", "Doling out basestation idx %zu : %s", i, segment.ToString().c_str());

    switch (segment.GetType()) {
    case Planning::PST_LINE:
    {
      RobotInterface::AppendPathSegmentLine m;
      const auto * l = &(segment.GetDef().line);
      m.x_start_mm = l->startPt_x;
      m.y_start_mm = l->startPt_y;
      m.x_end_mm = l->endPt_x;
      m.y_end_mm = l->endPt_y;

      m.speed.target = segment.GetTargetSpeed();
      m.speed.accel = segment.GetAccel();
      m.speed.decel = segment.GetDecel();

      if (msgHandler_->SendMessage(RobotInterface::EngineToRobot(std::move(m))) == RESULT_FAIL) {
        LOG_ERROR("PathDolerOuter.Dole", "ERROR: failed to send message!");
        return;
      }
      break;
    }
    case Planning::PST_ARC:
    {
      RobotInterface::AppendPathSegmentArc m;
      const auto * a = &(segment.GetDef().arc);
      m.x_center_mm = a->centerPt_x;
      m.y_center_mm = a->centerPt_y;
      m.radius_mm = a->radius;
      m.startRad = a->startRad;
      m.sweepRad = a->sweepRad;

      m.speed.target = segment.GetTargetSpeed();
      m.speed.accel = segment.GetAccel();
      m.speed.decel = segment.GetDecel();

      if (msgHandler_->SendMessage(RobotInterface::EngineToRobot(std::move(m))) == RESULT_FAIL) {
        LOG_ERROR("PathDolerOuter.Dole", "ERROR: failed to send message!");
        return;
      }
      break;
    }
    case Planning::PST_POINT_TURN:
    {
      RobotInterface::AppendPathSegmentPointTurn m;
      const auto * t = &(segment.GetDef().turn);
      m.x_center_mm = t->x;
      m.y_center_mm = t->y;
      m.startRad  = t->startAngle;
      m.targetRad = t->targetAngle;
      m.angleTolerance = t->angleTolerance;
      m.speed.target = segment.GetTargetSpeed();
      m.speed.accel = segment.GetAccel();
      m.speed.decel = segment.GetDecel();
      m.useShortestDir = t->useShortestDir;

      if (msgHandler_->SendMessage(RobotInterface::EngineToRobot(std::move(m))) == RESULT_FAIL) {
        LOG_ERROR("PathDolerOuter.Dole", "ERROR: failed to send message!");
        return;
      }
      break;
    }
    default:
      LOG_ERROR("PathDolerOuter.Dole", "Invalid path segment - unknown type");
      return;
    }

    lastDoledSegmentIdx_ = Util::numeric_cast<int16_t>(i);
  }

}

void PathDolerOuter::Update(const s8 currPathIdx)
{
  // If there is a free slot on the robot and there are segments left to dole, then dole
  const int numFreeSlots = MAX_NUM_PATH_SEGMENTS_ROBOT - (lastDoledSegmentIdx_ - currPathIdx) - 1;

  if ((numFreeSlots > 0) && (pathSizeOnBasestation_ > 0) && (lastDoledSegmentIdx_ < pathSizeOnBasestation_-1)) {
    Dole(numFreeSlots);
  }
}

}
}
