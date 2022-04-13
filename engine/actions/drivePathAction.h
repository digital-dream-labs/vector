/**
 * File: drivePathAction.h
 *
 * Author: Kevin M. Karol
 * Date:   2016-06-16
 *
 * Description: Allows Cozmo to drive an arbitrary specified path
 *
 *
 * Copyright: Anki, Inc. 2016
 **/


#ifndef __Anki_Cozmo_Actions_DrivePathAction_H__
#define __Anki_Cozmo_Actions_DrivePathAction_H__


#include "engine/actions/actionInterface.h"
#include "coretech/planning/shared/path.h"
#include "util/signals/simpleSignal_fwd.h"

namespace Anki {

//Forward Declaration
namespace Planning{
  class Path;
  
}

namespace Vector {
  
//Forward Declaration
class Robot;


class DrivePathAction : public IAction
{
public:
  // NOTE: this action does not support custom motion profiles from the path component. It will always
  // execute the given path at the speed specified in path.
  DrivePathAction(const Planning::Path& path);
  
protected:
  virtual ActionResult Init() override;
  virtual ActionResult CheckIfDone() override;
  
  
private:
  Planning::Path _path;
  std::vector<Signal::SmartHandle> _signalHandles;
  
  
}; // class DrivePathAction
  
  
} // namespace Vector
}// namespace Anki

#endif // __Anki_Cozmo_Actions_DrivePathAction_H__
