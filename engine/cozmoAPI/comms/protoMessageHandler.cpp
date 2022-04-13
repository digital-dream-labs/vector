/**
 * File: protoMessageHandler.cpp
 *
 * Author: Shawn Blakesley
 * Date:   6/15/2018
 *
 * Description: Handles messages between gateway and engine just as
 *              RobotMessageHandler handles messages between basestation and robot.
 *
 * Copyright: Anki, Inc. 2018
 **/

#include "util/logging/logging.h"
#include "util/global/globalDefinitions.h"

#include "engine/cozmoContext.h"
#include "engine/robot.h"
#include "engine/cozmoAPI/comms/localUdpSocketComms.h"
#include "engine/cozmoAPI/comms/protoMessageHandler.h"
#include "engine/cozmoAPI/comms/protoCladInterpreter.h"
#include "engine/components/robotExternalRequestComponent.h"

#include "coretech/common/engine/utils/timer.h"
#include "coretech/messaging/shared/socketConstants.h"
#include "coretech/messaging/engine/IComms.h"

#include "proto/external_interface/shared.pb.h"

#include "util/cpuProfiler/cpuProfiler.h"

#ifdef SIMULATOR
#include "osState/osState.h"
#endif

namespace Anki {
namespace Vector {


ProtoMessageHandler::ProtoMessageHandler()
  : _socketComms(new LocalUdpSocketComms(true, ENGINE_GATEWAY_PROTO_SERVER_PATH))
  , _messageCountOutgoing(0)
  , _messageCountIncoming(0)
{
}


ProtoMessageHandler::~ProtoMessageHandler()
{
}


// Initialize the proto message handler with the cozmo context
Result ProtoMessageHandler::Init(CozmoContext* context, const Json::Value& config)
{
  if (_socketComms)
  {
    if (!_socketComms->Init(UiConnectionType::Gateway, config)) // These params don't really do anything
    {
      return RESULT_FAIL;
    }
  }

  _socketComms->ConnectToDeviceByID(1);

  _isInitialized = true;
  _context = context;

  _externalRequestComponent = std::make_unique<RobotExternalRequestComponent>();
  _externalRequestComponent->Init(_context);

  auto * externalRequestComponent = _externalRequestComponent.get();

  auto versionStateRequestCallback = std::bind(&RobotExternalRequestComponent::GetVersionState, externalRequestComponent, std::placeholders::_1);
  auto batteryStateRequestCallback = std::bind(&RobotExternalRequestComponent::GetBatteryState, externalRequestComponent, std::placeholders::_1);

  // Subscribe to desired simple events
  _signalHandles.push_back(Subscribe(external_interface::GatewayWrapperTag::kBatteryStateRequest, batteryStateRequestCallback));
  _signalHandles.push_back(Subscribe(external_interface::GatewayWrapperTag::kVersionStateRequest, versionStateRequestCallback));

  return RESULT_OK;
}


void ProtoMessageHandler::DeliverToExternal(const external_interface::GatewayWrapper& message)
{
  ANKI_CPU_PROFILE("ProtoMH::DeliverToExternal");
  ++_messageCountOutgoing;

  Comms::MsgPacket p;
  message.SerializeToArray(p.data, Comms::MsgPacket::MAX_SIZE);

  p.dataLen = message.ByteSizeLong();
  p.destId = 1;

  if (_socketComms)
  {
    _socketComms->SendMessage(p);
  }
}

Result ProtoMessageHandler::ProcessMessageBytes(const uint8_t* const packetBytes, const size_t packetSize, bool isSingleMessage)
{
  ANKI_CPU_PROFILE("ProtoMH::ProcessMessageBytes");
  external_interface::GatewayWrapper message;

  if (!isSingleMessage)
  {
    return RESULT_FAIL;
  }

  if (packetSize > 0)
  {
    const bool success = message.ParseFromArray(packetBytes, static_cast<int>(packetSize));
    if (isSingleMessage && !success)
    {
      PRINT_STREAM_ERROR("ProtoMessageHandler.MessageBufferParseFailed",
                         "Failed to parse buffer as protobuf message.");
      return RESULT_FAIL;
    }

    // Is there a potential, in adding the redirect and not returning (on success), for these messages
    // to arrive at their destination twice?
    (void) ProtoCladInterpreter::Redirect(message, _context);

    ++_messageCountIncoming;

    Broadcast(message);
  }

  return RESULT_OK;
}

void ProtoMessageHandler::Broadcast(const external_interface::GatewayWrapper& message)
{
  ANKI_CPU_PROFILE("ProtoMH::Broadcast_GatewayWrapper");

  DEV_ASSERT(nullptr == _context || _context->IsEngineThread(),
             "UiMessageHandler.GameToEngineRef.BroadcastOffEngineThread");

  DeliverToExternal(message);
  _eventMgr.Broadcast(AnkiEvent<external_interface::GatewayWrapper>(
    BaseStationTimer::getInstance()->GetCurrentTimeInSeconds(), static_cast<u32>(message.GetTag()), message));
}


void ProtoMessageHandler::Broadcast(external_interface::GatewayWrapper&& message)
{
  ANKI_CPU_PROFILE("ProtoMH::BroadcastMove_GatewayWrapper");

  DEV_ASSERT(nullptr == _context || _context->IsEngineThread(),
             "UiMessageHandler.GameToEngineRef.BroadcastOffEngineThread");

  DeliverToExternal(message);
  _eventMgr.Broadcast(AnkiEvent<external_interface::GatewayWrapper>(
    BaseStationTimer::getInstance()->GetCurrentTimeInSeconds(), static_cast<u32>(message.GetTag()), std::move(message)));
} // Broadcast(MessageGameToEngine &&)


// Provides a way to subscribe to message types using the AnkiEventMgrs
Signal::SmartHandle ProtoMessageHandler::Subscribe(const external_interface::GatewayWrapperTag& tagType,
                                                std::function<void(const AnkiEvent<external_interface::GatewayWrapper>&)> messageHandler)
{
  return _eventMgr.Subscribe(static_cast<u32>(tagType), messageHandler);
} // Subscribe(MessageEngineToGame)


Result ProtoMessageHandler::ProcessMessages()
{
  ANKI_CPU_PROFILE("ProtoMH::ProcessMessages");

  Result retVal = RESULT_FAIL;

  if(_isInitialized)
  {
    retVal = RESULT_OK;

    if (_socketComms)
    {
      bool keepReadingMessages = true;
      const bool isSingleMessage = !_socketComms->AreMessagesGrouped();
      while(keepReadingMessages)
      {
        std::vector<uint8_t> buffer;
        keepReadingMessages = _socketComms->RecvMessage(buffer);

        if (keepReadingMessages)
        {
          Result res = ProcessMessageBytes(buffer.data(), buffer.size(), isSingleMessage);
          if (res != RESULT_OK)
          {
            retVal = RESULT_FAIL;
          }
        }
      }
    }
  }

  return retVal;
} // ProcessMessages()


Result ProtoMessageHandler::Update()
{
  ANKI_CPU_PROFILE("ProtoMH::Update");

  ++_updateCount;

  // Update all the comms
  if (_socketComms)
  {
    _socketComms->Update();
  }

  // Read messages from all the comms
  Result lastResult = ProcessMessages();
  if (RESULT_OK != lastResult)
  {
    return lastResult;
  }

  {
    ANKI_CPU_PROFILE("ProtoMH::BroadcastThreadedMessages");
    std::lock_guard<std::mutex> lock(_mutex);
    if( _threadedMsgs.size() > 0 )
    {
      for(auto& threaded_msg : _threadedMsgs) {
        Broadcast(std::move(threaded_msg));
      }
      _threadedMsgs.clear();
    }
  }

  return lastResult;
} // Update()


const Util::Stats::StatsAccumulator& ProtoMessageHandler::GetLatencyStats() const
{
  if (_socketComms)
  {
    return _socketComms->GetLatencyStats();
  }
  else
  {
    static Util::Stats::StatsAccumulator sDummyStats;
    return sDummyStats;
  }
}

} // namespace Vector
} // namespace Anki
