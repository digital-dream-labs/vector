/**
 * File: log.cpp
 *
 * Author: seichert
 * Created: 1/10/2018
 *
 * Description: log functions for Anki Bluetooth Daemon
 *
 * Copyright: Anki, Inc. 2018
 *
 **/

#include "log.h"
#include <stdio.h>
#include <string.h>

#define LOG_BUF_SIZE 1024

static char sAndroidLoggingTag[64] = {'\0'};
static int sMinLogLevel = AnkiCameraLogLevelError;

#ifdef USE_ANDROID_LOGGING
#include <android/log.h>
static bool sUsingAndroidLogging = true;
#else
static bool sUsingAndroidLogging = false;
int __android_log_write(int prio, const char *tag, const char *msg) {
  int r = 0;  
  if (prio >= sMinLogLevel) {
    r = fprintf(stdout, "%s\n", msg);
  }
  return r;
}
#endif

bool isUsingAndroidLogging() {
  return sUsingAndroidLogging;
}

void enableAndroidLogging(const bool enable) {
  sUsingAndroidLogging = enable;
}

void setAndroidLoggingTag(const char* tag) {
  strncpy(sAndroidLoggingTag, tag, sizeof(sAndroidLoggingTag));
}

int getMinLogLevel() {
  return sMinLogLevel;
}

void setMinLogLevel(const int level) {
  sMinLogLevel = level;
}

void __log_write(int prio, const char *msg) {
  if (prio >= sMinLogLevel) {
    if (!sAndroidLoggingTag[0]) {
      setAndroidLoggingTag("anki-camera");
    }
    __android_log_write(prio, sAndroidLoggingTag, msg);
  }
}

void __log_vprint(int prio, const char *fmt, va_list ap) {
  if (prio >= sMinLogLevel) {
    char buf[LOG_BUF_SIZE];

    vsnprintf(buf, sizeof(buf), fmt, ap);

    __log_write(prio, buf);
  }
}

void logv(const char* fmt, ...) {
  va_list ap;

  va_start(ap, fmt);
  __log_vprint(AnkiCameraLogLevelVerbose, fmt, ap);
  va_end(ap);
}

void logd(const char* fmt, ...) {
  va_list ap;

  va_start(ap, fmt);
  __log_vprint(AnkiCameraLogLevelDebug, fmt, ap);
  va_end(ap);
}

void logi(const char* fmt, ...) {
  va_list ap;

  va_start(ap, fmt);
  __log_vprint(AnkiCameraLogLevelInfo, fmt, ap);
  va_end(ap);
}

void logw(const char* fmt, ...) {
  va_list ap;

  va_start(ap, fmt);
  __log_vprint(AnkiCameraLogLevelWarn, fmt, ap);
  va_end(ap);

}

void loge(const char* fmt, ...) {
  va_list ap;

  va_start(ap, fmt);
  __log_vprint(AnkiCameraLogLevelError, fmt, ap);
  va_end(ap);
}

