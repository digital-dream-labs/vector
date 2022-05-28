#
# platform/vic-log-forward/CMakeLists.txt
#
# Standalone application to forward DAS events from text file to log system
#

project(vic-log-forward)

include(anki_build_cxx)

if (VICOS)

  anki_build_cxx_executable(vic-log-forward ${ANKI_SRCLIST_DIR})

  anki_build_target_license(vic-log-forward "ANKI")

  target_include_directories(vic-log-forward
    PRIVATE
    ${CMAKE_SOURCE_DIR}
  )

  target_link_libraries(vic-log-forward
    PRIVATE
    util
  )

  anki_build_strip(TARGET vic-log-forward)

endif(VICOS)
