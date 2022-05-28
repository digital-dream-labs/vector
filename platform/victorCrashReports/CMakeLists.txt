cmake_minimum_required(VERSION 3.6)

project(victorCrashReports)

#
# Build options
#
option(USE_GOOGLE_BREAKPAD "Enable Google Breakpad crash reports" ON)
option(USE_TOMBSTONE_HOOKS "Enable Android tombstone hooks" ON)

message(STATUS "USE_GOOGLE_BREAKPAD is ${USE_GOOGLE_BREAKPAD}")
message(STATUS "USE_TOMBSTONE_HOOKS is ${USE_TOMBSTONE_HOOKS}")

include(breakpad)
include(anki_build_cxx)
include(anki_build_strip)

if (VICOS)

  anki_build_cxx_library(victorCrashReports ${ANKI_SRCLIST_DIR} STATIC)
  anki_build_target_license(victorCrashReports "ANKI")

  target_include_directories(victorCrashReports
    PRIVATE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/lib/util/source/anki>
  )

  if (USE_GOOGLE_BREAKPAD)
    target_compile_definitions(victorCrashReports
      PRIVATE
      USE_GOOGLE_BREAKPAD
    )
    target_include_directories(victorCrashReports
      PRIVATE
      $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/lib/crash-reporting-vicos/Breakpad/include/breakpad>
    )
    target_link_libraries(victorCrashReports
      PRIVATE
      util
      breakpad_client
    )
  endif()

  if (USE_TOMBSTONE_HOOKS)
    target_compile_definitions(victorCrashReports
      PRIVATE
      USE_TOMBSTONE_HOOKS
    )
    target_link_libraries(victorCrashReports
      PRIVATE
      util
      cutils
    )
  endif()

  anki_build_strip(TARGET victorCrashReports)

endif()
