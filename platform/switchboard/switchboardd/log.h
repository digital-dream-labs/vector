/**
 * File: log.h
 *
 * Author: paluri
 * Created: 1/31/2018
 *
 * Description: Abstracted log for ankiswitchboardd
 *
 * Copyright: Anki, Inc. 2018
 *
 **/

#ifndef log_h
#define log_h

#define debug_logging

#include "anki-ble/common/log.h"
#include <cstdio>
#include <thread>
#include <stdlib.h>

class Log {
  public:
  template<typename... Args>
  static void Write(Args&&... args) {
    logi(std::forward<Args>(args)...);
  }

  template<typename... Args>
  static void Error(Args&&... args) {
    loge(std::forward<Args>(args)...);
  }

  template<typename... Args>
  static void Green(Args&&... args) {
    char buffer [200];
    snprintf(buffer, 200, "%s", std::forward<Args>(args)...);
    printf("ankiswitchboardd: \033[42;30m%s\033[0m\n", buffer);
  }

  template<typename... Args>
  static void Blue(Args&&... args) {
    char buffer [200];
    snprintf(buffer, 200, "%s", std::forward<Args>(args)...);
    printf("ankiswitchboardd: \033[44;37m%s\033[0m\n", buffer);
  }
};

#endif /* log_h */
