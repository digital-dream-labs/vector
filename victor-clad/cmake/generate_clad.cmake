# helper functions for generating code from clad


# constructs custom commands for executing a specified clad emiiter
# on inputs to generate outpts (in OUTPUT_SRCS).  If LIBRARY is specified,
# creates a library target that depends on the output of the clad emitter
# commands
function(generate_clad)
    set(options "")
    set(oneValueArgs TARGET LIBRARY RELATIVE_SRC_DIR OUTPUT_FILE OUTPUT_DIR EMITTER)
    set(multiValueArgs SRCS OUTPUT_EXTS INCLUDES FLAGS OUTPUT_SRCS)
    cmake_parse_arguments(genclad "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    # message(STATUS "LIBRARY ${genclad_LIBRARY}")

    if(NOT genclad_OUTPUT_EXTS)
        set(genclad_OUTPUT_EXTS ".h" ".cpp")
    endif()

    set(OUTPUT_FILES "")

    foreach(CLAD_SRC ${genclad_SRCS})
        get_filename_component(SRC_DIR ${CLAD_SRC} DIRECTORY)
        get_filename_component(SRC_BASENAME ${CLAD_SRC} NAME_WE)

        set(SRC_DIR_ABS ${CMAKE_CURRENT_SOURCE_DIR}/${genclad_RELATIVE_SRC_DIR})
        get_filename_component(CLAD_SRC_ABS ${CMAKE_CURRENT_SOURCE_DIR}/${CLAD_SRC} ABSOLUTE)
        file(RELATIVE_PATH CLAD_SRC_REL ${SRC_DIR_ABS} ${CLAD_SRC_ABS})

        #message(STATUS "CLAD_SRC: ${CLAD_SRC}")
        #message(STATUS "  SRC_DIR: ${SRC_DIR}")
        #message(STATUS "  SRC_BASENAME: ${SRC_BASENAME}")
        #message(STATUS "  ABS: ${CLAD_SRC_ABS}")
        #message(STATUS "  REL: ${CLAD_SRC_REL}")

        get_filename_component(REL_OUTPUT_DIR ${CLAD_SRC_REL} DIRECTORY)

        set(OUTPUT_BASE "${genclad_OUTPUT_DIR}/${REL_OUTPUT_DIR}/${SRC_BASENAME}")

        set(CLAD_EMITTER_OUTPUTS "")
        foreach(OUTPUT_EXT ${genclad_OUTPUT_EXTS})
            list(APPEND CLAD_EMITTER_OUTPUTS "${OUTPUT_BASE}${OUTPUT_EXT}")
        endforeach()

        set(INCLUDES "")
        if (genclad_INCLUDES)
            list(APPEND INCLUDES "-I")
            list(APPEND INCLUDES ${genclad_INCLUDES})
        endif()

        set(INPUT_DIR "")
        if (genclad_RELATIVE_SRC_DIR)
            set(INPUT_DIR "-C" "${genclad_RELATIVE_SRC_DIR}")
        endif()

        set(OUTPUT_OPTION "-o" "${genclad_OUTPUT_DIR}")
        if (genclad_OUTPUT_FILE)
            set(OUTPUT_OPTION "-o" ${CLAD_EMITTER_OUTPUTS})
            # message(STATUS "OUTPUT_OPTION ${OUTPUT_OPTION}")
        endif()

        #message(STATUS "INCLUDES: ${INCLUDES}")
        #message(STATUS "CLAD_EMITTER_OUTPUTS: ${CLAD_EMITTER_OUTPUTS}")

        add_custom_command(
            COMMAND /usr/bin/env python ${genclad_EMITTER} ${genclad_FLAGS} ${INPUT_DIR} ${INCLUDES} ${OUTPUT_OPTION} ${CLAD_SRC_REL}
            DEPENDS ${genclad_EMITTER} ${CLAD_SRC}
            OUTPUT ${CLAD_EMITTER_OUTPUTS}
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
            COMMENT "Generating code for ${CLAD_SRC}"
        )

        list(APPEND OUTPUT_FILES ${CLAD_EMITTER_OUTPUTS})
    endforeach()

    set(${genclad_OUTPUT_SRCS} ${OUTPUT_FILES} PARENT_SCOPE)

    if (genclad_LIBRARY)
        add_library(${genclad_LIBRARY} STATIC
            ${OUTPUT_FILES}
        )
        anki_build_target_license(${genclad_LIBRARY} "ANKI")
    endif()

    if (genclad_TARGET)
        add_custom_target(${genclad_TARGET} ALL DEPENDS
            ${OUTPUT_FILES}
        )
    endif()

endfunction()

function(generate_clad_cpplite)
    set(options "")
    set(oneValueArgs LIBRARY RELATIVE_SRC_DIR OUTPUT_DIR)
    set(multiValueArgs SRCS INCLUDES FLAGS OUTPUT_SRCS)
    cmake_parse_arguments(genclad "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    set(CLAD_BASE_DIR "${CMAKE_SOURCE_DIR}/victor-clad/tools/message-buffers")
    set(CLAD_EMITTER_DIR "${CLAD_BASE_DIR}/emitters")

    set(CLAD_CPP "${CLAD_EMITTER_DIR}/CPPLite_emitter.py")
    set(CLAD_CPP_EXTS ".h" ".cpp")
    set(CLAD_CPP_FLAGS
        "--max-message-size" "1400"
        "${genclad_FLAGS}")

    set(EMITTERS ${CLAD_CPP})
    set(EXTS CLAD_CPP_EXTS)
    set(FLAGS CLAD_CPP_FLAGS)
    list(LENGTH EMITTERS EMITTER_COUNT)
    math(EXPR EMITTER_COUNT "${EMITTER_COUNT} - 1")

    set(CLAD_GEN_OUTPUTS "")

    #message(STATUS "EMITTERS : ${EMITTERS}")
    #message(STATUS "    EXTS : ${EXTS}")
    #message(STATUS "     LEN : ${EMITTER_COUNT}")

    foreach(IDX RANGE ${EMITTER_COUNT})
        list(GET EMITTERS ${IDX} EMITTER)

        list(GET EXTS ${IDX} EXT_LIST_SYM)
        set(EMITTER_EXTS "${${EXT_LIST_SYM}}")

        if (FLAGS)
            list(GET FLAGS ${IDX} FLAG_LIST_SYM)
            set(EMITTER_FLAGS "${${FLAG_LIST_SYM}}")
        else()
            set(EMITTER_FLAGS "")
        endif()

        # message(STATUS "${IDX} :: ${EMITTER} -- ${EMITTER_EXTS} -- ${EMITTER_FLAGS}")

        generate_clad(
            SRCS ${genclad_SRCS}
            RELATIVE_SRC_DIR ${genclad_RELATIVE_SRC_DIR}
            OUTPUT_DIR ${genclad_OUTPUT_DIR}
            EMITTER ${EMITTER}
            OUTPUT_EXTS ${EMITTER_EXTS}
            FLAGS ${EMITTER_FLAGS}
            INCLUDES ${genclad_INCLUDES}
            OUTPUT_SRCS CLAD_GEN_SRCS
        )

        #message(STATUS "CLAD_GEN_SRCS :: ${CLAD_GEN_SRCS}")
        list(APPEND CLAD_GEN_OUTPUTS ${CLAD_GEN_SRCS})
    endforeach()

    set(${genclad_OUTPUT_SRCS} ${CLAD_GEN_OUTPUTS} PARENT_SCOPE)

    if (genclad_LIBRARY)
        add_library(${genclad_LIBRARY} STATIC
            ${CLAD_GEN_OUTPUTS}
        )
        target_compile_definitions(${genclad_LIBRARY}
          PUBLIC
          USES_CPPLITE
        )
        anki_build_target_license(${genclad_LIBRARY} "ANKI")
    endif()
endfunction()

function(generate_clad_c)
    set(options "")
    set(oneValueArgs LIBRARY RELATIVE_SRC_DIR OUTPUT_DIR)
    set(multiValueArgs SRCS INCLUDES FLAGS OUTPUT_SRCS)
    cmake_parse_arguments(genclad "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    set(CLAD_BASE_DIR "${CMAKE_SOURCE_DIR}/victor-clad/tools/message-buffers")
    set(CLAD_EMITTER_DIR "${CLAD_BASE_DIR}/emitters")

    set(CLAD_C "${CLAD_EMITTER_DIR}/C_emitter.py")
    set(CLAD_C_EXTS ".h" ".c")
    set(CLAD_C_FLAGS "${genclad_FLAGS}")

    set(EMITTERS ${CLAD_C})
    set(EXTS CLAD_C_EXTS)
    set(FLAGS CLAD_C_FLAGS)
    list(LENGTH EMITTERS EMITTER_COUNT)
    math(EXPR EMITTER_COUNT "${EMITTER_COUNT} - 1")

    set(CLAD_GEN_OUTPUTS "")

    #message(STATUS "EMITTERS : ${EMITTERS}")
    #message(STATUS "    EXTS : ${EXTS}")
    #message(STATUS "     LEN : ${EMITTER_COUNT}")

    foreach(IDX RANGE ${EMITTER_COUNT})
        list(GET EMITTERS ${IDX} EMITTER)

        list(GET EXTS ${IDX} EXT_LIST_SYM)
        set(EMITTER_EXTS "${${EXT_LIST_SYM}}")

        if (FLAGS)
            list(GET FLAGS ${IDX} FLAG_LIST_SYM)
            set(EMITTER_FLAGS "${${FLAG_LIST_SYM}}")
        else()
            set(EMITTER_FLAGS "")
        endif()

        # message(STATUS "${IDX} :: ${EMITTER} -- ${EMITTER_EXTS} -- ${EMITTER_FLAGS}")

        generate_clad(
            SRCS ${genclad_SRCS}
            RELATIVE_SRC_DIR ${genclad_RELATIVE_SRC_DIR}
            OUTPUT_DIR ${genclad_OUTPUT_DIR}
            EMITTER ${EMITTER}
            OUTPUT_EXTS ${EMITTER_EXTS}
            FLAGS ${EMITTER_FLAGS}
            INCLUDES ${genclad_INCLUDES}
            OUTPUT_SRCS CLAD_GEN_SRCS
        )

        #message(STATUS "CLAD_GEN_SRCS :: ${CLAD_GEN_SRCS}")
        list(APPEND CLAD_GEN_OUTPUTS ${CLAD_GEN_SRCS})
    endforeach()

    set(${genclad_OUTPUT_SRCS} ${CLAD_GEN_OUTPUTS} PARENT_SCOPE)

    if (genclad_LIBRARY)
        add_library(${genclad_LIBRARY} STATIC
            ${CLAD_GEN_OUTPUTS}
        )
        anki_build_target_license(${genclad_LIBRARY} "ANKI")
    endif()
endfunction()

# calls generate_clad using each of the C++ emitters used for cozmoEngine
function(generate_clad_cpp)
    set(options EXCLUDE_JSON)
    set(oneValueArgs LIBRARY RELATIVE_SRC_DIR OUTPUT_DIR)
    set(multiValueArgs SRCS INCLUDES FLAGS OUTPUT_SRCS)
    cmake_parse_arguments(genclad "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    set(CLAD_BASE_DIR "${CMAKE_SOURCE_DIR}/victor-clad/tools/message-buffers")
    set(CLAD_EMITTER_DIR "${CLAD_BASE_DIR}/emitters")

    set(CLAD_CPP "${CLAD_EMITTER_DIR}/CPP_emitter.py")
    set(CLAD_CPP_EXTS ".h" ".cpp")

    set(OUTPUT_JSON "")
    if(NOT genclad_EXCLUDE_JSON)
    set(OUTPUT_JSON "--output-json")
    endif(NOT genclad_EXCLUDE_JSON)

    set(CLAD_CPP_FLAGS
        "--output-union-helper-constructors"
        "${OUTPUT_JSON}"
        "${genclad_FLAGS}")

    set(CLAD_VICTOR_EMITTER_DIR ${CMAKE_SOURCE_DIR}/victor-clad/victorEmitters)
    set(CLAD_CPP_DECL "${CLAD_VICTOR_EMITTER_DIR}/cozmo_CPP_declarations_emitter.py")
    set(CLAD_CPP_DECL_EXTS "_declarations.def")
    set(CLAD_CPP_DECL_FLAGS "")

    set(CLAD_CPP_SWITCH "${CLAD_VICTOR_EMITTER_DIR}/cozmo_CPP_switch_emitter.py")
    set(CLAD_CPP_SWITCH_EXTS "_switch.def")
    set(CLAD_CPP_SWITCH_FLAGS "")

    set(CLAD_HASH "${CLAD_EMITTER_DIR}/ASTHash_emitter.py")
    set(CLAD_HASH_EXTS "_hash.h")
    set(CLAD_HASH_FLAGS "")

    set(EMITTERS ${CLAD_CPP} ${CLAD_CPP_DECL} ${CLAD_CPP_SWITCH})
    set(EXTS CLAD_CPP_EXTS CLAD_CPP_DECL_EXTS CLAD_CPP_SWITCH_EXTS)
    set(FLAGS CLAD_CPP_FLAGS CLAD_CPP_DECL_FLAGS CLAD_CPP_SWITCH_FLAGS)
    list(LENGTH EMITTERS EMITTER_COUNT)
    math(EXPR EMITTER_COUNT "${EMITTER_COUNT} - 1")

    set(CLAD_GEN_OUTPUTS "")

    #message(STATUS "EMITTERS : ${EMITTERS}")
    #message(STATUS "    EXTS : ${EXTS}")
    #message(STATUS "     LEN : ${EMITTER_COUNT}")

    foreach(IDX RANGE ${EMITTER_COUNT})
        list(GET EMITTERS ${IDX} EMITTER)

        list(GET EXTS ${IDX} EXT_LIST_SYM)
        set(EMITTER_EXTS "${${EXT_LIST_SYM}}")

        if (FLAGS)
            list(GET FLAGS ${IDX} FLAG_LIST_SYM)
            set(EMITTER_FLAGS "${${FLAG_LIST_SYM}}")
        else()
            set(EMITTER_FLAGS "")
        endif()

        # message(STATUS "${IDX} :: ${EMITTER} -- ${EMITTER_EXTS} -- ${EMITTER_FLAGS}")

        generate_clad(
            SRCS ${genclad_SRCS}
            RELATIVE_SRC_DIR ${genclad_RELATIVE_SRC_DIR}
            OUTPUT_DIR ${genclad_OUTPUT_DIR}
            EMITTER ${EMITTER}
            OUTPUT_EXTS ${EMITTER_EXTS}
            FLAGS ${EMITTER_FLAGS}
            INCLUDES ${genclad_INCLUDES}
            OUTPUT_SRCS CLAD_GEN_SRCS
        )

        #message(STATUS "CLAD_GEN_SRCS :: ${CLAD_GEN_SRCS}")
        list(APPEND CLAD_GEN_OUTPUTS ${CLAD_GEN_SRCS})
    endforeach()

    set(${genclad_OUTPUT_SRCS} ${CLAD_GEN_OUTPUTS} PARENT_SCOPE)

    if (genclad_LIBRARY)
        add_library(${genclad_LIBRARY} STATIC
            ${CLAD_GEN_OUTPUTS}
        )
        target_compile_definitions(${genclad_LIBRARY}
          PUBLIC
          USES_CLAD
        )
        anki_build_target_license(${genclad_LIBRARY} "ANKI")
    endif()

endfunction()

# calls generate_clad using each of the C++ emitters used for cozmoEngine
function(generate_clad_py)
    set(options "")
    set(oneValueArgs TARGET RELATIVE_SRC_DIR OUTPUT_DIR)
    set(multiValueArgs SRCS INCLUDES FLAGS OUTPUT_SRCS)
    cmake_parse_arguments(genclad "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    set(CLAD_BASE_DIR "${CMAKE_SOURCE_DIR}/victor-clad/tools/message-buffers")
    set(CLAD_EMITTER_DIR "${CLAD_BASE_DIR}/emitters")

    set(CLAD_PY "${CLAD_EMITTER_DIR}/Python_emitter.py")
    set(CLAD_PY_EXTS ".py")
    set(CLAD_PY_FLAGS "${genclad_FLAGS}")

    set(CLAD_VICTOR_EMITTER_DIR ${CMAKE_SOURCE_DIR}/victor-clad/victorEmitters)
    set(CLAD_PY_DECL "${CLAD_VICTOR_EMITTER_DIR}/cozmo_Python_declarations_emitter.py")
    set(CLAD_PY_DECL_EXTS "_declarations.def")
    set(CLAD_PY_DECL_FLAGS "")

    set(EMITTERS ${CLAD_PY})
    set(EXTS CLAD_PY_EXTS)
    set(FLAGS CLAD_PY_FLAGS)
    list(LENGTH EMITTERS EMITTER_COUNT)
    math(EXPR EMITTER_COUNT "${EMITTER_COUNT} - 1")

    set(CLAD_GEN_OUTPUTS "")

    foreach(IDX RANGE ${EMITTER_COUNT})
        list(GET EMITTERS ${IDX} EMITTER)

        list(GET EXTS ${IDX} EXT_LIST_SYM)
        set(EMITTER_EXTS "${${EXT_LIST_SYM}}")

        if (FLAGS)
            list(GET FLAGS ${IDX} FLAG_LIST_SYM)
            set(EMITTER_FLAGS "${${FLAG_LIST_SYM}}")
        else()
            set(EMITTER_FLAGS "")
        endif()


        generate_clad(
            SRCS ${genclad_SRCS}
            RELATIVE_SRC_DIR ${genclad_RELATIVE_SRC_DIR}
            OUTPUT_DIR ${genclad_OUTPUT_DIR}
            EMITTER ${EMITTER}
            OUTPUT_EXTS ${EMITTER_EXTS}
            FLAGS ${EMITTER_FLAGS}
            INCLUDES ${genclad_INCLUDES}
            OUTPUT_SRCS CLAD_GEN_SRCS
        )

        list(APPEND CLAD_GEN_OUTPUTS ${CLAD_GEN_SRCS})
    endforeach()

    set_source_files_properties(${CLAD_GEN_OUTPUTS} PROPERTIES GENERATED TRUE)

    set(${genclad_OUTPUT_SRCS} ${CLAD_GEN_OUTPUTS} PARENT_SCOPE)

    if (genclad_TARGET)
        add_custom_target(${genclad_TARGET} ALL DEPENDS
            ${CLAD_GEN_OUTPUTS}
        )
    endif()

endfunction()

# calls generate_clad using each of the C# emitters used for cozmoEngine
function(generate_clad_cs)
    set(options "")
    set(oneValueArgs TARGET RELATIVE_SRC_DIR OUTPUT_DIR)
    set(multiValueArgs SRCS INCLUDES FLAGS OUTPUT_SRCS)
    cmake_parse_arguments(genclad "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    set(CLAD_BASE_DIR "${CMAKE_SOURCE_DIR}/victor-clad/tools/message-buffers")
    set(CLAD_EMITTER_DIR "${CLAD_BASE_DIR}/emitters")

    set(CLAD_CS "${CLAD_EMITTER_DIR}/CSharp_emitter.py")
    set(CLAD_CS_EXTS ".cs")
    set(CLAD_CS_FLAGS "${genclad_FLAGS}")

    set(CLAD_VICTOR_EMITTER_DIR ${CMAKE_SOURCE_DIR}/victor-clad/victorEmitters)
    set(CLAD_CS_DECL "${CLAD_VICTOR_EMITTER_DIR}/cozmo_CSharp_declarations_emitter.py")
    set(CLAD_CS_DECL_EXTS "_declarations.def")
    set(CLAD_CS_DECL_FLAGS "")

    set(EMITTERS ${CLAD_CS})
    set(EXTS CLAD_CS_EXTS)
    set(FLAGS CLAD_CS_FLAGS)
    list(LENGTH EMITTERS EMITTER_COUNT)
    math(EXPR EMITTER_COUNT "${EMITTER_COUNT} - 1")

    set(CLAD_GEN_OUTPUTS "")

    foreach(IDX RANGE ${EMITTER_COUNT})
        list(GET EMITTERS ${IDX} EMITTER)

        list(GET EXTS ${IDX} EXT_LIST_SYM)
        set(EMITTER_EXTS "${${EXT_LIST_SYM}}")

        if (FLAGS)
            list(GET FLAGS ${IDX} FLAG_LIST_SYM)
            set(EMITTER_FLAGS "${${FLAG_LIST_SYM}}")
        else()
            set(EMITTER_FLAGS "")
        endif()

        generate_clad(
            SRCS ${genclad_SRCS}
            RELATIVE_SRC_DIR ${genclad_RELATIVE_SRC_DIR}
            OUTPUT_DIR ${genclad_OUTPUT_DIR}
            EMITTER ${EMITTER}
            OUTPUT_EXTS ${EMITTER_EXTS}
            FLAGS ${EMITTER_FLAGS}
            INCLUDES ${genclad_INCLUDES}
            OUTPUT_SRCS CLAD_GEN_SRCS
        )

        list(APPEND CLAD_GEN_OUTPUTS ${CLAD_GEN_SRCS})
    endforeach()

    set_source_files_properties(${CLAD_GEN_OUTPUTS} PROPERTIES GENERATED TRUE)

    set(${genclad_OUTPUT_SRCS} ${CLAD_GEN_OUTPUTS} PARENT_SCOPE)

    if (genclad_TARGET)
        add_custom_target(${genclad_TARGET} ALL DEPENDS
            ${CLAD_GEN_OUTPUTS}
        )
    endif()

endfunction()
