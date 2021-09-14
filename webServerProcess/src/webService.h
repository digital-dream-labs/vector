/**
 * File: webService.h
 *
 * Author: richard; adapted for Victor by Paul Terry 01/08/2018
 * Created: 4/17/2017
 *
 * Description: Provides interface to civetweb, an embedded web server
 *
 *
 * Copyright: Anki, Inc. 2017-2018
 *
 **/
#ifndef WEB_SERVICE_H
#define WEB_SERVICE_H

#include "civetweb/include/civetweb.h"

#include "json/json.h"

#include <string>
#include <vector>
#include <mutex>
#include <unordered_set>
#include <unordered_map>

#include "util/signals/simpleSignal.hpp"

struct mg_context; // copied from civetweb.h
struct mg_connection;

namespace Json {
  class Value;
}

namespace Anki {

namespace Util {

namespace Data {
  class DataPlatform;
}
namespace Dispatch {
  class Queue;
}

} // namespace Util

namespace Vector {

namespace WebService {

class WebService
{
public:
  
  WebService();
  ~WebService();

  void Start(Anki::Util::Data::DataPlatform* platform, const Json::Value& config);
  void Update();
  void Stop();
  
  // send data to any client subscribed to moduleName
  void SendToWebSockets(const std::string& moduleName, const Json::Value& data) const;
  
  inline void SendToWebViz(const std::string& moduleName, const Json::Value& data) const { SendToWebSockets(moduleName, data); }
  
  // returns true if a client has subscribed to a given module name (or any module if empty)
  bool IsWebVizClientSubscribed(const std::string& moduleName = {}) const;
  
  // subscribe to when a client connects and notifies the webservice that they want data for moduleName
  using SendToClientFunc = std::function<void(const Json::Value&)>;
  using OnWebVizSubscribedType = Signal::Signal<void(const SendToClientFunc&)>;
  OnWebVizSubscribedType& OnWebVizSubscribed(const std::string& moduleName) { return _webVizSubscribedSignals[moduleName]; }
  
  // subscribe to when a client (who is listening to moduleName) sends data back to the webservice
  using OnWebVizDataType = Signal::Signal<void(const Json::Value&,const SendToClientFunc&)>;
  OnWebVizDataType& OnWebVizData(const std::string& moduleName) { return _webVizDataSignals[moduleName]; }
  
  // temp signals for the app sending data and requesting data
  using OnAppToEngineOnDataType = Signal::Signal<std::string(const std::string&)>;
  OnAppToEngineOnDataType& OnAppToEngineOnData() { return _appToEngineOnData; }
  using OnAppToEngineRequestDataType = Signal::Signal<std::string()>;
  OnAppToEngineRequestDataType& OnAppToEngineRequestData() { return _appToEngineRequestData; }
  
  const std::string& getConsoleVarsTemplate();

  enum class WhichWebServer
  {
    Standalone = 0,
    Engine     = 1,
    Anim       = 2
  };

  enum RequestType
  {
    RT_ConsoleVarsUI,
    RT_ConsoleVarGet,
    RT_ConsoleVarSet,
    RT_ConsoleVarList,
    RT_ConsoleFuncList,
    RT_ConsoleFuncCall,
    
    RT_External,
    
    RT_TempAppToEngine,
    RT_TempEngineToApp,
    
    RT_WebsocketOnSubscribe,
    RT_WebsocketOnData,
  };

  struct Request;
  using ExternalCallback = int (*)(WebService::WebService::Request* request);

  struct Request
  {
    Request(RequestType rt, const std::string& param1, const std::string& param2);
    Request(RequestType rt, const std::string& param1, const std::string& param2,
            const std::string& param3, ExternalCallback extCallback, void* cbdata);
    RequestType _requestType;
    std::string _param1;
    std::string _param2;
    std::string _param3;
    ExternalCallback _externalCallback;
    void*       _cbdata;
    std::string _result;
    bool        _resultReady; // Result is ready for use by the webservice thread
    bool        _done;        // Result has been used and now it's OK for main thread to delete this item
    std::mutex  _readyMutex;
    std::condition_variable _readyCondition;
  };

  void AddRequest(Request* requestPtr);
  std::mutex _requestMutex;

  const Json::Value& GetConfig() { return _config; }
  const Anki::Util::Data::DataPlatform* GetPlatform() { return _platform; }

  void RegisterRequestHandler(std::string uri, mg_request_handler handler, void* cbdata);
  int ProcessRequestExternal(struct mg_connection *conn, void* cbdata,
                             ExternalCallback extCallback, const std::string& param1 = "",
                             const std::string& param2 = "", const std::string& param3 = "");

private:

  void GenerateConsoleVarsUI(std::string& page, const std::string& category,
                             const bool standalone);

  struct WebSocketConnectionData {
    struct mg_connection* conn = nullptr;
    std::unordered_set<std::string> subscribedModules;
  };
  
  // called by civetweb
  static void HandleWebSocketsReady(struct mg_connection* conn, void* cbparams);
  static int HandleWebSocketsConnect(const struct mg_connection* conn, void* cbparams);
  static int HandleWebSocketsData(struct mg_connection* conn, int bits, char* data, size_t dataLen, void* cbparams);
  static void HandleWebSocketsClose(const struct mg_connection* conn, void* cbparams);
  
  // called by the above handlers
  void OnOpenWebSocket(struct mg_connection* conn);
  void OnReceiveWebSocket(struct mg_connection* conn, const Json::Value& data);
  void OnCloseWebSocket(const struct mg_connection* conn);

  void SendToWebSocket(struct mg_connection* conn, const Json::Value& data) const;

  // todo: OTA update somehow?

  struct mg_context* _ctx;
  
  std::vector<WebSocketConnectionData> _webSocketConnections;
  mutable std::mutex s_wsConnectionsMutex;

  std::string _consoleVarsUIHTMLTemplate;

  std::vector<Request*> _requests;

  Json::Value _config;
  Anki::Util::Data::DataPlatform* _platform;
 
  std::unordered_map<std::string, OnWebVizSubscribedType> _webVizSubscribedSignals;
  std::unordered_map<std::string, OnWebVizDataType> _webVizDataSignals;
  
  OnAppToEngineOnDataType _appToEngineOnData;
  OnAppToEngineRequestDataType _appToEngineRequestData;
  
  Util::Dispatch::Queue* _dispatchQueue = nullptr;
};

} // namespace WebService
  
} // namespace Vector
} // namespace Anki

#endif // defined(WEB_SERVICE_H)
