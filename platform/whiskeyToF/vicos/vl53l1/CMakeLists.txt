cmake_minimum_required(VERSION 3.6)

project(vl53l1)

if (VICOS)

anki_build_cxx_library(vl53l1 ${ANKI_SRCLIST_DIR} STATIC)

target_include_directories(vl53l1
  SYSTEM PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/core/inc>
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/platform/inc>
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/protected/inc>
)

set_target_properties(vl53l1 PROPERTIES LINKER_LANGUAGE C)

anki_build_strip(TARGET vl53l1)

# VIC-13448 Figure out license(s) for vl53l1 code
# anki_build_target_license(vl53l1 "")

endif()
