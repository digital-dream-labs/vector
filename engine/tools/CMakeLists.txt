cmake_minimum_required(VERSION 3.6)

project(engine_tools)

include(anki_build_cxx)
include(anki_build_strip)

set(PLATFORM_LIBS "")
set(PLATFORM_INCLUDES "")
set(PLATFORM_FLAGS "")

if (VICOS)
  set(PLATFORM_LIBS log)
elseif (MACOSX)
  find_library(FOUNDATION Foundation)
  set(PLATFORM_LIBS
    ${FOUNDATION}
  )
endif()

anki_build_cxx_executable(vic-engine ${ANKI_SRCLIST_DIR})
anki_build_target_license(vic-engine "ANKI")

target_link_libraries(vic-engine
  PRIVATE
  # anki libs
  util
  cozmo_engine
  victorCrashReports
  # platform
  ${PLATFORM_LIBS}
  ${ASAN_EXE_LINKER_FLAGS}
)

anki_build_strip(TARGET vic-engine)
