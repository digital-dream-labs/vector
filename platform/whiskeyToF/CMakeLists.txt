cmake_minimum_required(VERSION 3.6)

project(whiskeyToF)

include(anki_build_cxx)

if (VICOS)
   add_subdirectory(vicos/vl53l1)
endif()


anki_build_cxx_library(whiskeyToF ${ANKI_SRCLIST_DIR})

set(PLATFORM_LIBS "")
set(PLATFORM_INCLUDES "")
set(PLATFORM_COMPILE_DEFS "")
if (VICOS)
    set(PLATFORM_LIBS
        vl53l1
        log
        gpio
    )
elseif (MACOSX)
    include(webots)
    set(PLATFORM_LIBS
        webotsCtrlCommon
        ${WEBOTS_LIBS}
    )
    set(PLATFORM_COMPILE_DEFS "-DSIMULATOR")
endif()

target_link_libraries(whiskeyToF
PRIVATE
  util
  cti_common
  engine_clad
  robot_interface # emrHelper.h
  ${PLATFORM_LIBS}
)

anki_build_strip(TARGET whiskeyToF)

target_compile_definitions(whiskeyToF
  PRIVATE
  ${PLATFORM_COMPILE_DEFS}
)

target_include_directories(whiskeyToF
  PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..> # allow "vicos/" prefix
  ${PLATFORM_INCLUDES}
)

anki_build_target_license(whiskeyToF "ANKI")


# Below are two different tof test programs, tof_test and multizone
# tof_test uses the whiskyToF library while multizone uses an ST provided test program
# Don't build by default
if (VICOS AND FALSE)

project(tof_test)

add_executable(tof_test
  ${CMAKE_SOURCE_DIR}/platform/whiskeyToF/tof_test.cpp)

target_include_directories(tof_test
  PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..> # allow "vicos/" prefix
  ${PLATFORM_INCLUDES}
)

target_link_libraries(tof_test
PRIVATE
  whiskeyToF
  util
  cti_common
  cti_vision
  engine_clad # imageTypes.h
  robot_interface # cozmoConfig.h
  ${PLATFORM_LIBS}
)

target_compile_definitions(tof_test
  PRIVATE
  ${PLATFORM_COMPILE_DEFS}
)

anki_build_target_license(tof_test "ANKI")


project(multizone)

add_executable(multizone
  ${CMAKE_SOURCE_DIR}/platform/whiskeyToF/vicos/vl53l1/example/src/vl53l1_MultiZones_Scanning_Example.c)

target_include_directories(multizone
  PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..> # allow "vicos/" prefix
  ${PLATFORM_INCLUDES}
)

target_link_libraries(multizone
PRIVATE
  vl53l1
  ${PLATFORM_LIBS}
)

target_compile_definitions(multizone
  PRIVATE
  ${PLATFORM_COMPILE_DEFS}
)

endif()
