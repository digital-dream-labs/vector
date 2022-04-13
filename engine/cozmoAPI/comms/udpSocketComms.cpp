/**
 * File: UdpSocketComms
 *
 * Author: Mark Wesley
 * Created: 05/14/16
 *
 * Description: UDP implementation for socket-based communications.  Used by webots for Vector.
 *
 * Copyright: Anki, Inc. 2016
 *
 **/


#include "engine/cozmoAPI/comms/udpSocketComms.h"
#include "coretech/common/shared/types.h"
#include "engine/multiClientComms.h"
#include "engine/utils/parsingConstants/parsingConstants.h"
#include "json/json.h"
#include "util/helpers/templateHelpers.h"
#include "util/logging/logging.h"


namespace Anki {
namespace Vector {
    
  
UdpSocketComms::UdpSocketComms(UiConnectionType connectionType)
  : _comms(new MultiClientComms())
  , _advertisementService(nullptr)
{
  std::string serviceName = std::string(EnumToString(connectionType)) + "AdvertisementService";
  _advertisementService = new Comms::AdvertisementService(serviceName.c_str());
  StartAdvertising(connectionType);
}


UdpSocketComms::~UdpSocketComms()
{
  Util::SafeDelete(_comms);
  Util::SafeDelete(_advertisementService);
}

  
void UdpSocketComms::StartAdvertising(UiConnectionType connectionType)
{
  assert((connectionType == UiConnectionType::UI) || (connectionType == UiConnectionType::SdkOverUdp));
  
  const int registrationPort = UI_ADVERTISEMENT_REGISTRATION_PORT;
  const int advertisingPort  = UI_ADVERTISING_PORT;

  PRINT_CH_INFO("UiComms", "UdpSocketComms::StartAdvertising",
                   "Starting %sAdvertisementService, reg port %d, ad port %d",
                   EnumToString(connectionType), registrationPort, advertisingPort);

  _advertisementService->StartService(registrationPort, advertisingPort);
}
  
  
bool UdpSocketComms::Init(UiConnectionType connectionType, const Json::Value& config)
{
  assert((connectionType == UiConnectionType::UI) || (connectionType == UiConnectionType::SdkOverUdp));
  
  const bool isUI = (connectionType == UiConnectionType::UI);
  using namespace AnkiUtil;
  const char* hostIPKey     = kP_ADVERTISING_HOST_IP;
  const char* advertPortKey = kP_UI_ADVERTISING_PORT;
  
  const Json::Value& hostIPValue     = config[hostIPKey];
  const Json::Value& advertPortValue = config[advertPortKey];
  
  if (hostIPValue.isString() && advertPortValue.isNumeric())
  {
    Result retVal = _comms->Init(hostIPValue.asCString(), advertPortValue.asInt());
    
    if(retVal != RESULT_OK)
    {
      PRINT_NAMED_ERROR("UdpSocketComms.Init.InitComms", "Failed to initialize %s Comms.", EnumToString(connectionType));
      return false;
    }
  }
  else
  {
    if (isUI)
    {
      PRINT_NAMED_ERROR("UdpSocketComms.Init.MissingSettings", "Missing advertising host IP / UI advertising port in Json config file.");
      return false;
    }
  }
  
  const char* numDevicesKey = kP_NUM_UI_DEVICES_TO_WAIT_FOR;
  
  if(!config.isMember(numDevicesKey))
  {
    PRINT_NAMED_WARNING("UdpSocketComms.Init.NoNumDevices",
                        "No %s defined in Json config, defaulting to %d", numDevicesKey, GetNumDesiredDevices());
  }
  else
  {
    SetNumDesiredNumDevices( config[numDevicesKey].asInt() );
  }
  
  return true;
}


void UdpSocketComms::UpdateInternal()
{
  if(_comms->IsInitialized())
  {
    _comms->Update(); // pulls all the packets off the wire and stores them
  }
  
  // Always update advertisement service to support re-connections
  // if this becomes an issue we could check if there are no recently active connections
  
  _advertisementService->Update();
}

  
bool UdpSocketComms::SendMessageInternal(const Comms::MsgPacket& msgPacket)
{
  if (_comms->GetNumConnectedDevices() > 0)
  {
    const ssize_t bytesSent = _comms->Send(msgPacket);
    return (bytesSent >= msgPacket.dataLen);
  }
  else
  {
    return false;
  }
}
  

bool UdpSocketComms::RecvMessageInternal(std::vector<uint8_t>& outBuffer)
{
  return _comms->GetNextMsgPacket(outBuffer);
}


bool UdpSocketComms::ConnectToDeviceByID(DeviceId deviceId)
{

  const bool success = _comms->ConnectToDeviceByID(deviceId);
  return success;
}
  
  
bool UdpSocketComms::DisconnectDeviceByID(DeviceId deviceId)
{
  const bool success = _comms->DisconnectDeviceByID(deviceId);
  return success;
}

bool UdpSocketComms::DisconnectAllDevices()
{
  _comms->DisconnectAllDevices();
  return true;
}

void UdpSocketComms::GetAdvertisingDeviceIDs(std::vector<ISocketComms::DeviceId>& outDeviceIds)
{
  _comms->GetAdvertisingDeviceIDs(outDeviceIds);
}

  
uint32_t UdpSocketComms::GetNumConnectedDevices() const
{
  const uint32_t numConnectedDevices = _comms->GetNumConnectedDevices();
  return numConnectedDevices;
}


} // namespace Vector
} // namespace Anki

