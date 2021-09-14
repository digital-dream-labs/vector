/**
* File: spriteSequenceLoader.cpp
*
* Authors: Kevin M. Karol
* Created: 4/10/18
*
* Description:
*    Class that loads sprite sequences from data on worker threads and
*    returns the final sprite sequence container
*
* Copyright: Anki, Inc. 2018
*
**/

#include "cannedAnimLib/spriteSequences/spriteSequenceLoader.h"
#include "cannedAnimLib/proceduralFace/proceduralFace.h"

#include "coretech/common/engine/jsonTools.h"
#include "coretech/common/engine/utils/data/dataPlatform.h"
#include "coretech/common/engine/utils/data/dataScope.h"
#include "coretech/vision/engine/image.h"
#include "coretech/vision/shared/spriteCache/spriteCache.h"
#include "coretech/vision/shared/spriteSequence/spriteSequenceContainer.h"

#include "util/fileUtils/fileUtils.h"
#include "util/dispatchWorker/dispatchWorker.h"

#include <set>
#include <sys/stat.h>


namespace Anki {
namespace Vector {

namespace{
static const char* kLoopKey        = "loop";
static const char* kSequenceKey    = "sequence";
static const char* kSegmentTypeKey = "segmentType";
static const char* kFileListKey    = "fileList";

}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
Vision::SpriteSequenceContainer* SpriteSequenceLoader::LoadSpriteSequences(const Util::Data::DataPlatform* dataPlatform,
                                                                           Vision::SpritePathMap* spritePathMap,
                                                                           Vision::SpriteCache* cache,
                                                                           const std::vector<std::string>& spriteSequenceDirs)
{
  if (dataPlatform == nullptr) { 
    return nullptr;
  }
  Util::Data::Scope resourceScope = Util::Data::Scope::Resources;

  // Set up the worker that will process all the image frame folders
  
  using MyDispatchWorker = Util::DispatchWorker<3, Vision::SpriteCache*, std::string>;
  MyDispatchWorker::FunctionType workerFunction = std::bind(&SpriteSequenceLoader::LoadSequence, 
                                                            this, std::placeholders::_1, std::placeholders::_2);
  MyDispatchWorker worker(workerFunction);
  for(const auto& path : spriteSequenceDirs){
    const std::string spriteSeqFolder = dataPlatform->pathToResource(resourceScope, path);
    
    // Get the list of all the directory names
    std::vector<std::string> spriteSequenceDirNames;
    Util::FileUtils::ListAllDirectories(spriteSeqFolder, spriteSequenceDirNames);
    
    // Go through the list of directories, removing disallowed names, updating timestamps, and removing ones that don't need to be loaded
    auto nameIter = spriteSequenceDirNames.begin();
    while (nameIter != spriteSequenceDirNames.end())
    {
      const std::string& folderName = *nameIter;
      std::string fullDirPath = Util::FileUtils::FullFilePath({spriteSeqFolder, folderName});

      spritePathMap->AddAsset(folderName, fullDirPath, true);

      // Now we can start looking at the next name, this one is ok to load
      worker.PushJob(cache, fullDirPath);
      ++nameIter;
    }
    
    // Go through and load the sequences from our list
    worker.Process();
  }

  auto* container = new Vision::SpriteSequenceContainer(std::move(_spriteSequences));
  _spriteSequences.clear();
  
  return container;
} // LoadSpriteSequences()


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void SpriteSequenceLoader::LoadSequence(Vision::SpriteCache* cache, const std::string& fullDirectoryPath)
{

  // Even though files *might* be sorted alphabetically by the readdir call inside FilesInDirectory,
  // we can't rely on it so do it ourselves
  std::vector<std::string> fileNames = Util::FileUtils::FilesInDirectory(fullDirectoryPath);
  std::sort(fileNames.begin(), fileNames.end());
  auto specIter = fileNames.end();
  auto fileIter = fileNames.begin();
  while(fileIter != fileNames.end()){
    size_t dotPos = fileIter->find_last_of(".");
    if(dotPos == std::string::npos) {
      fileIter++;
      continue;
    }

    const std::string fileSuffix(fileIter->substr(dotPos, std::string::npos));
    if(fileSuffix == ".json"){
      specIter = fileIter;
      break;
    }
    fileIter++;
  }

  Vision::SpriteSequence seq;
  if(specIter == fileNames.end()){
    LoadSequenceLegacy(cache, fullDirectoryPath, fileNames, seq);
  }else{
    Json::Value spec;
    const std::string fullFilename = Util::FileUtils::FullFilePath({fullDirectoryPath, *specIter});
    const bool success = Util::Data::DataPlatform::readAsJson(fullFilename, spec);
    fileNames.erase(specIter);
    if(success){
      LoadSequenceFromSpec(cache, fullDirectoryPath, spec, fileNames, seq);
    }
  }

  // Place the sequence in the appropriate map
  std::lock_guard<std::mutex> guard(_mapMutex);
  _spriteSequences.emplace(Vision::SpritePathMap::GetAssetID(Util::FileUtils::GetFileName(fullDirectoryPath)), seq);
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void SpriteSequenceLoader::LoadSequenceLegacy(Vision::SpriteCache* cache,
                                              const std::string& fullDirectoryPath, 
                                              const std::vector<std::string>& relativeImgNames,
                                              Vision::SpriteSequence& outSeq) const
{
  for (auto fileIter = relativeImgNames.begin(); fileIter != relativeImgNames.end(); ++fileIter)
  {
    const std::string& filename = *fileIter;
    size_t underscorePos = filename.find_last_of("_");
    size_t dotPos = filename.find_last_of(".");
    if(dotPos == std::string::npos) {
      PRINT_NAMED_ERROR("SpriteSequenceLoader.LoadSequenceLegacy",
                        "Could not find '.' in frame filename %s",
                        filename.c_str());
      return;
    } else if(underscorePos == std::string::npos) {
      PRINT_NAMED_ERROR("SpriteSequenceLoader.LoadSequenceLegacy",
                        "Could not find '_' in frame filename %s",
                        filename.c_str());
      return;
    } else if(dotPos <= underscorePos+1) {
      PRINT_NAMED_ERROR("SpriteSequenceLoader.LoadSequenceLegacy",
                        "Unexpected relative positions for '.' and '_' in frame filename %s",
                        filename.c_str());
      return;
    }
    
    const std::string digitStr(filename.substr(underscorePos+1,
                                                (dotPos-underscorePos-1)));
    
    s32 frameNum = 0;
    try {
      frameNum = std::stoi(digitStr);
    } catch (std::invalid_argument&) {
      PRINT_NAMED_ERROR("SpriteSequenceLoader.LoadSequenceLegacy",
                        "Could not get frame number from substring '%s' "
                        "of filename '%s'.",
                        digitStr.c_str(), filename.c_str());
      return;
    }
    
    if(frameNum < 0) {
      PRINT_NAMED_ERROR("SpriteSequenceLoader.LoadSequenceLegacy",
                        "Found negative frame number (%d) for filename '%s'.",
                        frameNum, filename.c_str());
      return;
    }
    
    if(frameNum < outSeq.GetNumFrames()){
      PRINT_NAMED_ERROR("SpriteSequenceLoader.LoadSequenceLegacy",
                        "Image %s has frame number %d, but sequence already has %d frames - skipping frame",
                        filename.c_str(), frameNum, outSeq.GetNumFrames());
      continue;
    }
    
    // Load the image
    const std::string fullFilePath = Util::FileUtils::FullFilePath({fullDirectoryPath, filename});
    Vision::HSImageHandle faceHueAndSaturation = ProceduralFace::GetHueSatWrapper();
    Vision::SpriteHandle handle;
    handle = cache->GetSpriteHandleForSpritePath(fullFilePath, faceHueAndSaturation);
    
    if(frameNum != outSeq.GetNumFrames()){
      PRINT_NAMED_ERROR("SpriteSequenceLoader.LoadSequenceLegacy.MissingFrameNumbers",
                        "Sprite sequences must either start at 0 and have every frame number, \
                        or specify loading via JSON. Missing frame %d, have frime name %s",
                        outSeq.GetNumFrames(),
                        filename.c_str());
      break;
    }

    outSeq.AddFrame(handle);
  } // end for (auto fileIter)
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void SpriteSequenceLoader::LoadSequenceFromSpec(Vision::SpriteCache* cache,
                                                const std::string& fullDirectoryPath,
                                                const Json::Value& spec,
                                                const std::vector<std::string>& relativeImgNames,
                                                Vision::SpriteSequence& outSeq) const
{
  {
    
    const auto loopStr = JsonTools::ParseString(spec, kLoopKey, "SpriteSequenceLoader.LoadSequenceFromSpec.NoLoopString");
    Vision::SpriteSequence::LoopConfig loopConfig = Vision::SpriteSequence::LoopConfigFromString(loopStr);
    outSeq.SetLoopConfig(loopConfig);
  }
  const auto segmentArray = spec[kSequenceKey];
  if(ANKI_VERIFY(segmentArray.isArray(), 
                 "SpriteSequenceLoader.LoadSequenceFromSpec.NoSequenceArray", 
                 "")){
    for(const auto& segment: segmentArray){
      if(segment[kSegmentTypeKey] != "straightThrough"){
        PRINT_NAMED_ERROR("SpriteSequenceLoader.LoadSequenceFromSpec.InvalidSegmentType", 
                          "Segment type %s is not implemented in the sequence loader",
                          segment[kSegmentTypeKey].asString().c_str());
        continue;
      }
      const auto orderedFileArray = segment[kFileListKey];
      if(ANKI_VERIFY(orderedFileArray.isArray(), 
                     "SpriteSequenceLoader.LoadSequenceFromSpec.FileListIsNotAnArray", "")){
        for(const auto& fileNameJSON: orderedFileArray){
          const auto fileName = fileNameJSON.asString();

          auto iter = std::find (relativeImgNames.begin(), relativeImgNames.end(), fileName);
          if(iter != relativeImgNames.end()){
            const std::string& fullFilePath = Util::FileUtils::FullFilePath({fullDirectoryPath, fileName});
            Vision::HSImageHandle faceHueAndSaturation = ProceduralFace::GetHueSatWrapper();
            Vision::SpriteHandle handle;
            handle = cache->GetSpriteHandleForSpritePath(fullFilePath, faceHueAndSaturation);
            outSeq.AddFrame(handle);
          }else{
            PRINT_NAMED_ERROR("SpriteSequenceLoader.LoadSequenceFromSpec.FileNotInFolder",
                              "Could not find file %s in folder %s",
                              fileName.c_str(), fullDirectoryPath.c_str());
          }
        }
      }
    }
  }
}


} // namespace Vector
} // namespace Anki
