/**
 * File: drivePathAction.cpp
 *
 * Author: Kevin M. Karol
 * Date:   2016-06-16
 *
 * Description: Allows Cozmo to drive an arbitrary specified path
 *
 *
 * Copyright: Anki, Inc. 2016
 **/

#include "engine/actions/drivePathAction.h"
#include "engine/ankiEventUtil.h"
#include "engine/components/pathComponent.h"
#include "engine/robot.h"
#include "coretech/planning/shared/path.h"

namespace Anki {
namespace Vector {
  

DrivePathAction::DrivePathAction(const Planning::Path& path)
: IAction("DrivePathAction"
          , RobotActionType::DRIVE_PATH
          , (u8)AnimTrackFlag::BODY_TRACK)
,_path(path)
{
}

ActionResult DrivePathAction::Init()
{
  ActionResult result = ActionResult::SUCCESS;
  
  // Tell robot to execute this simple path
  if(RESULT_OK != GetRobot().GetPathComponent().ExecuteCustomPath(_path)) {
    result = ActionResult::SEND_MESSAGE_TO_ROBOT_FAILED;
    return result;
  }

  return result;
}

ActionResult DrivePathAction::CheckIfDone()
{
  if( GetRobot().GetPathComponent().LastPathFailed() ) {
    return ActionResult::FAILED_TRAVERSING_PATH;
  }

  if( GetRobot().GetPathComponent().IsActive() ) {
    return ActionResult::RUNNING;
  }

  // otherwise we must have completed
  return ActionResult::SUCCESS;
}

  
} //namespace Vector
} //namespace Anki

