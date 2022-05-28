#
# platform/vic-log-event/CMakeLists.txt
#
# Standalone application to log a DAS event
#

project(vic-log-event)

include(anki_build_cxx)

if (VICOS)

  anki_build_cxx_executable(vic-log-event ${ANKI_SRCLIST_DIR})
  anki_build_target_license(vic-log-event "ANKI")

  target_include_directories(vic-log-event
    PRIVATE
    ${CMAKE_SOURCE_DIR}
  )

  target_link_libraries(vic-log-event
    PRIVATE
    util
    ${ASAN_LINKER_FLAGS}
  )

  anki_build_strip(TARGET vic-log-event)

endif(VICOS)
