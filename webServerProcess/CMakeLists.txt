cmake_minimum_required(VERSION 3.6)

project(webServerProcess)

# Build options
option(USE_DAS "Enable DAS reporting" OFF)

include(anki_build_cxx)

#
# victor_web_library
#

anki_build_cxx_library(victor_web_library ${ANKI_SRCLIST_DIR})
anki_build_target_license(victor_web_library "ANKI")

set(PLATFORM_LIBS "")
set(PLATFORM_INCLUDES ${OPENCV_INCLUDE_PATHS})
set(PLATFORM_COMPILE_DEFS "")

if (VICOS)
    set(PLATFORM_LIBS
        log
    )
elseif (MACOSX)
    include(webots)
    set(PLATFORM_LIBS
        ${WEBOTS_LIBS}
    )
    set(PLATFORM_COMPILE_DEFS "-DSIMULATOR")
endif()

target_link_libraries(victor_web_library
  PRIVATE
  cti_vision
  DAS
  osState
  PUBLIC
  # vendor
  civetweb
  robot_interface  # Needs to be public for cozmoConfig.h
)

anki_build_strip(TARGET victor_web_library)

target_compile_definitions(victor_web_library
  PRIVATE
  USE_DAS=$<BOOL:${USE_DAS}>
  ${PLATFORM_COMPILE_DEFS}
)

target_include_directories(victor_web_library
  PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src> # allow "android/" prefix
  ${PLATFORM_INCLUDES}
)

# victor_web_server binary only builds on VICOS now
# mac implementation uses (webotsCtrlWebServer)
if (VICOS AND NOT ANKI_NO_WEBSERVER_ENABLED)

  anki_build_cxx_executable(vic-webserver ${ANKI_SRCLIST_DIR})
  anki_build_target_license(vic-webserver "ANKI")

  target_link_libraries(vic-webserver
    PRIVATE
    victor_web_library
    cti_common
    victorCrashReports
    ${PLATFORM_LIBS}
    ${ASAN_EXE_LINKER_FLAGS}
  )

  anki_build_strip(TARGET vic-webserver)

endif()
