cmake_minimum_required(VERSION 3.6)

if (VICOS)

project(vic-switchboard)

include(anki_build_cxx)
include(sodium)

anki_build_cxx_library(switchboard ${ANKI_SRCLIST_DIR} STATIC)
anki_build_target_license(switchboard "ANKI" "CC0,${CMAKE_SOURCE_DIR}/licenses/simple-signal.license")

set(PLATFORM_LIBS "")
set(PLATFORM_INCLUDES "")
set(PLATFORM_COMPILE_DEFS "")

set(PLATFORM_LIBS
  log
  c
)

target_link_libraries(switchboard
PRIVATE
  ${PLATFORM_LIBS}
  sdk_clad_cpp
  robot_clad
  cti_messaging_shared
  engine_clad
  ${SODIUM_LIBS}
  jsoncpp
  libglib-2.0.so
  libgio-2.0.so
  libgobject-2.0.so
  libffi.so.6
  libdl.so.2
  libz.so.1
  libresolv.so.2
  libgmodule-2.0.so.0
  libpcre.so.1
  libcutils.so.0
  util
  victorCrashReports
  anki-ble
  osState
)

target_compile_options(switchboard
  PRIVATE
  ${ANKI_BUILD_CXX_COMPILE_OPTIONS}
)

target_compile_definitions(switchboard
  PRIVATE
  ${PLATFORM_COMPILE_DEFS}
  ANKI_SWITCHBOARD_CLOUD_AUTH=1
)

target_include_directories(switchboard
  PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/switchboardd>
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/bleClient>
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/cutils>
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/anki-wifi>
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/signals>
  $<BUILD_INTERFACE:${LIBSODIUM_INCLUDE_PATH}>
  $<BUILD_INTERFACE:${VICOS_SDK}/sysroot/usr/include/gio-unix-2.0>
  $<BUILD_INTERFACE:${VICOS_SDK}/sysroot/usr/include/glib-2.0>
  $<BUILD_INTERFACE:${VICOS_SDK}/sysroot/usr/lib/glib-2.0/include>
  ${PLATFORM_INCLUDES}
)

anki_build_cxx_executable(vic-switchboard ${ANKI_SRCLIST_DIR})
anki_build_target_license(vic-switchboard "ANKI")

target_compile_options(vic-switchboard
  PRIVATE
  ${ANKI_BUILD_CXX_COMPILE_OPTIONS}
)

target_link_libraries(vic-switchboard
  PRIVATE
  switchboard
  ${PLATFORM_LIBS}
  sdk_clad_cpp
  robot_clad
  cti_messaging_shared
  engine_clad
  ${SODIUM_LIBS}
  jsoncpp
  libglib-2.0.so
  libgio-2.0.so
  libgobject-2.0.so
  libffi.so.6
  libdl.so.2
  libz.so.1
  libresolv.so.2
  libgmodule-2.0.so.0
  libpcre.so.1
  libcutils.so.0
  util
  victorCrashReports
  anki-ble
)

anki_build_strip(TARGET vic-switchboard)

anki_build_cxx_executable(vic-rescue ${ANKI_SRCLIST_DIR})
anki_build_target_license(vic-rescue "ANKI")

include(robot_core)

target_compile_options(vic-rescue
  PRIVATE
  ${ANKI_BUILD_CXX_COMPILE_OPTIONS}
)

target_link_libraries(vic-rescue
  PRIVATE
  switchboard
  robot_interface
  robot_core
  cti_common
  cti_vision 
  ${PLATFORM_LIBS}
  sdk_clad_cpp
  robot_clad
  cti_messaging_shared
  engine_clad
  ${SODIUM_LIBS}
  jsoncpp
  libglib-2.0.so
  libgio-2.0.so
  libgobject-2.0.so
  libffi.so.6
  libdl.so.2
  libz.so.1
  libresolv.so.2
  libgmodule-2.0.so.0
  libpcre.so.1
  libcutils.so.0
  util
  victorCrashReports
  anki-ble
)

anki_build_strip(TARGET vic-rescue)

endif(VICOS)
