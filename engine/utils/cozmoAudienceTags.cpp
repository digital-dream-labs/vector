/**
 * File: cozmoAudienceTags
 *
 * Author: baustin
 * Created: 7/24/17
 *
 * Description: Light wrapper for AudienceTags to initialize it with Cozmo-specific configuration
 *
 * Copyright: Anki, Inc. 2017
 *
 **/

#include "engine/utils/cozmoAudienceTags.h"

#include "engine/cozmoContext.h"
#include "util/environment/locale.h"
#include "util/string/stringUtils.h"
#include <chrono>

namespace Anki {
namespace Vector {

CozmoAudienceTags::CozmoAudienceTags(const CozmoContext* context)
{
  // Define audience tags that will be used in Cozmo and provide handlers to determine if they apply

  // first day user
  // NOTE:  This works for manually-started experiments, but not for automatic experiments.  This is
  // because we're calling this handler from AutoActivateExperiments from constructors, that is
  // happening well before we get to initialize the needs manager and read the 'time created' from
  // device.
  auto firstDayUserHandler = [] {
    // TODO:  Need to write some other logic to determine first-day user; previously this was
    // based on the needs system's saved state's 'creation time'
    const bool firstDayUser = false;
    return firstDayUser;
  };

  RegisterTag("app_user_d0", firstDayUserHandler);

  auto localeLanguageHandler = [context] {
    Util::Locale* locale = context->GetLocale();
    std::string locale_language_tag =
      "locale_language_" + Util::StringToLower(locale->GetLanguageString());
    return locale_language_tag;
  };
  RegisterDynamicTag(localeLanguageHandler);

  auto localeCountryHandler = [context] {
    Util::Locale* locale = context->GetLocale();
    std::string locale_country_tag =
      "locale_country_" + Util::StringToLower(locale->GetCountryString());
    return locale_country_tag;
  };
  RegisterDynamicTag(localeCountryHandler);
}

}
}
