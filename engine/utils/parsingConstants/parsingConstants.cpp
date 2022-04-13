/**
 * File: parsingConstants.cpp
 *
 * Author: Damjan Stulic
 * Created: 1/Aug/2012
 *
 * Description: data needed for parsing config files
 *
 * Copyright: Anki, Inc. 2012
 *
 **/
#include "engine/utils/parsingConstants/parsingConstants.h"

#ifdef STRING_NAMED_CONST
#error "STRING_NAMED_CONST already defined. Please fix."
#endif

namespace AnkiUtil
{

//#include <string>
//#define STRING_NAMED_CONST(name, value) const std::string name(value);
#define STRING_NAMED_CONST(name, value) const char * const name = value;
#include "engine/utils/parsingConstants/parsingConstants.def"
#undef STRING_NAMED_CONST

}
