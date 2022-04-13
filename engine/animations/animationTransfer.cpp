/**
 * File: animationTransfer.cpp
 *
 * Author: Molly Jameson
 * Created: 09/22/16
 *
 * Description: Container for chunked uploads for the SDK uploading animation files at runtime
 *
 * Copyright: Anki, Inc. 2016
 *
 **/


#include "engine/animations/animationTransfer.h"

#include "engine/externalInterface/externalInterface.h"
#include "clad/externalInterface/messageGameToEngine.h"
#include "coretech/common/engine/utils/data/dataPlatform.h"
#include "util/fileUtils/fileUtils.h"
#include "util/logging/logging.h"


namespace Anki {
namespace Vector {
  
  const std::string AnimationTransfer::kCacheAnimFileName("NewAnim.json");
  const std::string AnimationTransfer::kCacheFaceAnimsDir(Anki::Util::FileUtils::FullFilePath({"assets", "faceAnimations"}));
  
  AnimationTransfer::AnimationTransfer(Anki::Vector::IExternalInterface* externalInterface, Anki::Util::Data::DataPlatform* dataPlatform)
  {
    _externalInterface = externalInterface;
    _dataPlatform = dataPlatform;
    
    auto callback = std::bind(&AnimationTransfer::HandleGameEvents, this, std::placeholders::_1);
    _signalHandle = _externalInterface->Subscribe(ExternalInterface::MessageGameToEngineTag::TransferFile, callback);
    CleanUp();
  }
  
  // Clean up the temp resources
  AnimationTransfer::~AnimationTransfer()
  {
    CleanUp();
  }
  
  void AnimationTransfer::CleanUp(bool removeFaceImgDir)
  {
    std::string full_path = _dataPlatform->pathToResource(Anki::Util::Data::Scope::Cache, kCacheAnimFileName);
    if( Util::FileUtils::FileExists(full_path) )
    {
      Util::FileUtils::DeleteFile(full_path);
    }
    
    // Face animation cleanup
    if( removeFaceImgDir && !_lastFaceAnimDir.empty() )
    {
      std::string face_anim_dir = _dataPlatform->pathToResource(Anki::Util::Data::Scope::Cache,
                                                                kCacheFaceAnimsDir);
      std::string full_path_face_imgs = Anki::Util::FileUtils::FullFilePath({face_anim_dir, _lastFaceAnimDir});
      if (Anki::Util::FileUtils::DirectoryExists(full_path_face_imgs))
      {
        Anki::Util::FileUtils::RemoveDirectory(full_path_face_imgs);
      }
      _lastFaceAnimDir.clear();
    }
    _expectedNextChunk = 0;
  }
  
  void AnimationTransfer::HandleGameEvents(const AnkiEvent<ExternalInterface::MessageGameToEngine>& event)
  {
    if( event.GetData().GetTag() == ExternalInterface::MessageGameToEngineTag::TransferFile )
    {
      const Anki::Vector::ExternalInterface::TransferFile& msg = event.GetData().Get_TransferFile();
      
      if ((msg.fileType != ExternalInterface::FileType::Animation) &&
          (msg.fileType != ExternalInterface::FileType::FaceImg))
      {
        // File type is handled elsewhere
        return;
      }
      
      // Verify this is the chunk we're waiting for.
      if( _expectedNextChunk == msg.filePart)
      {
        ++_expectedNextChunk;
      }
      else if( msg.filePart == 0 )
      {
        // resets expectedNextChunk
        // processing FaceImg below does another cleanup if the subdirectory has changed,
        // but we don't want to remove the whole directory if this is just a new image in the same set.
        CleanUp(msg.fileType != ExternalInterface::FileType::FaceImg);
        ++_expectedNextChunk;
      }
      else
      {
        PRINT_NAMED_ERROR("FileTransfer.Upload","File Part unexpected got: %d expected: %d",msg.filePart,_expectedNextChunk);
        CleanUp();
        return;
      }
      
      if( msg.fileType == ExternalInterface::FileType::Animation)
      {
        // Store data if not the last chunk.
        // If it is the last chunk.
        // Write out the files
        // Tell animation system to read in files again ( including cache )
        std::string full_path = _dataPlatform->pathToResource(Util::Data::Scope::Cache, kCacheAnimFileName);
        // Clear the old file if this is the first chunk
        if( msg.filePart == 0)
        {
          CleanUp();
          _expectedNextChunk = 1;
        }

        //Append so not keeping all chunks in memory.
        Util::FileUtils::WriteFile(full_path, msg.fileBytes,true);
        // This was the last chunk, refresh the animations.
        if( msg.filePart == msg.numFileParts - 1)
        {
          ExternalInterface::MessageGameToEngine read_msg;
          ExternalInterface::ReadAnimationFile m;
          m.full_path = full_path;
          read_msg.Set_ReadAnimationFile(m);
          _externalInterface->Broadcast(std::move(read_msg));
        }
      }
      else if( msg.fileType == ExternalInterface::FileType::FaceImg)
      {
        std::size_t found_char = msg.filename.find_last_of("_");
        if( found_char != std::string::npos)
        {
          // first part of the filename without the frame number is the directory name
          std::string currDir = msg.filename.substr(0,found_char);
          // This is the first of a batch of frames
          if( _lastFaceAnimDir != currDir )
          {
            // clean up any previous uploads.
            CleanUp();
            
            _lastFaceAnimDir = currDir;
            std::string face_anim_dir = _dataPlatform->pathToResource(Util::Data::Scope::Cache, kCacheFaceAnimsDir);
            std::string full_path_face_imgs = Anki::Util::FileUtils::FullFilePath({face_anim_dir, _lastFaceAnimDir});
            Anki::Util::FileUtils::CreateDirectory(full_path_face_imgs);
          }
        }
        if( !_lastFaceAnimDir.empty() )
        {
          std::string face_anim_dir = _dataPlatform->pathToResource(Util::Data::Scope::Cache,kCacheFaceAnimsDir);
          std::string full_path_face_imgs = Anki::Util::FileUtils::FullFilePath({face_anim_dir, _lastFaceAnimDir,msg.filename});
          Util::FileUtils::WriteFile(full_path_face_imgs, msg.fileBytes,true);
        }
      }
    }
  }
} // namespace Vector
} // namespace Anki

