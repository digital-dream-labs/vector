/**
 * File: webVizSender.h
 *
 * Author: Brad Neuman
 * Created: 2018-08-11
 *
 * Description: RAII style helper to send json to webviz
 *
 * Copyright: Anki, Inc. 2018
 *
 **/

#ifndef __WebServerProcess_Src_WebVizSender_H__
#define __WebServerProcess_Src_WebVizSender_H__

#include "json/json.h"

#include <memory>

namespace Anki {
namespace Vector {
namespace WebService {

class WebService;

class WebVizSender
{
public:

  // Construct a webviz sender that will automatically send the contents of `data` to moduleName automatically
  // when it goes out of scope
  WebVizSender(const std::string& moduleName, WebService* webService);
  ~WebVizSender();

  Json::Value& Data() { return _data; }

  // helper that will return a shared ptr to an appropriate web viz sender if the specified module has a
  // client, otherwise (including if webService is null) it will return an empty (null) ptr.
  //
  // Warning: don't store this value long term. It stores a raw pointer to the web service, so if it's stored
  // (e.g. as a member variable) during engine tear down, undefined behavior might result)
  static std::shared_ptr<WebVizSender> CreateWebVizSender(const std::string& moduleName, WebService* webService);

private:

  void Send();

  Json::Value _data;
  std::string _module;
  WebService* _webService;
};

}
}
}


#endif
