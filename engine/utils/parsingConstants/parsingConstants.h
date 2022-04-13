/**
 * File: parsingConstants.h
 *
 * Author: Damjan Stulic
 * Created: 1/Aug/2012
 * 
 * Description: data needed for parsing config files
 *
 * Copyright: Anki, Inc. 2012
 *
 **/

#ifndef UTIL_PARSING_CONSTANTS_H_
#define UTIL_PARSING_CONSTANTS_H_

namespace AnkiUtil
{
  
  #ifdef STRING_NAMED_CONST
  #error "STRING_NAMED_CONST already defined. Please fix."
  #endif

  //#define STRING_NAMED_CONST(name, value) extern const std::string name;
  #define STRING_NAMED_CONST(name, value) __attribute__((visibility("default"))) extern char const * const name;
  #include "engine/utils/parsingConstants/parsingConstants.def"
  #undef STRING_NAMED_CONST
  
}

#endif
