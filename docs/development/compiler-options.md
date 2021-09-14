# RELEASE and DEBUG macros

`_DEBUG` is Microsoft compiler specific, but needs to be retained as does `DEBUG` due to use in third-party software (acapela, opencv)

release build, is the shipping build:
  - #undefines `_DEBUG`, `DEBUG`
  - #defines `NDEBUG`
  - -O2
  - no developer code
  - console vars are enabled
  - PII is stripped
  - webservice enabled and password protected

debug build, everything else:
  - #defines `_DEBUG` and `DEBUG`
  - #undefines `NDEBUG`
  - -O0
  - developer code
  - console vars
  - PII is enabled
  - webservice enabled no password required

# Compiler Options

```
-c
-fdata-sections
-fdiagnostics-absolute-paths
-fdiagnostics-show-category=name
-fexceptions                                                           # Android only
-ffunction-sections                                                    # Android only
-fno-exceptions                                                        # Android only
-fno-integrated-as                                                     # Android only
-fno-limit-debug-info                                                  # Android only DEBUG
-fno-rtti                                                              # Android only
-fno-short-enums                                                       # Android only
-fPIC
-fPIE                                                                  # Android only
-frtti                                                                 # Android only
-fsigned-char
-fstack-protector-all                                                  # DEBUG
-fstack-protector-strong                                               # Android only
-funwind-tables                                                        # Android only
-fvisibility=hidden                                                    # Android only
-g
-gsplit-dwarf                                                          # Android only
-march=armv7-a                                                         # Android only
-mfloat-abi=softfp                                                     # Android only
-mfpu=neon                                                             # Android only
-mfpu=vfpv3-d16                                                        # Android only
-mthumb                                                                # Android only
-no-canonical-prefixes                                                 # Android only
-o
-O0                                                                    # DEBUG
-O2                                                                    # RELEASE
-Os
-std=c++11
-std=c++14
-std=c11                                                               # Android only
-std=gnu99
-stdlib=libc++
-Wa,--noexecstack
-fobjc-arc                                                             # Mac only
```

## Warnings

```
-Wall
-Wconditional-uninitialized
-Werror
-Werror=format-security
-Wformat
-Wformat-security
-Wheader-guard
-Winit-self
-Wno-deprecated-declarations
-Wno-enum-conversion                                                   # Android only
-Wno-error                                                             # Android only
-Wno-implicit-function-declaration                                     # Android only
-Wno-int-conversion                                                    # Android only
-Wno-logical-not-parentheses                                           # Android only
-Wno-shorten-64-to-32
-Wno-tautological-constant-out-of-range-compare                        # Android only
-Wno-undef
-Wno-unguarded-availability                                            # Mac only
-Wno-unused-command-line-argument
-Woverloaded-virtual
-Wshorten-64-to-32
-Wundef
```

## Defines

```
-D_DEBUG                                                               # FIX: debug builds only
-D_POSIX_SOURCE                                                        # Android only
-DAE_AK_ALSA_SINK                                                      # Android only
-DAE_AK_COMPRESSOR_FX
-DAE_AK_DELAY_FX
-DAE_AK_EXPANDER_FX
-DAE_AK_FLANGER_FX
-DAE_AK_GAIN_FX
-DAE_AK_HARMONIZER_FX
-DAE_AK_METER_FX
-DAE_AK_PARAMETRIC_EQ_FX
-DAE_AK_PEAK_LIMITER_FX
-DAE_AK_PITCH_SHIFTER_FX
-DAE_AK_SILENCE_SOURCE
-DAE_AK_SINE_GENERATOR_SOURCE
-DAE_AK_TIME_STRETCH_FX
-DAE_AK_TONE_GENERATOR_SOURCE
-DAE_AK_TREMOLO_FX
-DAK_LINUX                                                             # Android only
-DANDROID                                                              # Android only
-DANKICORETECH_EMBEDDED_USE_MATLAB=0
-DANKICORETECH_EMBEDDED_USE_OPENCV=1
-DANKICORETECH_USE_MATLAB=0
-DANKICORETECH_USE_OPENCV=1
-DCAN_STREAM=false
-DCORETECH_ENGINE
-DCORETECH_ENGINE=1                                                    # Mac only, duplicated -DCORETECH_ENGINE
-DCORETECH_ROBOT
-DCOZMO_ROBOT
-DDEBUG                                                                # CHECK: debug builds only
-DDEVELOPMENT                                                          # Android only, missing in Mac build, CHECK: remove
-DDISABLE_JNI=1                                                        # Android only
-DFACE_TRACKER_FACESDK=1
-DFACE_TRACKER_FACIOMETRIC=0
-DFACE_TRACKER_OKAO=3
-DFACE_TRACKER_OPENCV=2
-DFACE_TRACKER_PROVIDER=FACE_TRACKER_OKAO
-DFACTORY_TEST=0
-DFACTORY_TEST_DEV=0
-DHEADER_NAME=\\\"proctest_go.h\\\"                                    # Mac only
-DJSONCPP_USING_SECURE_MEMORY=0
-DMAX_REQUEST_SIZE=16384
-DNDEBUG                                                               # CHECK: release builds only
-DMACOS                                                                # Mac only
-DNO_LOCALE_SUPPORT=1
-DNO_SSL
-DPIC                                                                  # Android only
-DSIMULATOR                                                            # Mac only
-DTEST_DATA_PATH=/Users/richard/projects/victor/coretech/common/..     # Mac only
-DTEST_DATA_PATH=/Users/richard/projects/victor/coretech/planning/..   # Mac only
-DUSE_ANDROID_LOGGING=1                                                # Android only
-DUSE_DAS=0
-DUSE_ION                                                              # Android only
-DUNIT_TEST=1                                                          # Mac only
-DUSE_STACK_SIZE=102400
-DUSE_WEBSOCKET
-DVICOS                                                                # Android only
-UANDROID                                                              # Android only, CHECK: remove
-UNDEBUG                                                               # Android only, CHECK: remove
```

```
-Dasound_EXPORTS                                                       # Android only
-Daudio_engine_EXPORTS
-Dble_cozmo_EXPORTS
-Dc_library_EXPORTS
-DcameraService_EXPORTS
-DCIVETWEB_DLL_EXPORTS
-DCIVETWEB_DLL_IMPORTS
-Dcozmo_engine_EXPORTS
-DcubeBleClient_EXPORTS
-DDAS_EXPORTS
-Dipctest_go_fake_dep_EXPORTS
-DosState_EXPORTS
-Dproctest_go_fake_dep_EXPORTS                                         # Mac only
-Dservbench_fake_dep_EXPORTS
-Dutil_audio_EXPORTS
-Dutil_EXPORTS
-Dvic_cloud_fake_dep_EXPORTS
-Dvictor_web_library_EXPORTS
-Dvoicego_fake_dep_EXPORTS                                             # Mac only
-Dwavtest_fake_dep_EXPORTS
-Dwebots_plugin_physics_EXPORTS                                        # Mac only
-F/Users/richard/projects/victor/lib/util/cmake/../libs/framework      # Mac only
```

## Includes

```
-I../../../animProcess/src
-I../../../coretech/planning/basestation/src                           # Mac only
-I../../../coretech/common/../..                                       # duplicates -I../../..
-I../../../coretech/messaging/../..                                    # duplicates -I../../..
-I../../../coretech/planning/../..                                     # duplicates -I../../..
-I../../../coretech/vision/../..                                       # duplicates -I../../..
-I../../../cubeBleClient
-I../../../cubeBleClient/..                                            # duplicates -I../../..
-I../../../cubeBleClient/../generated/clad/engine                      # duplicates -I../../../generated/clad/engine
-I../../../cubeBleClient/../robot/Includes                             # duplicates -I../../../robot/include
-I../../../engine
-I../../../engine/..                                                   # duplicates -I../../..
-I../../../EXTERNALS/anki-thirdparty/signalEssence/android/project/anki_victor
-I../../../EXTERNALS/anki-thirdparty/signalEssence/android/project/anki_victor_vad
-I../../../EXTERNALS/anki-thirdparty/signalEssence/android/se_lib_public
-I../../../EXTERNALS/anki-thirdparty/signalEssence/android/se_lib_public/cpu_arm
-I../../../EXTERNALS/coretech_external/build/opencv-3.4.0
-I../../../EXTERNALS/coretech_external/build/opencv-3.4.0/android
-I../../../EXTERNALS/coretech_external/build/opencv-3.4.0/android/sdk/native/jni/include
-I../../../EXTERNALS/coretech_external/opencv-3.4.0/modules/calib3d/include
-I../../../EXTERNALS/coretech_external/opencv-3.4.0/modules/core/include
-I../../../EXTERNALS/coretech_external/opencv-3.4.0/modules/dnn/include
-I../../../EXTERNALS/coretech_external/opencv-3.4.0/modules/features2d/include
-I../../../EXTERNALS/coretech_external/opencv-3.4.0/modules/flann/include
-I../../../EXTERNALS/coretech_external/opencv-3.4.0/modules/highgui/include
-I../../../EXTERNALS/coretech_external/opencv-3.4.0/modules/imgcodecs/include
-I../../../EXTERNALS/coretech_external/opencv-3.4.0/modules/imgproc/include
-I../../../EXTERNALS/coretech_external/opencv-3.4.0/modules/ml/include
-I../../../EXTERNALS/coretech_external/opencv-3.4.0/modules/objdetect/include
-I../../../EXTERNALS/coretech_external/opencv-3.4.0/modules/video/include
-I../../../EXTERNALS/coretech_external/opencv-3.4.0/modules/videoio/include
-I../../../generated/clad
-I../../../generated/clad/engine
-I../../../generated/clad/robot
-I../../../generated/clad/robot/common
-I../../../generated/clad/util
-I../../../generated/coretech/common
-I../../../generated/coretech/vision
-I../../../include
-I../../../include/anki/cozmo
-I../../../lib/audio/../../generated/clad/engine                       # duplicates -I../../../generated/clad/engine
-I../../../lib/audio/../../generated/clad/robot                        # duplicates -I../../../generated/clad/robot
-I../../../lib/audio/include
-I../../../lib/audio/plugins/akAlsaSink/common/include
-I../../../lib/audio/plugins/akAlsaSink/common/src
-I../../../lib/audio/plugins/akAlsaSink/pluginInclude
-I../../../lib/audio/plugins/akAlsaSink/soundEnginePlugin
-I../../../lib/audio/plugins/alsa/include
-I../../../lib/audio/plugins/hijackAudio/pluginInclude
-I../../../lib/audio/plugins/hijackAudio/pluginSource
-I../../../lib/audio/plugins/wavePortal/pluginInclude
-I../../../lib/audio/plugins/wavePortal/pluginSource
-I../../../lib/audio/source
-I../../../lib/audio/wwise/versions/current/include
-I../../../lib/audio/zipreader
-I../../../lib/BLECozmo
-I../../../lib/BLECozmo/shared
-I../../../lib/das-client/android/DASNativeLib/jni
-I../../../lib/das-client/include
-I../../../lib/das-client/include/DAS
-I../../../lib/das-client/src
-I../../../lib/util/source/3rd/civetweb/include
-I../../../lib/util/source/3rd/jsoncpp
-I../../../lib/util/source/3rd/kazmath/..
-I../../../lib/util/source/3rd/kazmath/src
-I../../../lib/util/source/anki/audioUtil/..                           # duplicates -I../../../lib/util/source/anki
-I../../../lib/util/source/anki/util/..                                # duplicates -I../../../lib/util/source/anki
-I../../../osState
-I../../../osState/..                                                  # duplicates -I../../..
-I../../../osState/../robot/include                                    # duplicates -I../../../robot/include
-I../../../platform/camera
-I../../../platform/camera/..
-I../../../platform/camera/SYSTEM
-I../../../platform/camera/vicos/camera_client/inc
-I../../../robot
-I../../../robot/clad/src
-I../../../robot/core/inc
-I../../../robot/hal
-I../../../robot/hal/include
-I../../../robot/hal/sim/include
-I../../../robot/include
-I../../../robot/supervisor/src
-I../../../robot/syscon
-I../../../victor-clad/tools/message-buffers/support/cpp/include
-I../../../webServerProcess/src
```

# -DNDEBUG, -D_DEBUG, -DDEBUG, -DSHIPPING, -DRELEASE, -DDEVELOPMENT usage

## -DNDEBUG

/Users/richard/projects/victor/coretech/common/engine/math/point_impl.h:

assert() and everywhere else assert() is used

/Users/richard/projects/victor/coretech/common/engine/math/fastPolygon2d.cpp
/Users/richard/projects/victor/coretech/planning/engine/xythetaPlanner.cpp
/Users/richard/projects/victor/EXTERNALS/anki-thirdparty/signalEssence
/Users/richard/projects/victor/EXTERNALS/coretech_external/build/opencv-3.4.0
/Users/richard/projects/victor/lib/audio
/Users/richard/projects/victor/lib/crash-reporting-android/Breakpad
/Users/richard/projects/victor/lib/das-client
/Users/richard/projects/victor/lib/util/libs/framework/gtest-linux
/Users/richard/projects/victor/lib/util/source/3rd/civetweb
/Users/richard/projects/victor/lib/util/source/3rd/lua
/Users/richard/projects/victor/lib/util/source/anki/util
/Users/richard/projects/victor/resources/externals/anki-thirdparty/signalEssence
/Users/richard/projects/victor/resources/externals/coretech_external/build/opencv-3.4.0
/Users/richard/projects/victor/robot/hal

## -D_DEBUG

/Users/richard/projects/victor/coretech/common/robot/config.h:
```
#ifdef _DEBUG
#define ANKI_DEBUG_LEVEL ANKI_DEBUG_ERRORS_AND_WARNS_AND_ASSERTS
#else
#define ANKI_DEBUG_LEVEL ANKI_DEBUG_ERRORS
#endif
```

/Users/richard/projects/victor/robot/include/anki/common/robot/config.h:
```
#ifdef _DEBUG
#define ANKI_DEBUG_LEVEL ANKI_DEBUG_ERRORS_AND_WARNS_AND_ASSERTS
#else
#define ANKI_DEBUG_LEVEL ANKI_DEBUG_ERRORS
#endif
```

/Users/richard/projects/victor/EXTERNALS/anki-thirdparty/acapela/
/Users/richard/projects/victor/EXTERNALS/coretech_external/build/opencv-*
/Users/richard/projects/victor/lib/audio

## -DDEBUG

### poseTreeNode.h/.cpp
```
#if defined(DEBUG)
#  define DO_DEV_POSE_CHECKS     1
#elif defined(SHIPPING)
#  define DO_DEV_POSE_CHECKS     0
#else // Release
#  define DO_DEV_POSE_CHECKS     1
#endif
```

DO_DEV_POSE_CHECKS which shows up on profiler: Dev_AssertIsValidParentPointer

### /Users/richard/projects/victor/coretech/common/robot/config.h
```
#if defined(_DEBUG) || defined(DEBUG)
#define ANKI_DEBUG_LEVEL ANKI_DEBUG_ERRORS_AND_WARNS_AND_ASSERTS
#else
#define ANKI_DEBUG_LEVEL ANKI_DEBUG_ERRORS
#endif
```

### cozmoEngine.cpp

Anki::Util::sEvent

### perfmetric.cpp
```
  sprintf(_lineBuffer, "Summary:  (%s build; %s; %s; %s; %i engine ticks; %.3f seconds total)",
#if defined(DEBUG)
                "DEBUG"
#elif defined(RELEASE)
                "RELEASE"
#elif defined(PROFILE)
                "PROFILE"
#elif defined(SHIPPING)
                "SHIPPING"
#else
                "UNKNOWN"
#endif
```

## -DSHIPPING

### poseTreeNode.h/.cpp

#if defined(DEBUG)
#  define DO_DEV_POSE_CHECKS     1
#elif defined(SHIPPING)
#  define DO_DEV_POSE_CHECKS     0
#else // Release
#  define DO_DEV_POSE_CHECKS     1
#endif

DO_DEV_POSE_CHECKS which shows up on profiler: Dev_AssertIsValidParentPointer

### cozmoEngine.cpp

Anki::Util::sEvent

### perfmetric.cpp
```
  sprintf(_lineBuffer, "Summary:  (%s build; %s; %s; %s; %i engine ticks; %.3f seconds total)",
#if defined(DEBUG)
                "DEBUG"
#elif defined(RELEASE)
                "RELEASE"
#elif defined(PROFILE)
                "PROFILE"
#elif defined(SHIPPING)
                "SHIPPING"
#else
                "UNKNOWN"
#endif
```

### consoleMacro
```
#ifndef REMOTE_CONSOLE_ENABLED
  #if defined(SHIPPING)
    #define REMOTE_CONSOLE_ENABLED 0
  #else
    #define REMOTE_CONSOLE_ENABLED 1
  #endif
#endif
```

### globalDefinitions
```
#if defined(DEBUG)
  #define ANKI_DEVELOPER_CODE     1
  #define ANKI_DEV_CHEATS         1
  #define ANKI_PROFILING_ENABLED  1
  #define ANKI_PRIVACY_GUARD      0 // PII displayed in debug logs!!!
#elif defined(RELEASE)
  #define ANKI_DEVELOPER_CODE     0
  #define ANKI_DEV_CHEATS         1
  #define ANKI_PROFILING_ENABLED  1
  #define ANKI_PRIVACY_GUARD      0 // PII displayed in non-shipping release logs!!!
#elif defined(SHIPPING)
  #define ANKI_DEVELOPER_CODE     0
  #define ANKI_DEV_CHEATS         0
  #define ANKI_PROFILING_ENABLED  0
  #define ANKI_PRIVACY_GUARD      1 // PII redacted in shipping logs
#else
```

### numericCast.h
```
 * Description: numeric_cast<> for asserting that a value won't under/over-flow into the new type
 *              resolves to a constexpr static_cast<> in SHIPPING
```

comment only


### webService.cpp
```
#if defined(DEBUG)
  "DEBUG";
#elif defined(RELEASE)
  "RELEASE";
#elif defined(PROFILE)
  "PROFILE";
#elif defined(SHIPPING)
  "SHIPPING";
#else
  "UNKNOWN";
#endif

//#if defined(SHIPPING)
//    "global_auth_file",
//    passwordFile.c_str(),
//#endif
```

## -DRELEASE

/Users/richard/projects/victor/coretech/common/engine/math/point.h

comment only

cozmoEngine.cpp
```
Anki::Util::sEvent
```

perfmetric.cpp
```
  sprintf(_lineBuffer, "Summary:  (%s build; %s; %s; %s; %i engine ticks; %.3f seconds total)",
#if defined(DEBUG)
                "DEBUG"
#elif defined(RELEASE)
                "RELEASE"
#elif defined(PROFILE)
                "PROFILE"
#elif defined(SHIPPING)
                "SHIPPING"
#else
                "UNKNOWN"
#endif
```

/Users/richard/projects/victor/lib/util/source/anki/util/global/globalDefinitions.h
```
#if defined(DEBUG)
  #define ANKI_DEVELOPER_CODE     1
  #define ANKI_DEV_CHEATS         1
  #define ANKI_PROFILING_ENABLED  1
  #define ANKI_PRIVACY_GUARD      0 // PII displayed in debug logs!!!
#elif defined(RELEASE)
  #define ANKI_DEVELOPER_CODE     0
  #define ANKI_DEV_CHEATS         1
  #define ANKI_PROFILING_ENABLED  1
  #define ANKI_PRIVACY_GUARD      0 // PII displayed in non-shipping release logs!!!
#elif defined(SHIPPING)
  #define ANKI_DEVELOPER_CODE     0
  #define ANKI_DEV_CHEATS         0
  #define ANKI_PROFILING_ENABLED  0
  #define ANKI_PRIVACY_GUARD      1 // PII redacted in shipping logs
#else
  #error "You must define DEBUG, RELEASE, or SHIPPING"
#endif
```

/Users/richard/projects/victor/robot/include/anki/cozmo/robot/buildTypes.h:
```
#if defined(RELEASE)
#define ANKI_DEBUG_LEVEL  ANKI_DEBUG_ERRORS_AND_WARNS_AND_ASSERTS
#define ANKI_DEBUG_INFO   0
#define ANKI_DEBUG_EVENTS 1
#elif defined(SIMULATOR)
#define ANKI_DEBUG_LEVEL  ANKI_DEBUG_ALL
#define ANKI_DEBUG_INFO   1
#define ANKI_DEBUG_EVENTS 1
#elif defined(DEVELOPMENT) // Default is development build
#define ANKI_DEBUG_LEVEL  ANKI_DEBUG_ALL
#define ANKI_DEBUG_INFO   1
#define ANKI_DEBUG_EVENTS 1
#else
#error "Unsupported build type"
#endif
```

webService.cpp
```
#if defined(DEBUG)
  "DEBUG";
#elif defined(RELEASE)
  "RELEASE";
#elif defined(PROFILE)
  "PROFILE";
#elif defined(SHIPPING)
  "SHIPPING";
#else
  "UNKNOWN";
#endif

//#if defined(SHIPPING)
//    "global_auth_file",
//    passwordFile.c_str(),
//#endif
```

## -DDEVELOPMENT

/Users/richard/projects/victor/robot/include/anki/cozmo/robot/buildTypes.h
```
#if defined(RELEASE)
#define ANKI_DEBUG_LEVEL  ANKI_DEBUG_ERRORS_AND_WARNS_AND_ASSERTS
#define ANKI_DEBUG_INFO   0
#define ANKI_DEBUG_EVENTS 1
#elif defined(SIMULATOR)
#define ANKI_DEBUG_LEVEL  ANKI_DEBUG_ALL
#define ANKI_DEBUG_INFO   1
#define ANKI_DEBUG_EVENTS 1
#elif defined(DEVELOPMENT) // Default is development build
#define ANKI_DEBUG_LEVEL  ANKI_DEBUG_ALL
#define ANKI_DEBUG_INFO   1
#define ANKI_DEBUG_EVENTS 1
#else
#error "Unsupported build type"
#endif
```

## -DSIMULATOR

/Users/richard/projects/victor/robot/include/anki/cozmo/robot/buildTypes.h
```
#if defined(RELEASE)
#define ANKI_DEBUG_LEVEL  ANKI_DEBUG_ERRORS_AND_WARNS_AND_ASSERTS
#define ANKI_DEBUG_INFO   0
#define ANKI_DEBUG_EVENTS 1
#elif defined(SIMULATOR)
#define ANKI_DEBUG_LEVEL  ANKI_DEBUG_ALL
#define ANKI_DEBUG_INFO   1
#define ANKI_DEBUG_EVENTS 1
#elif defined(DEVELOPMENT) // Default is development build
#define ANKI_DEBUG_LEVEL  ANKI_DEBUG_ALL
#define ANKI_DEBUG_INFO   1
#define ANKI_DEBUG_EVENTS 1
#else
#error "Unsupported build type"
#endif
```
/everywhere else/
