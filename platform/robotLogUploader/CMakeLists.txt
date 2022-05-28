#
# platform/robotLogUploader/CMakeLists.txt
#
# cmake directives for robotLogUploader
#

project(robotLogUploader)

include(anki_build_cxx)

anki_build_cxx_library(robotLogUploader ${ANKI_SRCLIST_DIR})
anki_build_target_license(robotLogUploader "ANKI")

target_compile_options(robotLogUploader
  PRIVATE
  -Wconversion
  -Wsign-conversion
  -Wshorten-64-to-32
  -Wsign-compare
)

target_include_directories(robotLogUploader
  PUBLIC
  ${CMAKE_SOURCE_DIR}
)

target_link_libraries(robotLogUploader
  PRIVATE
  cloud_clad_cpp
  cti_messaging
  cti_common
  util
)

anki_build_strip(TARGET robotLogUploader)
