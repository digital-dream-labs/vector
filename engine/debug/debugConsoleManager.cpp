/**
 * File: DebugConsoleManager
 *
 * Author: Molly Jameson
 * Created: 11/17/15
 *
 * Description: A lightweight wrapper around the console so that it can use CLAD at 
 * the game level instead of util
 *
 * Copyright: Anki, Inc. 2015
 *
 **/

#include "engine/debug/debugConsoleManager.h"
#include "util/console/consoleSystem.h"
#include "clad/types/debugConsoleTypes.h"
#include "engine/events/ankiEvent.h"
#include "engine/externalInterface/externalInterface.h"
#include "engine/robotInterface/messageHandler.h"
#include "engine/robot.h"
#include "clad/externalInterface/messageEngineToGame.h"
#include "clad/externalInterface/messageGameToEngine.h"
#include "clad/robotInterface/messageEngineToRobot.h"
#include "util/console/consoleChannelFile.h"
#include "coretech/messaging/engine/IComms.h"

#include <assert.h>


namespace Anki {
namespace Vector {
  
  void DebugConsoleManager::Init( IExternalInterface* externalInterface, RobotInterface::MessageHandler* robotInterface )
  {
    DEV_ASSERT(nullptr != externalInterface, "DebugConsoleManager.Init.NullExternalInterface");
    DEV_ASSERT(nullptr != robotInterface,    "DebugConsoleManager.Init.NullRobotInterface");
    
    _externalInterface = externalInterface;
    _robotInterface    = robotInterface;
    
    auto debugConsoleEventCallback = std::bind(&DebugConsoleManager::HandleEvent, this, std::placeholders::_1);
    std::vector<ExternalInterface::MessageGameToEngineTag> tagList =
    {
      ExternalInterface::MessageGameToEngineTag::GetAllDebugConsoleVarMessage,
      ExternalInterface::MessageGameToEngineTag::SetDebugConsoleVarMessage,
      ExternalInterface::MessageGameToEngineTag::SetAnimDebugConsoleVarMessage,
      ExternalInterface::MessageGameToEngineTag::RunDebugConsoleFuncMessage,
      ExternalInterface::MessageGameToEngineTag::RunAnimDebugConsoleFuncMessage,
      ExternalInterface::MessageGameToEngineTag::GetDebugConsoleVarMessage,
    };
    
    // Subscribe to desired events
    for (auto tag : tagList)
    {
      _signalHandles.push_back(_externalInterface->Subscribe(tag, debugConsoleEventCallback));
    }
  }
  
  void FlushBuffer(std::vector<ExternalInterface::DebugConsoleVar>&& dataVals, IExternalInterface* externalInterface )
  {
    if( dataVals.size() > 0 )
    {
      ExternalInterface::InitDebugConsoleVarMessage message;
      message.varData = std::move(dataVals);
      externalInterface->Broadcast(ExternalInterface::MessageEngineToGame(std::move(message)));
    }
  }
  
  static void SetCladVarUnionFromConsoleVar(Anki::Vector::ConsoleVarUnion& outVarValue, const Anki::Util::IConsoleVariable* consoleVar)
  {
    if( consoleVar->IsToggleable())
    {
      outVarValue.Set_varBool( consoleVar->GetAsUInt64() );
    }
    else if( consoleVar->IsIntegerType() )
    {
      if( consoleVar->IsSignedType())
      {
        outVarValue.Set_varInt( consoleVar->GetAsInt64() );
      }
      else
      {
        outVarValue.Set_varUint( consoleVar->GetAsUInt64() );
      }
    }
    else
    {
      outVarValue.Set_varDouble( consoleVar->GetAsDouble() );
    }
  }
  
  static void SetCladVarFromConsoleVar(ExternalInterface::DebugConsoleVar& outDebugConsoleVar, const Anki::Util::IConsoleVariable* consoleVar)
  {
    outDebugConsoleVar.varName  = consoleVar->GetID();
    outDebugConsoleVar.category = consoleVar->GetCategory();
    SetCladVarUnionFromConsoleVar(outDebugConsoleVar.varValue, consoleVar);
    outDebugConsoleVar.maxValue = consoleVar->GetMaxAsDouble();
    outDebugConsoleVar.minValue = consoleVar->GetMinAsDouble();
  }
  
  static void SetCladVarFromConsoleFunc(ExternalInterface::DebugConsoleVar& outDebugConsoleVar, const Anki::Util::IConsoleFunction* consoleFunc)
  {
    outDebugConsoleVar.varName  = consoleFunc->GetID();
    outDebugConsoleVar.category = consoleFunc->GetCategory();
    outDebugConsoleVar.varValue.Set_varFunction(consoleFunc->GetSignature());
    outDebugConsoleVar.maxValue = 0.0;
    outDebugConsoleVar.minValue = 0.0;
  }
  
  // Used for init of window.
  void DebugConsoleManager::SendAllDebugConsoleVars()
  {
    const Anki::Util::ConsoleSystem& consoleSystem = Anki::Util::ConsoleSystem::Instance();

    const Anki::Util::ConsoleSystem::VariableDatabase& varDatabase = consoleSystem.GetVariableDatabase();
    std::vector<ExternalInterface::DebugConsoleVar> dataVals;
    
    // Flush when we're about half full of the clad buffer it doesn't go over...
    // Note: There is an MTU limit too to avoid the message being split up into multiple packets - staying <1400 is good
    uint32_t messageSize = 0;
    constexpr uint32_t kMaxFlushSize = 1024;
    
    for (const auto& entry : varDatabase)
    {
      const Anki::Util::IConsoleVariable* consoleVar = entry.second;
      if (nullptr == consoleVar) {
        PRINT_NAMED_ERROR("DebugConsoleManager.SendAllDebugConsoleVars.InvalidConsoleVariable",
                          "Console variable %s is NULL", entry.first.c_str());
        continue;
      }

      ExternalInterface::DebugConsoleVar varObject;
      SetCladVarFromConsoleVar(varObject, consoleVar);
      
      dataVals.push_back(varObject);
      
      messageSize += varObject.Size();
      if (messageSize >= kMaxFlushSize)
      {
        DEV_ASSERT(messageSize < Anki::Comms::MsgPacket::MAX_SIZE, "DebugConsoleManager.VarDatabaseOverMaxSize");
        FlushBuffer(std::move(dataVals), _externalInterface);
        dataVals.clear();
        messageSize = 0;
      }
    }
    
    const Anki::Util::ConsoleSystem::FunctionDatabase& funcDatabase = consoleSystem.GetFunctionDatabase();
    for (const auto& entry : funcDatabase)
    {
      const Anki::Util::IConsoleFunction* consoleFunc = entry.second;
      if (nullptr == consoleFunc) {
        PRINT_NAMED_ERROR("DebugConsoleManager.SendAllDebugConsoleVars.InvalidConsoleFunction",
                          "Console function %s is NULL", entry.first.c_str());
        continue;
      }

      ExternalInterface::DebugConsoleVar varObject;
      SetCladVarFromConsoleFunc(varObject, consoleFunc);
      
      dataVals.push_back(varObject);
      
      messageSize += varObject.Size();
      if (messageSize >= kMaxFlushSize)
      {
        DEV_ASSERT(messageSize < Anki::Comms::MsgPacket::MAX_SIZE, "DebugConsoleManager.FuncDatabaseOverMaxSize");
        FlushBuffer(std::move(dataVals), _externalInterface);
        dataVals.clear();
        messageSize = 0;
      }
    }
    // Flush remaining...
    FlushBuffer(std::move(dataVals), _externalInterface);
    dataVals.clear();
  }
  
  
  void SendVerifyDebugConsoleVarMessage(IExternalInterface* externalInterface,
                                        const char* varName,
                                        const char* statusMessageText,
                                        const Anki::Util::IConsoleVariable* consoleVar,
                                        bool success)
  {
    ExternalInterface::VerifyDebugConsoleVarMessage message;
    message.varName = varName;
    message.statusMessage = statusMessageText;
    message.success = success;
    if (consoleVar)
    {
      SetCladVarUnionFromConsoleVar(message.varValue, consoleVar);
    }
    else
    {
      message.varValue.Set_varFunction("");
    }
    
    externalInterface->Broadcast(ExternalInterface::MessageEngineToGame(std::move(message)));
  }
  
  
  void SendVerifyDebugConsoleFuncMessage(IExternalInterface* externalInterface,
                                        const char* funcName,
                                        const char* statusMessageText,
                                        bool success)
  {
    ExternalInterface::VerifyDebugConsoleFuncMessage message;
    message.funcName = funcName;
    message.statusMessage = statusMessageText;
    message.success = success;
    externalInterface->Broadcast(ExternalInterface::MessageEngineToGame(std::move(message)));
  }
  
  template<typename T, size_t N_in>
  inline static void CopyStringHelper(const std::string& inputString, std::array<T,N_in>& outputString)
  {
    // Ensure zero-termination
    outputString.fill(0);
    
    // Make sure we don't look outside input or output's bounds, and leave outputString zero-terminated
    size_t N = std::min(outputString.size()-1, inputString.size());
    
    // Do the copy
    std::copy_n(inputString.begin(), N, outputString.begin());
  }
  
  void DebugConsoleManager::HandleEvent(const AnkiEvent<ExternalInterface::MessageGameToEngine>& event)
  {
    const auto& eventData = event.GetData();
    switch (eventData.GetTag())
    {
      case ExternalInterface::MessageGameToEngineTag::GetDebugConsoleVarMessage:
      {
        const Anki::Vector::ExternalInterface::GetDebugConsoleVarMessage& msg = eventData.Get_GetDebugConsoleVarMessage();
        Anki::Util::IConsoleVariable* consoleVar = Anki::Util::ConsoleSystem::Instance().FindVariable(msg.varName.c_str());
        if (consoleVar)
        {
          SendVerifyDebugConsoleVarMessage(_externalInterface, msg.varName.c_str(), consoleVar->ToString().c_str(), consoleVar, true);
        }
        else
        {
          PRINT_NAMED_WARNING("DebugConsoleManager.HandleEvent.NoConsoleVar", "No Console Var '%s'", msg.varName.c_str());
          SendVerifyDebugConsoleVarMessage(_externalInterface, msg.varName.c_str(), "Error: No such variable", consoleVar, false);
        }
      }
      break;
      case ExternalInterface::MessageGameToEngineTag::GetAllDebugConsoleVarMessage:
      {
        // Shoot back all the init messages
        SendAllDebugConsoleVars();
      }
      break;
      case ExternalInterface::MessageGameToEngineTag::RunDebugConsoleFuncMessage:
      {
        const Anki::Vector::ExternalInterface::RunDebugConsoleFuncMessage& msg = eventData.Get_RunDebugConsoleFuncMessage();
        Anki::Util::IConsoleFunction* consoleFunc = Anki::Util::ConsoleSystem::Instance().FindFunction(msg.funcName.c_str());
        if( consoleFunc )
        {
          enum { kBufferSize = 512 };
          char buffer[kBufferSize];
          const uint32_t res = NativeAnkiUtilConsoleCallFunction( msg.funcName.c_str(), msg.funcArgs.c_str(), kBufferSize, buffer);
          SendVerifyDebugConsoleFuncMessage(_externalInterface, msg.funcName.c_str(), buffer, (res != 0));
        }
        else
        {
          PRINT_NAMED_WARNING("DebugConsoleManager.HandleEvent.NoConsoleFunc", "No Func named '%s'",msg.funcName.c_str());
          SendVerifyDebugConsoleFuncMessage(_externalInterface, msg.funcName.c_str(), "Error: No such function", false);
        }
      }
      break;
      case ExternalInterface::MessageGameToEngineTag::SetDebugConsoleVarMessage:
      {
        const Anki::Vector::ExternalInterface::SetDebugConsoleVarMessage& msg = eventData.Get_SetDebugConsoleVarMessage();
        
        Anki::Util::IConsoleVariable* consoleVar = Anki::Util::ConsoleSystem::Instance().FindVariable(msg.varName.c_str());
        if (consoleVar && consoleVar->ParseText(msg.tryValue.c_str()) )
        {
          SendVerifyDebugConsoleVarMessage(_externalInterface, msg.varName.c_str(), consoleVar->ToString().c_str(), consoleVar, true);
        }
        else
        {
          PRINT_NAMED_WARNING("DebugConsoleManager.HandleEvent.SetDebugConsoleVarMessage", "Error setting %svar '%s' to '%s'",
                              consoleVar ? "" : "UNKNOWN ", msg.varName.c_str(), msg.tryValue.c_str());
          SendVerifyDebugConsoleVarMessage(_externalInterface, msg.varName.c_str(),
                                           consoleVar ? "Error: Failed to Parse" : "Error: No such variable",
                                           consoleVar, false);
        }
      }
      break;
      // Animation Process Console Var Message
      case ExternalInterface::MessageGameToEngineTag::SetAnimDebugConsoleVarMessage:
      {
        const Anki::Vector::ExternalInterface::SetAnimDebugConsoleVarMessage& msg = eventData.Get_SetAnimDebugConsoleVarMessage();
        
        RobotInterface::SetDebugConsoleVarMessage robotInterfaceMsg;
        
        if(msg.varName.size() > robotInterfaceMsg.varName.size())
        {
          PRINT_NAMED_WARNING("DebugConsoleManager.HandleEvent.SetDebugConsoleVarMessage.VarNameTooLong",
                              "Variable name '%s' exceeds maximum length of %zu",
                              msg.varName.c_str(), robotInterfaceMsg.varName.size());
        }
        else
        {
          CopyStringHelper(msg.varName,  robotInterfaceMsg.varName);
          CopyStringHelper(msg.tryValue, robotInterfaceMsg.tryValue);
          
          const bool reliable = true;
          const bool hot = false;
          
          Result sendResult = _robotInterface->SendMessage(RobotInterface::EngineToRobot(std::move(robotInterfaceMsg)), reliable, hot);
          if(sendResult != RESULT_OK) {
            PRINT_NAMED_WARNING("DebugConsoleManager.HandleEvent.SetDebugConsoleVarMessage.SendFailed",
                                "Failed to send message to set '%s'", msg.varName.c_str());
          }
        }
      }
      break;
      // Animation Process Console func Message
      case ExternalInterface::MessageGameToEngineTag::RunAnimDebugConsoleFuncMessage:
      {
        const auto& msg = eventData.Get_RunAnimDebugConsoleFuncMessage();
        RobotInterface::RunDebugConsoleFuncMessage robotInterfaceMsg;
        if (msg.funcName.size() > robotInterfaceMsg.funcName.size())
        {
          PRINT_NAMED_WARNING("DebugConsoleManager.HandleEvent.RunAnimDebugConsoleFuncMessage.FuncNameTooLong",
                              "Variable name '%s' exceeds maximum length of %zu",
                              msg.funcName.c_str(), robotInterfaceMsg.funcName.size());
        }
        else if (msg.funcArgs.size() > robotInterfaceMsg.funcArgs.size()) {
          PRINT_NAMED_WARNING("DebugConsoleManager.HandleEvent.RunAnimDebugConsoleFuncMessage.FuncArgsTooLong",
                              "Args '%s' exceeds maximum length of %zu",
                              msg.funcArgs.c_str(), robotInterfaceMsg.funcArgs.size());
        }
        else {
          CopyStringHelper(msg.funcName,  robotInterfaceMsg.funcName);
          CopyStringHelper(msg.funcArgs, robotInterfaceMsg.funcArgs);
          
          const bool reliable = true;
          const bool hot = false;
          
          Result sendResult = _robotInterface->SendMessage(RobotInterface::EngineToRobot(std::move(robotInterfaceMsg)),
                                                           reliable,
                                                           hot);
          if (sendResult != RESULT_OK) {
            PRINT_NAMED_WARNING("DebugConsoleManager.HandleEvent.RunAnimDebugConsoleFuncMessage.SendFailed",
                                "Failed to send message to set '%s'", msg.funcName.c_str());
          }
        }
      }
      break;
        
      default:
        PRINT_NAMED_ERROR("DebugConsoleManager.HandleEvent.UnhandledMessageGameToEngineTag", "Unexpected tag %u", (uint32_t)eventData.GetTag());
        assert(0);
      break;
    }
  }
  
} // namespace Vector
} // namespace Anki

