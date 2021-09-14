/**
 * File: textToSpeechProvider_acapela.h
 *
 * Description: Declarations shared by various implementations of Acapela TTS SDK
 *
 * Copyright: Anki, Inc. 2017
 *
 */


#ifndef __Anki_cozmo_cozmoAnim_textToSpeech_textToSpeechProvider_acapela_H__
#define __Anki_cozmo_cozmoAnim_textToSpeech_textToSpeechProvider_acapela_H__

#include "util/global/globalDefinitions.h"

#include <string>

namespace Anki {
namespace Vector {
namespace TextToSpeech {

class AcapelaTTS
{
public:
  // Acapela Colibri voices are sampled at 22050hz
  static int GetSampleRate() { return 22050; }

  // Acapela Colibri voices are monophonic
  static int GetNumChannels() { return 1; }

  // Adjust base speed by unit scalar, then clamp to supported range
  static float GetSpeechRate(int speed, float durationScalar);

  // Adjust base pitch by unit scalar, then clamp to supported range
  static float GetAdjustedPitch(int pitch, float pitchScalar);

  // License info
  static int GetUserid();
  static int GetPassword();
  static std::string GetLicense();

};

} // end namespace TextToSpeech
} // end namespace Vector
} // end namespace Anki

#endif // __Anki_cozmo_cozmoAnim_textToSpeech_textToSpeechProvider_acapela_H__
