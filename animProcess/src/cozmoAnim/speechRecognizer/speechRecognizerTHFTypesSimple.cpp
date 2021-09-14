/**
* File: speechRecognizerTHFTypesSimple.cpp
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

#include "speechRecognizerTHFTypesSimple.h"

namespace Anki {
namespace Vector {

RecogData::RecogData(recog_t* recog, searchs_t* search, bool isPhraseSpotted, bool allowsFollowupRecog)
: _recognizer(recog)
, _search(search)
, _isPhraseSpotted(isPhraseSpotted)
, _allowsFollowupRecog(allowsFollowupRecog)
{ }

RecogData::~RecogData()
{
  DestroyData(_recognizer, _search);
}

RecogData::RecogData(RecogData&& other)
: _recognizer(other._recognizer)
, _search(other._search)
{
  other._recognizer = nullptr;
  other._search = nullptr;
}

RecogData& RecogData::operator=(RecogData&& other) // move assignment
{
  if(this != &other) // prevent self-move
  {
    DestroyData(_recognizer, _search);
    _recognizer = other._recognizer;
    _search = other._search;
    other._recognizer = nullptr;
    other._search = nullptr;
  }
  return *this;
}

void RecogData::DestroyData(recog_t*& recognizer, searchs_t*& search)
{
  if (recognizer)
  {
    thfRecogDestroy(recognizer);
    recognizer = nullptr;
  }
  
  if (search)
  {
    thfSearchDestroy(search);
    search = nullptr;
  }
}
  
} // end namespace Vector
} // end namespace Anki
