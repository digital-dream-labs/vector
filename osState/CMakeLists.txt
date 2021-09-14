cmake_minimum_required(VERSION 3.6)

project(osState)

include(anki_build_cxx)

anki_build_cxx_library(osState ${ANKI_SRCLIST_DIR})
anki_build_target_license(osState "ANKI")

set(PLATFORM_LIBS "")
set(PLATFORM_COMPILE_DEFS "")
if (VICOS)
    set(PLATFORM_LIBS
        log
        libcutils.so.0)
elseif (MACOSX)
    include(webots)
    set(PLATFORM_LIBS
        ${WEBOTS_LIBS}
    )
    set(PLATFORM_COMPILE_DEFS "-DSIMULATOR")
endif()

target_link_libraries(osState
PRIVATE
  util
  cti_common
# platform
  ${PLATFORM_LIBS}
  ${ASAN_LINKER_FLAGS}
)

anki_build_strip(TARGET osState)

target_compile_definitions(osState
  PUBLIC
  ${PLATFORM_COMPILE_DEFS}
)

target_include_directories(osState
  PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
  PRIVATE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../robot/include>
)

