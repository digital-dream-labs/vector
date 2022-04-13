cmake_minimum_required(VERSION 3.6)

project(engine)

# Build options
option(USE_DAS "Enable DAS reporting" OFF)
option(ALEXA_ACOUSTIC_TEST "Enable alexa acoustic testing (no movement mode)" OFF)

include(opencv)
include(okao)
include(flatbuffers)
include(protobuf)
include(anki_build_strip)
include(anki_build_cxx)

#
# cozmo_engine
#

# Generate c++ protobuf files
set(PROTOBUF_CPP_PLUGIN "protocCppPlugin")
set(PROTOBUF_CPP_PLUGIN_PATH "${CMAKE_SOURCE_DIR}/tools/protobuf/plugin/${PROTOBUF_CPP_PLUGIN}")
add_custom_command(
  OUTPUT ${GATEWAY_PROTO_CPP_GEN_HEADERS} ${GATEWAY_PROTO_CPP_GEN_SRCS}
  COMMAND mkdir -p ${PROTOBUF_OUTPUT_DIR}
  COMMAND ${PROTOBUF_HOME}/bin/protoc -I"${GATEWAY_PROTO_HOME_DIR}/public"
    -I"${PROTOBUF_HOME}/include"
    "--plugin=protoc-gen-${PROTOBUF_CPP_PLUGIN}=${PROTOBUF_CPP_PLUGIN_PATH}"
    "--cpp_out=${PROTOBUF_OUTPUT_DIR}"
    "--${PROTOBUF_CPP_PLUGIN}_out=${PROTOBUF_OUTPUT_DIR}"
    ${GATEWAY_PROTO_SRCS}
  DEPENDS "${GATEWAY_PROTO_SRCS}" "${PROTOBUF_CPP_PLUGIN_PATH}"
)
# NOTE: protobuf generated files must be linked directly to cozmo_engine
#       Otherwise, there is a problem comparing the default string, and it
#       generates incorrect serialized messages.

anki_build_cxx_library(cozmo_engine ${ANKI_SRCLIST_DIR} ${GATEWAY_PROTO_CPP_GEN_SRCS} ${GATEWAY_PROTO_CPP_GEN_HEADERS})
anki_build_target_license(cozmo_engine "ANKI")


set(PLATFORM_LIBS "")
set(PLATFORM_INCLUDES "")
set(PLATFORM_COMPILE_DEFS "")
set(PLATFORM_FLAGS "")
if (VICOS)
  set(PLATFORM_LIBS log)
elseif (MACOSX)
  find_library(FOUNDATION Foundation)
  include(webots)
  set(PLATFORM_LIBS
    ${FOUNDATION}
    ${OPENCV_LIBS}
    ${WEBOTS_LIBS}
  )
  set(PLATFORM_COMPILE_DEFS
    SIMULATOR
  )
endif()

target_link_libraries(cozmo_engine
  PUBLIC
  # anki libs
  clad
  cti_common
  cti_messaging
  robot_interface
  audio_engine
  audio_multiplexer_engine
  engine_clad
  DAS
  util
  PRIVATE
  cameraService
  whiskeyToF
  canned_anim_lib_engine
  victor_web_library
  # cti
  cti_vision
  cti_planning
  cti_neuralnets
  cubeBleClient
  osState
  micdata
  robotLogUploader
  # vendor
  ${PROTOBUF_LIBS}
  ${FLATBUFFERS_LIBS}
  ${LIBARCHIVE_LIBS}
  # system
  z
  # platform
  ${PLATFORM_LIBS}
  ${ANKITRACE}
  ${ASAN_SHARED_LINKER_FLAGS}
)

target_compile_options(cozmo_engine PRIVATE
  ${SYMBOL_VISIBILITY_FLAGS}
  "${PLATFORM_FLAGS}"
  ${ASAN_CXX_FLAGS}
)

anki_build_strip(TARGET cozmo_engine)

target_compile_definitions(cozmo_engine
  PUBLIC
  USE_DAS=$<BOOL:${USE_DAS}>
  ALEXA_ACOUSTIC_TEST=$<BOOL:${ALEXA_ACOUSTIC_TEST}>
  ANKI_DISABLE_ALEXA=$<BOOL:${ANKI_DISABLE_ALEXA}>
  PRIVATE
  CORETECH_ENGINE
  ${PLATFORM_COMPILE_DEFS}
)

target_include_directories(cozmo_engine
  PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..> # allows reference to engine/
  #PRIVATE
  $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/generated>
)

if (VICOS)
  # TODO: support tools on mac
  add_subdirectory(tools)

  # Placeholder install task for cozmo_engine. We will eventually transition to using
  # install for all libs & binaries in order to assign appropriate permissions.
  install(TARGETS cozmo_engine
    LIBRARY DESTINATION ${CMAKE_BINARY_DIR}/dist/lib
    ARCHIVE DESTINATION ${CMAKE_BINARY_DIR}/dist/lib
  )
endif()
