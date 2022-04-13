/**
 * File: DasToSdkHandler
 *
 * Author: Jason Liptak
 * Created: 09/09/16
 *
 * Copyright: Anki, Inc. 2015
 *
 **/

#include "engine/debug/dasToSdkHandler.h"
#include "engine/events/ankiEvent.h"
#include "engine/externalInterface/externalInterface.h"
#include "clad/externalInterface/messageEngineToGame.h"
#include "clad/externalInterface/messageGameToEngine.h"
#include "coretech/messaging/engine/IComms.h"
#include "util/fileUtils/fileUtils.h"
#include "util/logging/logging.h"
#include "DAS/DAS.h"

#include <assert.h>
#include <fstream>


namespace Anki {
namespace Vector {

  void DasToSdkHandler::Init( IExternalInterface* externalInterface )
  {
    _externalInterface = externalInterface;
    
    auto dasToSdkEventCallback = std::bind(&DasToSdkHandler::HandleEvent, this, std::placeholders::_1);
    std::vector<ExternalInterface::MessageGameToEngineTag> tagList =
    {
      ExternalInterface::MessageGameToEngineTag::GetJsonDasLogsMessage,
    };
    
    // Subscribe to desired events
    for (auto tag : tagList)
    {
      _signalHandles.push_back(_externalInterface->Subscribe(tag, dasToSdkEventCallback));
    }
  }

  void SendJsonDasLogMessage(std::string jsonData, std::string fileName, IExternalInterface* externalInterface )
  {
    if( jsonData.size() > 0 )
    {
      ExternalInterface::JsonDasLogMessage message;
      message.jsonData = jsonData;
      message.fileName = fileName;
      externalInterface->Broadcast(ExternalInterface::MessageEngineToGame(std::move(message)));
    }
  }

  void SendAllDasSentMessage(uint8_t filesSent, IExternalInterface* externalInterface )
  {
    ExternalInterface::JsonDasLogAllSentMessage message;
    message.filesSent = filesSent;
    externalInterface->Broadcast(ExternalInterface::MessageEngineToGame(std::move(message)));
  }

  // Used for init of window.
  void DasToSdkHandler::SendJsonDasLogsToSdk()
  {
    PRINT_NAMED_INFO("DasToSdkHandler.SendJsonDasLogs", "Sending das logs from %s", DASGetLogDir());

    std::vector<std::string> logFiles = Util::FileUtils::FilesInDirectory(DASGetLogDir(), true);
    uint8_t filesSent = 0;
    for(const auto& logFile : logFiles)
    {
      PRINT_NAMED_INFO("DasToSdkHandler.SendJsonDasLogs", "Sending DAS json file: %s", logFile.c_str());
      std::string logFileData = Util::FileUtils::ReadFile(logFile);


      logFileData.pop_back(); // strip off the last byte, it's a comma.
      std::string postBody = "[" + logFileData + "]";

      //Send only a little at a time so we don't flood the pipe
      constexpr uint32_t kMaxFlushSize = 1024;
      constexpr uint32_t kMsgSendFrequency_us = 100;
      for (size_t i = 0; i < postBody.size(); i += kMaxFlushSize)
      {
        usleep(kMsgSendFrequency_us);
        std::string logFileName =  Util::FileUtils::GetFileName(logFile);
        SendJsonDasLogMessage(postBody.substr(i,kMaxFlushSize),logFileName.c_str(),_externalInterface);
      }
      filesSent++;
    }

    PRINT_NAMED_INFO("DasToSdkHandler.SendJsonDasLogs", "Done sending DAS json files");
    SendAllDasSentMessage(filesSent, _externalInterface);
  }
    
  void DasToSdkHandler::HandleEvent(const AnkiEvent<ExternalInterface::MessageGameToEngine>& event)
  {
    const auto& eventData = event.GetData();
    switch (eventData.GetTag())
    {
      case ExternalInterface::MessageGameToEngineTag::GetJsonDasLogsMessage:
      {
        SendJsonDasLogsToSdk();
      }
      break;
      default:
        PRINT_NAMED_ERROR("DasToSdkManager.HandleEvent.UnhandledMessageGameToEngineTag", "Unexpected tag %u", (uint32_t)eventData.GetTag());
        assert(0);
      break;
    }
  }
  
} // namespace Vector
} // namespace Anki

