/**
 * File: createHttpAdapter_vicos
 *
 * Author: baustin
 * Date: 7/22/16
 *
 * Description: Create native interface for http connections, Android edition
 *
 * Copyright: Anki, Inc. 2016
 **/

#include "engine/util/http/createHttpAdapter.h"
#include "util/http/httpAdapter_vicos.h"

Anki::Util::IHttpAdapter* CreateHttpAdapter()
{
  return new Anki::Util::HttpAdapter();
}
