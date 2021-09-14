include(generate_clad)

set(CLAD_EMITTER_DIR "${CLAD_BASE_DIR}/emitters")
set(CLAD_CPP "${CLAD_EMITTER_DIR}/CPP_emitter.py")
set(CLAD_CS "${CLAD_EMITER_DIR}/CSharp_emitter.py")
set(CLAD_C "${CLAD_EMITER_DIR}/C_emitter.py")
set(CLAD_PY "${CLAD_EMITTER_DIR}/Python_emitter.py")
set(CLAD_HASH "${CLAD_EMITTER_DIR}/ASTHash_emitter.py")

set(CLAD_INCLUDES
    "${CMAKE_CURRENT_SOURCE_DIR}/sdk"
)

set(CLAD_CPP_FLAGS "--output-union-helper-constructors" "--output-json")

if(NOT ANKI_SRCLIST_DIR)
    set(ANKI_SRCLIST_DIR ${CMAKE_SOURCE_DIR}/generated/cmake)
endif()
file(STRINGS "${ANKI_SRCLIST_DIR}/sdk_clad.srcs.lst" SDK_SRCS)

if(CLAD_VICTOR_EMIT_CPP)
    generate_clad_cpp(
        SRCS "${SDK_SRCS}"
        RELATIVE_SRC_DIR "sdk"
        OUTPUT_DIR "${CLAD_OUTPUT_DIR_CPP}"
        INCLUDES "${CLAD_INCLUDES}"
        OUTPUT_SRCS CLAD_CPP_GEN_SDK_SRCS
        EXCLUDE_JSON
    )

    add_library(sdk_clad_cpp STATIC
        ${CLAD_CPP_GEN_SDK_SRCS}
    )

    target_include_directories(sdk_clad_cpp
        PUBLIC
        $<BUILD_INTERFACE:${CLAD_OUTPUT_DIR_CPP}>
    )

    target_link_libraries(sdk_clad_cpp
        clad
    )

    anki_build_target_license(sdk_clad_cpp "ANKI")
endif()

if(CLAD_VICTOR_EMIT_PYTHON)
    generate_clad_py(
        TARGET "sdk_clad_python"
        SRCS "${SDK_SRCS}"
        RELATIVE_SRC_DIR "sdk"
        OUTPUT_DIR "${CLAD_OUTPUT_DIR_PYTHON}"
        INCLUDES "${CLAD_INCLUDES}"
        OUTPUT_SRCS CLAD_PY_GEN_SDK_SRCS
    )
endif()

if(CLAD_VICTOR_EMIT_CSHARP)
    generate_clad_cs(
        TARGET "sdk_clad_csharp"
        SRCS "${SDK_SRCS}"
        RELATIVE_SRC_DIR "sdk"
        OUTPUT_DIR "${CLAD_OUTPUT_DIR_CSHARP}"
        INCLUDES "${CLAD_INCLUDES}"
        OUTPUT_SRCS CLAD_CS_GEN_SDK_SRCS
    )
endif()