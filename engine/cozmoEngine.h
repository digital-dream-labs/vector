/*
 * File:          cozmoEngine.h
 * Date:          12/23/2014
 *
 * Description:   A platform-independent container for spinning up all the pieces
 *                required to run Cozmo on a device. We can derive classes to be host or client
 *                for now host/client has been removed until we know more about multi-robot requirements.
 *
 *                 - Robot Vision Processor (for processing images from a physical robot's camera)
 *                 - RobotComms
 *                 - GameComms and GameMsgHandler
 *                 - RobotVisionMsgHandler
 *                   - Uses Robot Comms to receive image msgs from robot.
 *                   - Passes images onto Robot Vision Processor
 *                   - Sends processed image markers to the basestation's port on
 *                     which it receives messages from the robot that sent the image.
 *                     In this way, the processed image markers appear to come directly
 *                     from the robot to the basestation.
 *                   - While we only have TCP support on robot, RobotVisionMsgHandler
 *                     will also forward non-image messages from the robot on to the basestation.
 *
 * Author: Andrew Stein / Kevin Yoon
 *
 * Modifications:
 */

#ifndef ANKI_COZMO_BASESTATION_COZMO_ENGINE_H
#define ANKI_COZMO_BASESTATION_COZMO_ENGINE_H

#include "util/logging/multiFormattedLoggerProvider.h"
#include "coretech/vision/engine/image.h"
#include "json/json.h"
#include "util/signals/simpleSignal_fwd.h"

#include "clad/types/imageTypes.h"
#include "clad/types/engineState.h"
#include "engine/debug/debugConsoleManager.h"
#include "engine/debug/dasToSdkHandler.h"
#include "util/global/globalDefinitions.h"

#include <memory>


namespace Anki {

  // Forward declaration:
  namespace Util {
  namespace AnkiLab {
    struct ActivateExperimentRequest;
    struct AssignmentDef;
    enum class AssignmentStatus : uint8_t;
  }
  namespace Data {
    class DataPlatform;
  }
  }

namespace Vector {

// Forward declarations
class Robot;
class IExternalInterface;
class IGatewayInterface;
class CozmoContext;
class UiMessageHandler;
class ProtoMessageHandler;
class AnimationTransfer;

template <typename Type>
class AnkiEvent;

namespace ExternalInterface {
  class MessageGameToEngine;
}

class CozmoEngine
{
public:

  CozmoEngine(Util::Data::DataPlatform* dataPlatform);
  virtual ~CozmoEngine();


  Result Init(const Json::Value& config);

  // Hook this up to whatever is ticking the game "heartbeat"
  Result Update(const BaseStationTime_t currTime_nanosec);

  void ListenForRobotConnections(bool listen);

  Robot* GetRobot();

  Util::AnkiLab::AssignmentStatus ActivateExperiment(const Util::AnkiLab::ActivateExperimentRequest& request,
                                                     std::string& outVariationKey);

  void RegisterEngineTickPerformance(const float tickDuration_ms,
                                     const float tickFrequency_ms,
                                     const float sleepDurationIntended_ms,
                                     const float sleepDurationActual_ms) const;

  UiMessageHandler* GetUiMsgHandler() const { return _uiMsgHandler.get(); }
  ProtoMessageHandler* GetProtoMsgHandler() const { return _protoMsgHandler.get(); }

  EngineState GetEngineState() const { return _engineState; }

  // Designate calling thread as owner of engine updates
  void SetEngineThread();

  // Handle various message types
  template<typename T>
  void HandleMessage(const T& msg);

protected:

  std::vector<::Signal::SmartHandle> _signalHandles;

  bool                                                      _isInitialized = false;
  Json::Value                                               _config;
  std::unique_ptr<UiMessageHandler>                         _uiMsgHandler;
  std::unique_ptr<ProtoMessageHandler>                      _protoMsgHandler;
  std::unique_ptr<CozmoContext>                             _context;
  Anki::Vector::DebugConsoleManager                          _debugConsoleManager;
  Anki::Vector::DasToSdkHandler                              _dasToSdkHandler;
  bool                                                      _hasRunFirstUpdate = false;
  bool                                                      _uiWasConnected = false;
  bool                                                      _updateMoveComponent = false;

  virtual Result InitInternal();

  void SetEngineState(EngineState newState);

  Result ConnectToRobotProcess();
  Result AddRobot(RobotID_t robotID);

  void UpdateLatencyInfo();
  void InitUnityLogger();

  EngineState _engineState = EngineState::Stopped;

  std::unique_ptr<AnimationTransfer>                        _animationTransferHandler;

}; // class CozmoEngine


} // namespace Vector
} // namespace Anki

#endif // ANKI_COZMO_BASESTATION_COZMO_ENGINE_H
