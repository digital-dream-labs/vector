cmake_minimum_required(VERSION 3.6)

project(cubeBleClient)

include(anki_build_cxx)

anki_build_cxx_library(cubeBleClient ${ANKI_SRCLIST_DIR})
anki_build_target_license(cubeBleClient "ANKI")

set(PLATFORM_LIBS "")
set(PLATFORM_INCLUDES "")
set(PLATFORM_COMPILE_DEFS "")
if (VICOS)
    set(PLATFORM_LIBS
        anki-ble
        log
    )
elseif (MACOSX)
    include(webots)
    set(PLATFORM_LIBS
        ${WEBOTS_LIBS}
    )
    set(PLATFORM_COMPILE_DEFS "-DSIMULATOR")
endif()

target_link_libraries(cubeBleClient
PRIVATE
  util
  clad
  cti_common
  engine_clad
  libev
# platform
  ${PLATFORM_LIBS}
  ${ASAN_LINKER_FLAGS}
)

anki_build_strip(TARGET cubeBleClient)

target_compile_definitions(cubeBleClient
  PRIVATE
  ${PLATFORM_COMPILE_DEFS}
)

target_include_directories(cubeBleClient
  PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
  PRIVATE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../robot/include>
  ${PLATFORM_INCLUDES}
)

