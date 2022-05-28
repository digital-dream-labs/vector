#
# platform/vic-log-upload/CMakeLists.txt
#
# Standalone application to upload robot log files
#

project(vic-log-upload)

include(anki_build_cxx)

if (VICOS)

  anki_build_cxx_executable(vic-log-upload ${ANKI_SRCLIST_DIR})
  anki_build_target_license(vic-log-upload "ANKI")

  target_compile_options(vic-log-upload
    PRIVATE
    -Wconversion
    -Wsign-conversion
    -Wshorten-64-to-32
    -Wsign-compare
  )

  target_include_directories(vic-log-upload
    PRIVATE
    ${CMAKE_SOURCE_DIR}
  )

  target_link_libraries(vic-log-upload
    PRIVATE
    robotLogUploader
    victorCrashReports
    jsoncpp
    util
  )

  anki_build_strip(TARGET vic-log-upload)

endif(VICOS)
