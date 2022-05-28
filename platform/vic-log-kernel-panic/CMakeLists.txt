#
# platform/vic-log-kernel-panic/CMakeLists.txt
#
# Standalone application to generate a dummy crash report with
# dmesg attachment.
#

project(vic-log-kernel-panic)

include(anki_build_cxx)

if (VICOS)

  anki_build_cxx_executable(vic-log-kernel-panic ${ANKI_SRCLIST_DIR})

  target_link_libraries(vic-log-kernel-panic
    PRIVATE
    victorCrashReports
    util
  )

  anki_build_strip(TARGET vic-log-kernel-panic)

  anki_build_target_license(vic-log-kernel-panic "ANKI")

endif(VICOS)
