/**
* File: webotsCtrlDevLog
*
* Author: Lee Crippen
* Created: 6/21/2016
*
* Description: Webots controller for loading and displaying Cozmo dev logs
*
* Copyright: Anki, inc. 2016
*
*/
#ifndef __Simulator_Controllers_WebotsCtrlDevLog_H_
#define __Simulator_Controllers_WebotsCtrlDevLog_H_

#include "engine/debug/devLogReader.h"

#include <cstdint>
#include <memory>
#include <vector>
#include <set>

namespace webots {
  class Supervisor;
  class Node;
  class Display;
}

class UdpClient;

namespace Anki {
namespace Vector {
  
class DevLogProcessor;

class WebotsDevLogController
{
public:
  WebotsDevLogController(int32_t stepTime_ms);
  virtual ~WebotsDevLogController();
  
  std::string GetDirectoryPath() const;
  void InitDevLogProcessor(const std::string& directoryPath);
  
  // Set save images state
  void EnableSaveImages(bool enable);
  
  int32_t Update();
  
private:
  int32_t _stepTime_ms;
  float _fastForwardFactor = 1.0f;
  std::unique_ptr<webots::Supervisor> _supervisor;
  std::unique_ptr<DevLogProcessor>    _devLogProcessor;
  std::unique_ptr<UdpClient>          _vizConnection;
  std::set<int>                       _lastKeysPressed;
  webots::Node*                       _selfNode = nullptr;
  webots::Display*                    _disp = nullptr;
  std::string                         _endTimeText;
  std::string                         _currTimeText;
  uint32_t                            _totalLogLength_ms = 0;
  bool                                _savingImages;
  bool                                _isPaused = false;
  
  void HandleVizData(const DevLogReader::LogData& logData);
  void HandlePrintLines(const DevLogReader::LogData& logData);
  void UpdateKeyboard();
  bool UpdatePressedKeys();
  void EnableSaveImagesIfChecked();
  void UpdateStatusText(bool jumping = false);
  void UpdateCurrTimeRender(uint32_t time_ms, uint32_t targetJumpTime_ms = 0);
  void UpdateCurrTimeText(uint32_t time_ms);
  void UpdateEndTimeText(uint32_t time_ms);
  void PrintHelp();
  void JumpToMS(uint32_t targetTime_ms, bool dropMessages = false);
  void JumpByMS(uint32_t targetTime_ms, bool dropMessages = false);
  void SetLogCallbacks();
  void ClearLogCallbacks();
  
}; // class WebotsDevLogController
} // namespace Vector
} // namespace Anki

#endif  // __Simulator_Controllers_WebotsCtrlDevLog_H_
