#include "engine/cozmoAPI/comms/gameMessageHandler.h"

#include "util/logging/logging.h"


namespace Anki {
namespace Vector {

  GameMessageHandler::GameMessageHandler()
  : comms_(NULL)
  , isInitialized_(false)
  {
    
  }
  
  Result GameMessageHandler::Init(Comms::IComms* comms)
  {
    Result retVal = RESULT_FAIL;
    
    //TODO: PRINT_NAMED_DEBUG("MessageHandler", "Initializing comms");
    comms_ = comms;
    
    if(comms_) {
      isInitialized_ = comms_->IsInitialized();
      if (isInitialized_ == false) {
        // TODO: PRINT_NAMED_ERROR("MessageHandler", "Unable to initialize comms!");
        retVal = RESULT_OK;
      }
    }
    
    return retVal;
  }
  
  bool GameMessageHandler::IsInitialized() const {
    return isInitialized_;
  }
  
  
      Result GameMessageHandler::SendMessage(const UserDeviceID_t devID, const ExternalInterface::MessageGameToEngine& msg)
      {
//#if(RUN_UI_MESSAGE_TCP_SERVER)
        
        Comms::MsgPacket p;
        msg.Pack(p.data, Comms::MsgPacket::MAX_SIZE);
        p.dataLen = msg.Size();
        p.destId = devID;
        
        return comms_->Send(p) > 0 ? RESULT_OK : RESULT_FAIL;
        
//#else
        
        //MessageQueue::getInstance()->AddMessageForUi(msg);
        
//#endif
        
        return RESULT_OK;
      }
      
      
      Result GameMessageHandler::ProcessPacket(const std::vector<uint8_t>& buffer)
      {
        Result retVal = RESULT_FAIL;

        ExternalInterface::MessageEngineToGame message;
        if (message.Unpack(buffer.data(), buffer.size()) != buffer.size()) {
          PRINT_STREAM_ERROR("GameMessageHandler.MessageBufferWrongSize",
            "Buffer's size does not match expected size for this message ID. (Msg " <<
            ExternalInterface::MessageEngineToGameTagToString(message.GetTag()) << ", expected " << message.Size() <<
            ", recvd " << buffer.size() << ")");
        }

        if (messageCallback != nullptr) {
          messageCallback(message);
        }
        
        return retVal;
      } // ProcessBuffer()
      
      Result GameMessageHandler::ProcessMessages()
      {
        Result retVal = RESULT_FAIL;
        
        if(isInitialized_) {
          retVal = RESULT_OK;
          
          while(comms_->GetNumPendingMsgPackets() > 0)
          {
            std::vector<uint8_t> buffer;
            comms_->GetNextMsgPacket(buffer);
            
            if(ProcessPacket(buffer) != RESULT_OK) {
              retVal = RESULT_FAIL;
            }
          } // while messages are still available from comms
        }
        
        return retVal;
      } // ProcessMessages()
  
} // namespace Vector
} // namepsace Anki
