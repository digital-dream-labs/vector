/**
* File: speechRecognizerTHFTypesSimple.h
*
* Author: Lee Crippen
* Created: 04/20/2017
* Updated: 10/29/2017 Simple version rename to differentiate from legacy implementation.
*
* Description: SpeechRecognizer Sensory TrulyHandsFree type definitions
*
* Copyright: Anki, Inc. 2017
*
*/
#ifndef __Cozmo_Basestation_VoiceCommands_SpeechRecognizerTHFTypesSimple_H_
#define __Cozmo_Basestation_VoiceCommands_SpeechRecognizerTHFTypesSimple_H_

extern "C" {
#ifndef bool
#define bool_needsreset
#define bool
#endif
  
#include "trulyhandsfree.h"
  
#ifdef bool_needsreset
#undef bool
#endif
}

#include <memory>
#include <utility>

namespace Anki {
namespace Vector {

class RecogData
{
public:
  RecogData(recog_t* recog, searchs_t* search, bool isPhraseSpotted, bool allowsFollowupRecog);
  ~RecogData();
  
  RecogData(RecogData&& other);
  RecogData& operator=(RecogData&& other);
  
  RecogData(const RecogData& other) = delete;
  RecogData& operator=(const RecogData& other) = delete;
  
  recog_t* GetRecognizer() const { return _recognizer; }
  searchs_t* GetSearch() const { return _search; }
  
  static void DestroyData(recog_t*& recognizer, searchs_t*& search);
  
  bool IsPhraseSpotted() const { return _isPhraseSpotted; }
  bool AllowsFollowupRecog() const { return _allowsFollowupRecog; }
  
  
private:
  recog_t*          _recognizer = nullptr;
  searchs_t*        _search = nullptr;
  
  bool              _isPhraseSpotted = false;
  bool              _allowsFollowupRecog = false;
};
  
using RecogDataSP = std::shared_ptr<RecogData>;
  
template <typename ...Args>
static RecogDataSP MakeRecogDataSP(Args&& ...args)
{
  return std::shared_ptr<RecogData>(new RecogData(std::forward<Args>(args)...));
}

} // end namespace Vector
} // end namespace Anki

#endif // __Cozmo_Basestation_VoiceCommands_SpeechRecognizerTHFTypesSimple_H_
