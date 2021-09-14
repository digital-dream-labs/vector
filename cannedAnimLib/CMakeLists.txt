cmake_minimum_required(VERSION 3.6)

project(canned_anim_lib)

include(anki_build_cxx)

anki_build_cxx_library(canned_anim_lib_anim ${ANKI_SRCLIST_DIR} STATIC)
anki_build_target_license(canned_anim_lib_anim "ANKI")

include(flatbuffers)

target_link_libraries(canned_anim_lib_anim
    PRIVATE
    # cti
    cti_common_robot
    cti_vision
    cti_messaging_robot
    robot_clad_cpplite
    #vendor
    ${FLATBUFFERS_LIBS}
    PUBLIC
    robot_interface # Needs to be public for cozmoConfig.h
    ${ASAN_LINKER_FLAGS}
)

anki_build_cxx_library(canned_anim_lib_engine ${ANKI_SRCLIST_DIR} STATIC)
anki_build_target_license(canned_anim_lib_engine "ANKI")

target_compile_definitions(canned_anim_lib_engine PRIVATE CAN_STREAM=false)

target_link_libraries(canned_anim_lib_engine
    PRIVATE
    # cti
    cti_common
    cti_vision
    cti_messaging
    engine_clad
    #vendor
    ${FLATBUFFERS_LIBS}
    PUBLIC
    robot_interface # Needs to be public for cozmoConfig.h
    ${ASAN_LINKER_FLAGS}
)
