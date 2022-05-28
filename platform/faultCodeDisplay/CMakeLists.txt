cmake_minimum_required(VERSION 3.6)

project(vic-faultCodeDisplay)

include(anki_build_cxx)
include(robot_core)

if (VICOS)

  anki_build_cxx_executable(vic-faultCodeDisplay ${ANKI_SRCLIST_DIR})

  target_link_libraries(vic-faultCodeDisplay
    PUBLIC
    robot_interface
    robot_core
    cti_common
    cti_vision
    ${ANKITRACE}
    ${ASAN_EXE_LINKER_FLAGS}
  )

  anki_build_strip(TARGET vic-faultCodeDisplay)

endif(VICOS)
