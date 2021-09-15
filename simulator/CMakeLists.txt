#
# simulator/CMakeLists.txt
#
cmake_minimum_required(VERSION 3.6)

project(webots_controllers)

if (NOT MACOSX)
  return()
endif()

include(opencv)
include(protobuf)
include(symlink_target)
include(anki_build_cxx)
include(webots)

# Path to controllers
set(CONTROLLERS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/controllers")

# Path to resources
set(BUILT_RESOURCES_DIR "${CMAKE_BINARY_DIR}/data/assets/cozmo_resources")

set(PLATFORM_LIBS "")
set(PLATFORM_INCLUDES "")
set(PLATFORM_COMPILE_DEFS "")
set(PLATFORM_FLAGS "")
if (MACOSX)
  include(webots)
  find_library(FOUNDATION Foundation)
  find_library(OPENGL OpenGL)
  find_library(GLUT GLUT)
  set(PLATFORM_LIBS
    ${FOUNDATION}
    ${OPENGL}
    ${PROTOBUF_LIBS}
    ${GLUT}
    ${WEBOTS_LIBS}
    ${WEBOTS_SIM_LIBS}
    ${WEBOTS_PLUGINS}
  )
endif()


#
# webots plugins
#

anki_build_cxx_library(cozmo_physics ${ANKI_SRCLIST_DIR})
anki_build_target_license(cozmo_physics "ANKI")

#
# suppress warnings about old GLUT declarations used by webots
#
target_compile_options(cozmo_physics
  PRIVATE
  -Wno-deprecated-declarations
)

target_compile_definitions(cozmo_physics
  PRIVATE
  MACOS
)

target_link_libraries(cozmo_physics
  PRIVATE
  cozmo_engine
  ${PLATFORM_LIBS}
)


symlink_target(
  TARGET cozmo_physics
  DST "${CMAKE_CURRENT_SOURCE_DIR}/plugins/physics/cozmo_physics/"
)


#
# webots libs
#

anki_build_cxx_library(webotsCtrlCommon ${ANKI_SRCLIST_DIR} STATIC)
anki_build_target_license(webotsCtrlCommon "ANKI")

target_link_libraries(webotsCtrlCommon
  PUBLIC
  util
  cti_common
  robot_interface # for emrHelper.h
  ${PLATFORM_LIBS}
)

target_compile_definitions(webotsCtrlCommon
  PUBLIC
  WEBOTS_VER=\"${WEBOTS_VER}\"
  SIMULATOR
)

#
# webots controller macros
#

macro(webots_build_cxx_executable target srclist_dir)
  # Build the executable
  anki_build_cxx_executable(${target} ${srclist_dir})
  # Sign executable for use with webots firewall certificate
  if (WEBOTS_SETUP_FIREWALL)
    webots_setup_target(${target})
  endif(WEBOTS_SETUP_FIREWALL)
endmacro(webots_build_cxx_executable)

#
# webots controller executables
#

webots_build_cxx_executable(webotsCtrlLightCube ${ANKI_SRCLIST_DIR})
anki_build_target_license(webotsCtrlLightCube "ANKI")

target_link_libraries(webotsCtrlLightCube
  PRIVATE
  util
  cti_common
  cube_led_animator
  engine_clad
  robot_interface
  webotsCtrlCommon
  ${PLATFORM_LIBS}
)

target_compile_definitions(webotsCtrlLightCube
  PRIVATE
  SIMULATOR
)

symlink_target(
  TARGET webotsCtrlLightCube
  DST "${CONTROLLERS_DIR}/webotsCtrlLightCube/"
)

symlink_target(
  TARGET webotsCtrlLightCube
  SRC ${BUILT_RESOURCES_DIR}
  DST ${CONTROLLERS_DIR}/webotsCtrlLightCube/resources
)


webots_build_cxx_executable(webotsCtrlViz ${ANKI_SRCLIST_DIR})
anki_build_target_license(webotsCtrlViz "ANKI")

target_link_libraries(webotsCtrlViz
  PRIVATE
  util
  kazmath
  cozmo_engine
  webotsCtrlCommon
  ${PLATFORM_LIBS}
)

symlink_target(
  TARGET webotsCtrlViz
  DST "${CONTROLLERS_DIR}/webotsCtrlViz/"
)

symlink_target(
  TARGET webotsCtrlViz
  SRC ${BUILT_RESOURCES_DIR}
  DST ${CONTROLLERS_DIR}/webotsCtrlViz/resources
)

#
# webotsCtrlRobot2
#
webots_build_cxx_executable(webotsCtrlRobot2 ${ANKI_SRCLIST_DIR})
anki_build_target_license(webotsCtrlRobot2 "ANKI")

target_link_libraries(webotsCtrlRobot2
  PRIVATE
  util
  kazmath
  cti_common
  cti_common_robot
  cti_vision
  cti_messaging
  supervisor
  robot_interface
  robot_clad_cpplite
  webotsCtrlCommon
  ${OPENCV_LIBS}
  ${PLATFORM_LIBS}
)

target_compile_definitions(webotsCtrlRobot2
  PRIVATE
  COZMO_ROBOT
  SIMULATOR
)

symlink_target(
  TARGET webotsCtrlRobot2
  DST "${CONTROLLERS_DIR}/webotsCtrlRobot2/"
)

symlink_target(
  TARGET webotsCtrlRobot2
  SRC ${BUILT_RESOURCES_DIR}
  DST ${CONTROLLERS_DIR}/webotsCtrlRobot2/resources
)

#
# webotsCtrlAnim
#

webots_build_cxx_executable(webotsCtrlAnim ${ANKI_SRCLIST_DIR})
anki_build_target_license(webotsCtrlAnim "ANKI")

target_link_libraries(webotsCtrlAnim
  PRIVATE
  util
  kazmath
  cti_common
  #cti_common_robot
  cti_vision
  robot_interface
  webotsCtrlCommon
  victor_anim
  robot_clad_cpplite
  osState
  ${OPENCV_LIBS}
  ${PLATFORM_LIBS}
)

target_compile_definitions(webotsCtrlAnim
  PRIVATE
  SIMULATOR
)

symlink_target(
  TARGET webotsCtrlAnim
  DST "${CONTROLLERS_DIR}/webotsCtrlAnim/"
)

symlink_target(
  TARGET webotsCtrlAnim
  SRC ${BUILT_RESOURCES_DIR}
  DST ${CONTROLLERS_DIR}/webotsCtrlAnim/resources
)

#
# webotsCtrlWebServer
#

webots_build_cxx_executable(webotsCtrlWebServer ${ANKI_SRCLIST_DIR})
anki_build_target_license(webotsCtrlWebServer "ANKI")

target_link_libraries(webotsCtrlWebServer
  PRIVATE
  #util
  webotsCtrlCommon
  osState
  victor_web_library
)

target_compile_definitions(webotsCtrlWebServer
  PRIVATE
  SIMULATOR
)

symlink_target(
  TARGET webotsCtrlWebServer
  DST "${CONTROLLERS_DIR}/webotsCtrlWebServer/"
)

symlink_target(
  TARGET webotsCtrlWebServer
  SRC ${BUILT_RESOURCES_DIR}
  DST ${CONTROLLERS_DIR}/webotsCtrlWebServer/resources
)

#
# webotsCtrlGameEngine2
#

webots_build_cxx_executable(webotsCtrlGameEngine2 ${ANKI_SRCLIST_DIR})
anki_build_target_license(webotsCtrlGameEngine2 "ANKI")

target_link_libraries(webotsCtrlGameEngine2
  PRIVATE
  util
  kazmath
  cameraService
  whiskeyToF
  cti_common
  cti_common_robot
  cti_vision
  cti_messaging
  cti_planning
  cozmo_engine
  cubeBleClient
  osState
  webotsCtrlCommon
  ${OPENCV_LIBS}
  ${PLATFORM_LIBS}
)

target_compile_definitions(webotsCtrlGameEngine2
  PRIVATE
  SIMULATOR
)

symlink_target(
  TARGET webotsCtrlGameEngine2
  DST "${CONTROLLERS_DIR}/webotsCtrlGameEngine2/"
)

symlink_target(
  TARGET webotsCtrlGameEngine2
  SRC ${BUILT_RESOURCES_DIR}
  DST ${CONTROLLERS_DIR}/webotsCtrlGameEngine2/resources
)

#
# webotsCtrlKeyboard
#

webots_build_cxx_executable(webotsCtrlKeyboard ${ANKI_SRCLIST_DIR})
anki_build_target_license(webotsCtrlKeyboard "ANKI")

target_link_libraries(webotsCtrlKeyboard
  PRIVATE
  util
  kazmath
  webotsCtrlCommon
  cozmo_engine
  robot_interface
  ${PLATFORM_LIBS}
)

target_include_directories(webotsCtrlKeyboard
  PRIVATE
  ${CMAKE_SOURCE_DIR}
)

target_compile_definitions(webotsCtrlKeyboard
  PRIVATE
  CORETECH_ENGINE
)

symlink_target(
  TARGET webotsCtrlKeyboard
  DST "${CONTROLLERS_DIR}/webotsCtrlKeyboard/"
)

symlink_target(
  TARGET webotsCtrlKeyboard
  SRC ${BUILT_RESOURCES_DIR}
  DST ${CONTROLLERS_DIR}/webotsCtrlKeyboard/resources
)

#
# webotsCtrlBuildServerTest
#

webots_build_cxx_executable(webotsCtrlBuildServerTest ${ANKI_SRCLIST_DIR})
anki_build_target_license(webotsCtrlBuildServerTest "ANKI")

target_link_libraries(webotsCtrlBuildServerTest
  PRIVATE
  util
  kazmath
  cti_common
  cti_vision
  cti_messaging
  cti_planning
  webotsCtrlCommon
  cozmo_engine
  robot_interface
  ${OPENCV_LIBS}
  ${PLATFORM_LIBS}
)

target_include_directories(webotsCtrlBuildServerTest
  PRIVATE
  ${CMAKE_SOURCE_DIR}
)

target_compile_definitions(webotsCtrlBuildServerTest
  PRIVATE
  CORETECH_ENGINE
)

symlink_target(
  TARGET webotsCtrlBuildServerTest
  DST "${CONTROLLERS_DIR}/webotsCtrlBuildServerTest/"
)


#
# webotsCtrlDevLog
#

webots_build_cxx_executable(webotsCtrlDevLog ${ANKI_SRCLIST_DIR})
anki_build_target_license(webotsCtrlDevLog "ANKI")

target_link_libraries(webotsCtrlDevLog
  PRIVATE
  util
  kazmath
  cti_common
  cti_vision
  cti_messaging
  cti_planning
  webotsCtrlCommon
  cozmo_engine
  robot_interface
  ${OPENCV_LIBS}
  ${PLATFORM_LIBS}
)

target_include_directories(webotsCtrlDevLog
  PRIVATE
  ${CMAKE_SOURCE_DIR}
)

target_compile_definitions(webotsCtrlDevLog
  PRIVATE
  CORETECH_ENGINE=1
)

symlink_target(
  TARGET webotsCtrlDevLog
  DST "${CONTROLLERS_DIR}/webotsCtrlDevLog/"
)

#
# webotsCtrlGateway
#

webots_build_cxx_executable(webotsCtrlGateway ${ANKI_SRCLIST_DIR})
anki_build_target_license(webotsCtrlGateway "ANKI")

target_link_libraries(webotsCtrlGateway
  PRIVATE
  webotsCtrlCommon
)

target_compile_definitions(webotsCtrlGateway
  PRIVATE
  SIMULATOR
)

symlink_target(
  TARGET webotsCtrlGateway
  DST "${CONTROLLERS_DIR}/webotsCtrlGateway/"
)

symlink_target(
  TARGET webotsCtrlGateway
  SRC ${CMAKE_BINARY_DIR}/bin/vic-gateway
  DST "${CONTROLLERS_DIR}/webotsCtrlGateway/"
)

