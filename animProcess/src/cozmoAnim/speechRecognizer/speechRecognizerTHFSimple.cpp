/**
* File: speechRecognizerTHFSimple.cpp
*
* Author: Lee Crippen
* Created: 12/12/2016
* Updated: 10/29/2017 Simple version rename to differentiate from legacy implementation.
*
* Description: SpeechRecognizer implementation for Sensory TrulyHandsFree. The cpp
* defines the impl struct that is only declared in the header, in order to encapsulate
* accessing outside headers to only be in this file.
*
* Copyright: Anki, Inc. 2016
*
*/

#include "speechRecognizerTHFSimple.h"

#include "audioUtil/speechRecognizer.h"
#include "speechRecognizerTHFTypesSimple.h"
#include "util/logging/logging.h"
#include "util/math/numericCast.h"
#include "util/string/stringUtils.h"

#include <algorithm>
#include <array>
#include <locale>
#include <map>
#include <mutex>
#include <string>
#include <sstream>

namespace Anki {
namespace Vector {
  
#define LOG_CHANNEL "SpeechRecognizer"
  
// Anonymous namespace to use with debug console vars for manipulating input
namespace {
  std::string sPhraseForceHeard = "";
}

void SpeechRecognizerTHF::SetForceHeardPhrase(const char* phrase)
{
  sPhraseForceHeard = (phrase == nullptr) ? "" : phrase;
}

namespace {
  // THF keyword for "none of the above" that can be used in non-phrasespotted grammars and search lists
  const std::string kNotaString = "*nota";
  
  // Defaults based on the THF documentation for phrasespotting params A and B
//  const int kPhraseSpotParamADefault = -1200;
//  const int kPhraseSpotParamBDefault = 500;
}

// Local definition of data used internally for more strict encapsulation
struct SpeechRecognizerTHF::SpeechRecognizerTHFData
{
  thf_t*      thfSession = nullptr;
  
  // We intentionally don't store off and reuse the pronun object. Attempting to do so during testing resulted in
  // crashes when calling into thfSearchCreateFromGrammar and passing in a common pronun object. The safe way to use
  // the pronun object appears to be creating, using, and then destroying it each time a search object is to be created.
  std::string thfPronunPath;
  
  IndexType                         thfCurrentRecog = InvalidIndex;
  IndexType                         thfFollowupRecog = InvalidIndex;
  std::map<IndexType, RecogDataSP>  thfAllRecogs;
  mutable std::recursive_mutex      recogMutex;
  const recog_t*                    lastUsedRecognizer = nullptr;
  size_t                            sampleRate_kHz = 0;
  uint64_t                          sampleIndex = 0;
  uint64_t                          lastResetSampleIndex = 0;
  bool                              disabled = false;
  bool                              reset = false;
  
  const RecogDataSP RetrieveDataForIndex(IndexType index) const;
};
  
SpeechRecognizerTHF::SpeechRecognizerTHF()
: _impl(new SpeechRecognizerTHFData())
{
  
}

SpeechRecognizerTHF::~SpeechRecognizerTHF()
{
  Cleanup();
}

SpeechRecognizerTHF::SpeechRecognizerTHF(SpeechRecognizerTHF&& other)
: SpeechRecognizer(std::move(other))
{
  SwapAllData(other);
}

SpeechRecognizerTHF& SpeechRecognizerTHF::operator=(SpeechRecognizerTHF&& other)
{
  SpeechRecognizer::operator=(std::move(other));
  SwapAllData(other);
  return *this;
}

void SpeechRecognizerTHF::SwapAllData(SpeechRecognizerTHF& other)
{
  auto temp = std::move(other._impl);
  other._impl = std::move(this->_impl);
  this->_impl = std::move(temp);
}

const RecogDataSP SpeechRecognizerTHF::SpeechRecognizerTHFData::RetrieveDataForIndex(IndexType index) const
{
  std::lock_guard<std::recursive_mutex> lock(recogMutex);
  if (index == InvalidIndex)
  {
    return RecogDataSP();
  }
  
  // We can only use recognizers that actually exist
  auto indexIter = thfAllRecogs.find(index);
  if (indexIter == thfAllRecogs.end())
  {
    return RecogDataSP();
  }
  
  // Intentionally make a local copy of the shared ptr with the current recog data
  return indexIter->second;
}

void SpeechRecognizerTHF::SetRecognizerIndex(IndexType index)
{
  std::lock_guard<std::recursive_mutex>(_impl->recogMutex);
  _impl->thfCurrentRecog = index;
}
  
void SpeechRecognizerTHF::SetRecognizerFollowupIndex(IndexType index)
{
  std::lock_guard<std::recursive_mutex>(_impl->recogMutex);
  _impl->thfFollowupRecog = index;
}

SpeechRecognizerTHF::IndexType SpeechRecognizerTHF::GetRecognizerIndex() const
{
  std::lock_guard<std::recursive_mutex>(_impl->recogMutex);
  return _impl->thfCurrentRecog;
}

void SpeechRecognizerTHF::RemoveRecognitionData(IndexType index)
{
  std::lock_guard<std::recursive_mutex> lock(_impl->recogMutex);
  auto indexIter = _impl->thfAllRecogs.find(index);
  if (indexIter != _impl->thfAllRecogs.end())
  {
    _impl->thfAllRecogs.erase(indexIter);
  }
}

bool SpeechRecognizerTHF::Init(const std::string& pronunPath)
{
  Cleanup();
  
  /* Create SDK session */
  thf_t* createdSession = thfSessionCreate();
  if(nullptr == createdSession)
  {
    /* as of SDK 3.0.9 thfGetLastError(NULL) will return a valid string */
    const char *err=thfGetLastError(NULL) ? thfGetLastError(NULL) : "could not find dll or out of memory";
    std::string failMessage = std::string("ERROR thfSessionCreate ") + err;
    HandleInitFail(failMessage);
    return false;
  }
  _impl->thfSession = createdSession;
  
  // Store the pronunciation file path
  _impl->thfPronunPath = pronunPath;
  
  return true;
}

void SpeechRecognizerTHF::HandleInitFail(const std::string& failMessage)
{
  LOG_ERROR("SpeechRecognizerTHF.Init.Fail", "%s", failMessage.c_str());
  Cleanup();
}

bool SpeechRecognizerTHF::AddRecognitionDataFromFile(IndexType index,
                                                     const std::string& nnFilePath, const std::string& searchFilePath,
                                                     bool isPhraseSpotted, bool allowsFollowupRecog)
{
  std::lock_guard<std::recursive_mutex> lock(_impl->recogMutex);
  recog_t* createdRecognizer = nullptr;
  searchs_t* createdSearch = nullptr;
  
  auto cleanupAfterFailure = [&] (const std::string& failMessage)
  {
    LOG_ERROR("SpeechRecognizerTHF.AddRecognitionDataFromFile.Fail", "%s %s", failMessage.c_str(), thfGetLastError(_impl->thfSession));
    RecogData::DestroyData(createdRecognizer, createdSearch);
  };
  
  if (InvalidIndex == index)
  {
    cleanupAfterFailure(std::string("Specified index matches InvalidIndex and cannot be used: ") + std::to_string(index));
    return false;
  }
  
  // First check whether this spot is already taken
  auto indexIter = _impl->thfAllRecogs.find(index);
  if (indexIter != _impl->thfAllRecogs.end())
  {
    cleanupAfterFailure(std::string("Recognizer already added at index ") + std::to_string(index));
    return false;
  }
  
  // The code examples had a buffer size as double the standard chunk size, so we'll do the same
  auto bufferSizeInSamples = Util::numeric_cast<unsigned short>(AudioUtil::kSamplesPerChunk * 2);
  
  /* Create recognizer */
  auto doSpeechDetect = isPhraseSpotted ? NO_SDET : SDET;
  createdRecognizer = thfRecogCreateFromFile(_impl->thfSession, nnFilePath.c_str(), bufferSizeInSamples, -1, doSpeechDetect);
  if(nullptr == createdRecognizer)
  {
    cleanupAfterFailure("ERROR thfRecogCreateFromFile");
    return false;
  }
  
  /* Create search */
  constexpr unsigned short numBestResultsToReturn = 1;
  createdSearch = thfSearchCreateFromFile(_impl->thfSession, createdRecognizer, searchFilePath.c_str(), numBestResultsToReturn);
  if(nullptr == createdSearch)
  {
    const char *err = thfGetLastError(_impl->thfSession);
    std::string errorMessage = std::string("ERROR thfSearchCreateFromFile ") + err;
    cleanupAfterFailure(errorMessage);
    return false;
  }
  
  /* Initialize recognizer */
  if(!thfRecogInit(_impl->thfSession, createdRecognizer, createdSearch, RECOG_KEEP_NONE))
  {
    cleanupAfterFailure("ERROR thfRecogInit");
    return false;
  }

  // extract sample rate (so it matches file)
  size_t sampleRate_hz = thfRecogGetSampleRate(_impl->thfSession, createdRecognizer);
  if( ! ANKI_VERIFY(sampleRate_hz != 0,
                    "SpeechRecognizerTHF.Init.NoSampleRate",
                    "Could not get sample rate from model") ) {
    // set it to a valid value to avoid divide by 0
    sampleRate_hz = 16000;
  }
  _impl->sampleRate_kHz = sampleRate_hz / 1000;
  
  if (allowsFollowupRecog)
  {
    if (!isPhraseSpotted)
    {
      cleanupAfterFailure("Tried to set up phrase following with non-phrasespotting recognizers, which is not allowed.");
      return false;
    }
    
    constexpr float overlapTime_ms = 1000.f;
    if (!thfPhrasespotConfigSet(_impl->thfSession, createdRecognizer, createdSearch, PS_SEQ_BUFFER, overlapTime_ms))
    {
      cleanupAfterFailure("ERROR thfPhrasespotConfigSet PS_SEQ_BUFFER");
      return false;
    }
  }
  
  // This delay is recommended when using more complex command sets, but ours is simple for the moment. Keeping this
  // here for now just for reference.
  // if (isPhraseSpotted)
  // {
  //   if (!thfPhrasespotConfigSet(_impl->thfSession, createdRecognizer, createdSearch, PS_DELAY, 90))
  //   {
  //     cleanupAfterFailure("ERROR thfPhrasespotConfigSet PS_DELAY");
  //     return false;
  //   }
  // }
  
  // Everything should be happily added, so store off this recognizer
  _impl->thfAllRecogs[index] = MakeRecogDataSP(createdRecognizer, createdSearch, isPhraseSpotted, allowsFollowupRecog);
  
  return true;
}

void SpeechRecognizerTHF::Cleanup()
{
  std::lock_guard<std::recursive_mutex> lock(_impl->recogMutex);
  _impl->thfAllRecogs.clear();
  
  if (_impl->thfSession)
  {
    thfSessionDestroy(_impl->thfSession);
    _impl->thfSession = nullptr;
  }
}

bool SpeechRecognizerTHF::RecogStatusIsEndCondition(uint16_t status)
{
  switch (status)
  {
    case RECOG_SILENCE:   //Timed out waiting for start of speech (end condition).
    case RECOG_DONE:      //End of utterance detected (end condition).
    case RECOG_MAXREC:    //Timed out waiting for end of utterance (end condition).
    case RECOG_IGNORE:    //Speech detector triggered but failed the minduration test. Probably not speech (end condition).
    case RECOG_NODATA:    //The input audio buffer was empty (error condition).
      return true;
    default:
      return false;
  }
}

void SpeechRecognizerTHF::Update(const AudioUtil::AudioSample * audioData, unsigned int audioDataLen)
{
  // Intentionally make a local copy of the shared ptr with the current recog data
  RecogDataSP currentRecogSP;
  {
    std::lock_guard<std::recursive_mutex> lock(_impl->recogMutex);
    currentRecogSP = _impl->RetrieveDataForIndex(GetRecognizerIndex());
  }

  if (nullptr == currentRecogSP)
  {
    return;
  }

  // track the total number of samples processed
  // NOTE: on a 32 bit system with 16Khz audio this could overflow in 3 days....
  _impl->sampleIndex += audioDataLen;
  
  if (_impl->disabled)
  {
    // Don't process audio data in recognizer
    return;
  }
  
  // If the recognizer has changed since last update, we need to potentially reset and store it again
  auto* const currentRecognizer = currentRecogSP->GetRecognizer();
  if (_impl->reset || currentRecognizer != _impl->lastUsedRecognizer)
  {
    // If we actually had a last recognizer set, then we need to reset
    if (_impl->reset || _impl->lastUsedRecognizer)
    {
      if(thfRecogReset(_impl->thfSession, currentRecognizer))
      {
        _impl->lastResetSampleIndex = _impl->sampleIndex;
      }
      else
      {
        LOG_ERROR("SpeechRecognizerTHF.Update.thfRecogReset.Fail", "%s", thfGetLastError(_impl->thfSession));
      }
    }
    _impl->lastUsedRecognizer = currentRecognizer;
    _impl->reset = false;
  }
  
  auto recogPipeMode = currentRecogSP->IsPhraseSpotted() ? RECOG_ONLY : SDET_RECOG;
  unsigned short status = RECOG_SILENCE;
  if(!thfRecogPipe(_impl->thfSession, currentRecognizer, audioDataLen, (short*)audioData, recogPipeMode, &status))
  {
    LOG_ERROR("SpeechRecognizerTHF.Update.thfRecogPipe.Fail", "%s", thfGetLastError(_impl->thfSession));
    return;
  }
  
  if (!sPhraseForceHeard.empty() || RecogStatusIsEndCondition(status))
  {
    float score = 0;
    const char* foundStringRaw = nullptr;
    const char* wordAlign = nullptr;
    if (sPhraseForceHeard.empty())
    {
      if (!thfRecogResult(_impl->thfSession, currentRecognizer, &score, &foundStringRaw, &wordAlign, NULL, NULL, NULL, NULL, NULL))
      {
        LOG_ERROR("SpeechRecognizerTHF.Update.thfRecogResult.Fail", "%s", thfGetLastError(_impl->thfSession));
      }
    }
    else
    {
      foundStringRaw = sPhraseForceHeard.c_str();
      score = -1.0f;
      status = RECOG_DONE;
    }
    
    if (foundStringRaw != nullptr && foundStringRaw[0] != '\0' && kNotaString.compare(foundStringRaw) != 0)
    {
      // Get results for callback struct
      std::string foundString{foundStringRaw};
      std::replace(foundString.begin(), foundString.end(), '_', ' ');
      AudioUtil::SpeechRecognizerCallbackInfo info {
        .result       = foundString,
        .startTime_ms = 0,
        .endTime_ms   = 0,
        .score        = score
      };
      
      if( wordAlign != nullptr ) {
        std::string wordTimesS{wordAlign};
        // example: "21795 22440 hey_vector 0.00"
        auto split = Util::StringSplit(wordTimesS, ' ' );
        if( split.size() >= 2 ) {
          info.startTime_ms = std::atoi(split[0].c_str());
          info.endTime_ms = std::atoi(split[1].c_str()); // hope these are ints

          // convert to sample counts
          info.startSampleIndex = ( info.startTime_ms * _impl->sampleRate_kHz ) + _impl->lastResetSampleIndex;
          info.endSampleIndex   = ( info.endTime_ms   * _impl->sampleRate_kHz ) + _impl->lastResetSampleIndex;
        }
      }
      
      DoCallback(info);
      LOG_INFO("SpeechRecognizerTHF.Update", "Recognizer -  %s", info.Description().c_str());
    }
    
    // If the current recognizer allows a followup recognizer to immediately take over
    if (status == RECOG_DONE && currentRecogSP->AllowsFollowupRecog())
    {
      // Verify whether we actually have a followup recognizer set
      const RecogDataSP nextRecogSP = _impl->RetrieveDataForIndex(_impl->thfFollowupRecog);
      if (nextRecogSP)
      {
        // Actually do the switch over to the new recognizer (as long as this phrase wasn't forced), which copies some buffered audio data
        if (!sPhraseForceHeard.empty() ||
            thfRecogPrepSeq(_impl->thfSession, nextRecogSP->GetRecognizer(), currentRecognizer))
        {
          std::lock_guard<std::recursive_mutex>(_impl->recogMutex);
          LOG_INFO("SpeechRecognizerTHF.Update",
                   "Switching current recog from %d to %d",
                   _impl->thfCurrentRecog, _impl->thfFollowupRecog);
          _impl->thfCurrentRecog = _impl->thfFollowupRecog;
          _impl->thfFollowupRecog = InvalidIndex;
        }
        else
        {
          LOG_ERROR("SpeechRecognizerTHF.Update.thfRecogPrepSeq.Fail", "%s", thfGetLastError(_impl->thfSession));
        }
      }
    }
    
    if (thfRecogReset(_impl->thfSession, currentRecognizer))
    {
      _impl->lastResetSampleIndex = _impl->sampleIndex;
    }
    else
    {
      LOG_ERROR("SpeechRecognizerTHF.Update.thfRecogReset.Fail", "%s", thfGetLastError(_impl->thfSession));
    }
    
    sPhraseForceHeard = "";
  }
}
  
void SpeechRecognizerTHF::StartInternal()
{
  if (_impl->disabled) {
    _impl->reset = true;
  }
  _impl->disabled = false;
}

void SpeechRecognizerTHF::StopInternal()
{
  _impl->disabled = true;
}

void SpeechRecognizerTHF::Reset()
{
  _impl->reset = true;
}

} // end namespace Vector
} // end namespace Anki
