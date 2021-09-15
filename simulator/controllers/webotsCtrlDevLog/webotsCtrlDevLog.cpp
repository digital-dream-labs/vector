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

#include "webotsCtrlDevLog.h"
#include "engine/debug/devLogProcessor.h"
#include "coretech/messaging/shared/UdpClient.h"
#include "clad/types/imageTypes.h"
#include "clad/types/vizTypes.h"
#include "clad/vizInterface/messageViz.h"
#include "simulator/controllers/shared/webotsHelpers.h"
#include "util/fileUtils/fileUtils.h"
#include "util/helpers/templateHelpers.h"
#include "util/logging/logging.h"
#include "util/logging/printfLoggerProvider.h"
#include "util/math/numericCast.h"
#include <webots/Display.hpp>
#include <webots/Supervisor.hpp>
#include <webots/Keyboard.hpp>

#include <functional>
#include <cmath>

namespace Anki {
namespace Vector {

static constexpr auto kDevLogStepTime_ms = 10;
static const char* kLogsDirectoryFieldName = "logsDirectory";
static const char* kSaveImagesFieldName = "saveImages";

static const int kFontWidth = 8;
static const int kFontHeight = 8;
static const int kMaxStatusStrLen = 13; // PAUSE 128.00x
static const int kMaxCurrTimeStrLen = 20; // 1234567ms 00:00.000s
static const int kMaxEndTimeLen = 5; // 00:00

WebotsDevLogController::WebotsDevLogController(int32_t stepTime_ms)
: _stepTime_ms(stepTime_ms)
, _supervisor(new webots::Supervisor())
, _devLogProcessor()
, _vizConnection(new UdpClient())
, _selfNode(_supervisor->getSelf())
, _savingImages(false)
{
  DEV_ASSERT(nullptr != _selfNode, "WebotsDevLogController.Constructor.SelfNodeMissing");
  _supervisor->getKeyboard()->enable(stepTime_ms);
  _vizConnection->Connect("127.0.0.1", Util::EnumToUnderlying(VizConstants::VIZ_SERVER_PORT));

  _disp = _supervisor->getDisplay("playback_display");
  if( _disp == nullptr ) {
    printf("ERROR: no display field found in proto\n");
  }

  UpdateStatusText();
}

WebotsDevLogController::~WebotsDevLogController()
{
  if (_vizConnection)
  {
    _vizConnection->Disconnect();
  }
}

void WebotsDevLogController::UpdateStatusText(bool jumping)
{
  char text[kMaxStatusStrLen+1];
  text[0] = 0;

  if( GetDirectoryPath().empty() ) {
    snprintf(text, kMaxStatusStrLen+1, "WAIT");
  }
  else if(jumping) {
    snprintf(text, kMaxStatusStrLen+1, "JUMPING...");
  }
  else {
    snprintf(text, kMaxStatusStrLen+1, "%s %6.2fx",
             _isPaused ? "PAUSE" : "PLAY ",
             _fastForwardFactor);
  }

  // status goes in white in the bottom left
  const int width = kMaxStatusStrLen * kFontWidth;
  const int top = _disp->getHeight() - kFontHeight;

  // clear area
  _disp->setColor(0);
  _disp->fillRectangle(0, top, width, kFontHeight);

  _disp->setColor(0xFFFFFF);
  _disp->drawText(text, 0, top);
}

void WebotsDevLogController::UpdateEndTimeText(uint32_t time_ms)
{
  char str[kMaxEndTimeLen+1];
  int mins = time_ms / (1000 * 60);
  int secs = time_ms % (1000 * 60);
  snprintf( str, kMaxEndTimeLen+1, "%2d:%02d", mins, secs );

  const int width = kMaxEndTimeLen * kFontWidth;
  const int top = _disp->getHeight() - kFontHeight;
  const int left = _disp->getWidth() - width;

  // clear area
  _disp->setColor(0);
  _disp->fillRectangle(left, top, width, kFontHeight);

  _disp->setColor(0xFFFFFF);
  _disp->drawText(str, left, top);
}

void WebotsDevLogController::UpdateCurrTimeText(uint32_t time_ms)
{
  char str[kMaxCurrTimeStrLen+1];
  int mins = time_ms / (1000 * 60);
  float secs = ( time_ms % (1000 * 60) ) * 0.001f;
  snprintf( str, kMaxCurrTimeStrLen+1, "%7ums %2d:%06.3fs", time_ms, mins, secs);

  const int width = kMaxCurrTimeStrLen * kFontWidth;
  const int top = _disp->getHeight() - kFontHeight;
  const int left = _disp->getWidth()/2 - (kMaxCurrTimeStrLen/2) * kFontWidth;

  // clear area
  _disp->setColor(0);
  _disp->fillRectangle(left, top, width, kFontHeight);

  _disp->setColor(0x00CCCC);
  _disp->drawText(str, left, top);
}

void WebotsDevLogController::UpdateCurrTimeRender(uint32_t time_ms, uint32_t targetJumpTime_ms)
{
  // first update text
  UpdateCurrTimeText(time_ms);

  if( _totalLogLength_ms > 0 ) {
    // draw the progress bar
    static const int kPadding = 2;

    // draw outline
    const int totalTop = kPadding;
    const int totalLeft = kPadding;
    const int totalHeight = _disp->getHeight() - kFontHeight - 2 * kPadding;
    const int totalWidth = _disp->getWidth() - 2 * kPadding;

    _disp->setColor(0x00CCCC);
    _disp->drawRectangle(totalLeft, totalTop, totalWidth, totalHeight);

    // draw progress
    static const int kInnerPadding = 2;
    const int top = totalTop + kInnerPadding;
    const int left = totalLeft + kInnerPadding;
    const int height = totalHeight - 2 * kInnerPadding;
    const int maxInnerWidth = totalWidth - 2 * kInnerPadding;
    const int progressWidth = time_ms * maxInnerWidth / _totalLogLength_ms;
    const int width = std::min( progressWidth, maxInnerWidth );

    if (width > 0) {
      _disp->setColor(0x00CCCC);
      _disp->fillRectangle(left, top, width, height);
    }

    // if we are jumping, draw a mark where we are jumping to
    if( targetJumpTime_ms > 0 ) {
      const int markLeft = kPadding + kInnerPadding + targetJumpTime_ms * maxInnerWidth / _totalLogLength_ms - 1;
      const int markWidth = 2;

      _disp->setColor(0xffffff);
      _disp->fillRectangle(markLeft, top, markWidth, height);
    }
  }
}


int32_t WebotsDevLogController::Update()
{
  UpdateKeyboard();

  if (_devLogProcessor && ! _isPaused)
  {
    if (_devLogProcessor->AdvanceTime(std::round(_fastForwardFactor * _stepTime_ms)))
    {
      uint32_t currTime = _devLogProcessor->GetCurrPlaybackTime();
      UpdateCurrTimeRender(currTime);
    }
    else {
      // Once we no longer have log data left clear the processor
      _devLogProcessor.reset();
    }
  }

  // don't use fast forward factor here. This allows us to advance the log playback faster than the sim
  if (_supervisor->step(_stepTime_ms) == -1)
  {
    PRINT_NAMED_ERROR("WebotsDevLogController.Update.StepFailed", "");
    return -1;
  }

  return 0;
}

std::string WebotsDevLogController::GetDirectoryPath() const
{
  std::string dirPath;
  WebotsHelpers::GetFieldAsString(*_selfNode, kLogsDirectoryFieldName, dirPath);
  return dirPath;
}

void WebotsDevLogController::EnableSaveImagesIfChecked()
{
  const webots::Field* saveImagesField = _selfNode->getField(kSaveImagesFieldName);
  if (nullptr == saveImagesField)
  {
    PRINT_NAMED_ERROR("WebotsDevLogController.ToggleImageSaving.MissingSaveImagesField",
                      "Name: %s", kSaveImagesFieldName);
  }
  else
  {
    EnableSaveImages(saveImagesField->getSFBool());
  }
}

void WebotsDevLogController::EnableSaveImages(bool enable)
{
  if(enable == _savingImages)
  {
    // Nothing to do, already in correct mode
    return;
  }

  ImageSendMode mode = ImageSendMode::Off;
  if(enable)
  {
    mode = ImageSendMode::Stream;
    _savingImages = true;
  }
  else
  {
    _savingImages = false;
  }

  // Save images to "savedVizImages" in log directory
  std::string path = Util::FileUtils::FullFilePath({_devLogProcessor->GetDirectoryName(), "savedImages"});

  VizInterface::MessageViz message(VizInterface::SaveImages(mode, path));

  const size_t MAX_MESSAGE_SIZE{(size_t)VizConstants::MaxMessageSize};
  uint8_t buffer[MAX_MESSAGE_SIZE]{0};

  const size_t numPacked = message.Pack(buffer, MAX_MESSAGE_SIZE);

  if (_vizConnection->Send((const char*)buffer, numPacked) <= 0) {
    PRINT_NAMED_WARNING("VizManager.SendMessage.Fail", "Send vizMsgID %s of size %zd failed", VizInterface::MessageVizTagToString(message.GetTag()), numPacked);
  }
}

void WebotsDevLogController::PrintHelp()
{
  printf("DevLogger keyboard commands help:\n");
  printf("i   : toggle image save state\n");
  printf("l   : Init logging (path specified in field)\n");
  printf("-   : Slower playback\n");
  printf("+   : Faster playback\n");
  printf("0   : Reset playback speed\n");
  printf("j   : Jump to 'jumpToMS' milliseconds in the log\n");
  printf("J   : Shift+J to jump and skip all messages\n");
  printf("n   : Jump to next print message\n");
  printf("SPC : Play / pause\n");
}

void WebotsDevLogController::UpdateKeyboard()
{
  if (!UpdatePressedKeys())
  {
    return;
  }

  for(auto key : _lastKeysPressed)
  {
    // Extract modifier key(s)
    int modifier_key = key & ~webots::Keyboard::KEY;

    // Set key to its modifier-less self
    key &= webots::Keyboard::KEY;

    switch(key)
    {
      case(int)'I':
      {
        // Toggle save state:
        EnableSaveImages(!_savingImages);

        // Make field in object tree match new state (EnableSaveImages changes _savingImages)
        webots::Field* saveImagesField = _selfNode->getField(kSaveImagesFieldName);
        if (nullptr == saveImagesField)
        {
          PRINT_NAMED_ERROR("WebotsDevLogController.ToggleImageSaving.MissingSaveImagesField",
                            "Name: %s", kSaveImagesFieldName);
        }
        else
        {
          saveImagesField->setSFBool(_savingImages);
        }

        break;
      }

      case (int)'L':
      {
        std::string dirPath = GetDirectoryPath();
        if(!dirPath.empty())
        {
          InitDevLogProcessor(dirPath);
        }
        break;
      }

      case (int)' ':
      {
        _isPaused = ! _isPaused;
        UpdateStatusText();
        break;
      }

      case (int)'-':
      case (int)'_':
      {
        _fastForwardFactor = _fastForwardFactor / 2;
        if(_fastForwardFactor <= 0) {
          _fastForwardFactor = 1;
        }
        UpdateStatusText();
        break;
      }

      case (int)'=':
      case (int)'+':
      {
        _fastForwardFactor = _fastForwardFactor * 2;
        UpdateStatusText();
        break;
      }

      case (int)'0':
      case (int)')':
      {
        _fastForwardFactor = 1.0f;
        UpdateStatusText();
        break;
      }

      case (int)'J':
      {
        const bool dropMessages = modifier_key & webots::Keyboard::SHIFT;

        int ms = _selfNode->getField("jumpToMS")->getSFInt32();
        JumpToMS(ms, dropMessages);
        break;
      }

      case (int)'n':
      case (int)'N':
      {
        const uint32_t timeToJump = _devLogProcessor->GetNextPrintTime_ms();
        if( timeToJump == 0 ) {
          printf("No next print message (end of log?)\n");
        }
        else {
          JumpByMS(timeToJump);
        }
        break;
      }

      case (int)'/':
      {
        PrintHelp();
        break;
      }

    } // switch(key)

  }
}

void WebotsDevLogController::SetLogCallbacks()
{
  _devLogProcessor->SetVizMessageCallback( std::bind( &WebotsDevLogController::HandleVizData,
                                                      this,
                                                      std::placeholders::_1 ) );
  _devLogProcessor->SetPrintCallback( std::bind( &WebotsDevLogController::HandlePrintLines,
                                                 this,
                                                 std::placeholders::_1 ) );
}

void WebotsDevLogController::ClearLogCallbacks()
{
  _devLogProcessor->SetVizMessageCallback({});
  _devLogProcessor->SetPrintCallback({});
}


void WebotsDevLogController::JumpToMS(uint32_t targetTime_ms, bool dropMessages)
{

  if( ! _devLogProcessor ) {
    return;
  }

  uint32_t currTime_ms = _devLogProcessor->GetCurrPlaybackTime();
  if( targetTime_ms <= currTime_ms ) {
    PRINT_NAMED_ERROR("WebotsDevLogController.JumpToMS.NonPositive",
                      "Only positive jumps are supported, sorry");
    return;
  }

  uint32_t jump_ms = targetTime_ms - currTime_ms;

  PRINT_NAMED_INFO("WebotsDevLogController.JumpToMS",
                   "fast forwarding ahead to %d ms (jumping by %d)",
                   targetTime_ms, jump_ms);

  // update time now so we can show the jump carrot
  uint32_t currTime = _devLogProcessor->GetCurrPlaybackTime();
  UpdateCurrTimeRender(currTime, targetTime_ms);

  JumpByMS(jump_ms, dropMessages);

  PRINT_NAMED_INFO("WebotsDevLogController.JumpToMS.Complete",
                   "jump complete");
}

void WebotsDevLogController::JumpByMS(uint32_t jump_ms, bool dropMessages)
{
  if( ! _devLogProcessor ) {
    return;
  }

  if( dropMessages ) {
    ClearLogCallbacks();
  }

  // play all of the messages, skipping ahead by chunks of kMaxJumpInterval_ms
  static const uint32_t kMaxJumpInterval_ms = 60000;

  uint32_t currJump = 0;
  while( currJump < jump_ms ) {
    const uint32_t jumpLeft = (jump_ms - currJump);
    uint32_t thisJump = jumpLeft > kMaxJumpInterval_ms ? kMaxJumpInterval_ms : jumpLeft;
    currJump += thisJump;

    if (_devLogProcessor->AdvanceTime(thisJump) ) {
      uint32_t currTime = _devLogProcessor->GetCurrPlaybackTime();
      UpdateCurrTimeRender(currTime);
      const bool jumping = true;
      UpdateStatusText(jumping);
    }
    else {
      // Once we no longer have log data left clear the processor
      _devLogProcessor.reset();
    }

    _supervisor->step(_stepTime_ms);
  }

  const bool jumping = false;
  UpdateStatusText(jumping);

  if( dropMessages) {
    // restore callbacks
    SetLogCallbacks();
  }
}


bool WebotsDevLogController::UpdatePressedKeys()
{
  std::set<int> currentKeysPressed;
  int key = _supervisor->getKeyboard()->getKey();
  while(key >= 0)
  {
    currentKeysPressed.insert(key);
    key = _supervisor->getKeyboard()->getKey();
  }

  // If exact same keys were pressed last tic, do nothing.
  if (_lastKeysPressed == currentKeysPressed)
  {
    return false;
  }

  _lastKeysPressed = currentKeysPressed;
  return true;
}

void WebotsDevLogController::InitDevLogProcessor(const std::string& directoryPath)
{

  // We only init the dev log processor when we don't have one and we've been given a valid path.
  // It would be nice to handle loading a new log after having run one already, but the VizController is stateful
  // and we don't yet have a way to clear it before going through another log
  if (_devLogProcessor)
  {
    PRINT_NAMED_INFO("WebotsDevLogController.InitDevLogProcessor", "DevLogProcessor already exists. Ignoring.");
    return;
  }

  if (directoryPath.empty() || !Util::FileUtils::DirectoryExists(directoryPath))
  {
    PRINT_NAMED_INFO("WebotsDevLogController.InitDevLogProcessor", "Input directory %s not found.", directoryPath.c_str());
    return;
  }

  PRINT_NAMED_INFO("WebotsDevLogController.InitDevLogProcessor", "Loading directory %s", directoryPath.c_str());
  _devLogProcessor.reset(new DevLogProcessor(directoryPath));
  SetLogCallbacks();

  _totalLogLength_ms = _devLogProcessor->GetFinalTime_ms();
  PRINT_NAMED_INFO("WebotsDevLogController.InitDevLogProcessor.TotalLength",
                   "max log timestamp is %d",
                   _totalLogLength_ms);

  UpdateEndTimeText( _totalLogLength_ms );

  // Initialize saveImages to on if box is already checked
  EnableSaveImagesIfChecked();

  UpdateStatusText();
}

void WebotsDevLogController::HandleVizData(const DevLogReader::LogData& logData)
{
  if (_vizConnection && _vizConnection->IsConnected())
  {
    _vizConnection->Send(reinterpret_cast<const char*>(logData._data.data()), logData._data.size());
  }
}

void WebotsDevLogController::HandlePrintLines(const DevLogReader::LogData& logData)
{
  std::cout << reinterpret_cast<const char*>(logData._data.data());
}

} // namespace Vector
} // namespace Anki


// =======================================================================


int main(int argc, char **argv)
{
  // Note: we don't allow logFiltering in DevLog like we do in the other controllers because this
  // controller is meant to show all logs.

  Anki::Util::PrintfLoggerProvider loggerProvider;
  loggerProvider.SetMinLogLevel(Anki::Util::LOG_LEVEL_DEBUG);
  loggerProvider.SetMinToStderrLevel(Anki::Util::LOG_LEVEL_WARN);
  Anki::Util::gLoggerProvider = &loggerProvider;

  Anki::Vector::WebotsDevLogController webotsCtrlDevLog(Anki::Vector::kDevLogStepTime_ms);

  // If log directory is already specified when we start, just go ahead and use it,
  // without needing to press 'L' key
  std::string dirPath = webotsCtrlDevLog.GetDirectoryPath();
  if(!dirPath.empty())
  {
    webotsCtrlDevLog.Update(); // Tick once first
    webotsCtrlDevLog.InitDevLogProcessor(dirPath);
  }


  while (webotsCtrlDevLog.Update() == 0) { }

  return 0;
}
