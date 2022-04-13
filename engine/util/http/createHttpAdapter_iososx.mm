/**
 * File: createHttpAdapter_ios.mm
 *
 * Author: Molly Jameson
 * Date:   1/29/2016
 *
 * Description: Create native interface for http connections, the iOS/mac version
 *
 * Copyright: Anki, Inc. 2016
 **/

#include "engine/util/http/createHttpAdapter.h"
#include "util/http/httpAdapter_iososx.h"

Anki::Util::IHttpAdapter* CreateHttpAdapter()
{
  return new Anki::Util::HttpAdapter();
}
