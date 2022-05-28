/**
 * File: log.h
 *
 * Author: seichert
 * Created: 1/10/2018
 *
 * Description: log functions for Anki Bluetooth Daemon
 *
 * Copyright: Anki, Inc. 2018
 *
 **/

#ifndef __mm_anki_camera_log_h__
#define __mm_anki_camera_log_h__

#include <stdarg.h>
#include <stdbool.h>

enum anki_camera_log_level {
    AnkiCameraLogLevelVerbose = 2,
    AnkiCameraLogLevelDebug   = 3,
    AnkiCameraLogLevelInfo    = 4,
    AnkiCameraLogLevelWarn    = 5,
    AnkiCameraLogLevelError   = 6,
    AnkiCameraLogLevelSilent  = 7,
    AnkiCameraLogLevelMax     = AnkiCameraLogLevelSilent,
};

bool isUsingAndroidLogging();
void enableAndroidLogging(const bool enable);
void setAndroidLoggingTag(const char* tag);
int getMinLogLevel();
void setMinLogLevel(const int level);

void logv(const char* fmt, ...) __attribute__((format(printf,1,2)));
void logd(const char* fmt, ...) __attribute__((format(printf,1,2)));
void logi(const char* fmt, ...) __attribute__((format(printf,1,2)));
void logw(const char* fmt, ...) __attribute__((format(printf,1,2)));
void loge(const char* fmt, ...) __attribute__((format(printf,1,2)));

#endif //__mm_anki_camera_log_h__


