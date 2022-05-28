#
# platform/anki-trace/CMakeLists.txt
#
include (anki_build_cxx)

if(USE_ANKITRACE AND VICOS)

  anki_build_cxx_library(ankitrace ${ANKI_SRCLIST_DIR})

  target_include_directories(ankitrace
    PUBLIC
    $<BUILD_INTERFACE:${LTTNGUST_INCLUDE_DIRS}>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>
    )

  target_link_libraries(ankitrace
    LINK_PUBLIC
    dl
    ${LTTNGUST_LIBRARIES}
    )

  anki_build_strip(TARGET ankitrace)

endif()
