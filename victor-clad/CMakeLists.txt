cmake_minimum_required(VERSION 3.6)

if(NOT CMAKE_CXX_STANDARD)
    set(CMAKE_CXX_STANDARD 14)
endif()

list(APPEND CMAKE_MODULE_PATH
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake
)

# Default assume building C++
option(CLAD_VICTOR_EMIT_CPP "Enable SDK C++ Emitter" ON)
option(CLAD_VICTOR_EMIT_PYTHON "Enable SDK Python Emitter" OFF)
option(CLAD_VICTOR_EMIT_CSHARP "Enable SDK C# Emitter" OFF)
option(CLAD_VICTOR_SKIP_LICENSE "Skip licensing for builds outside of victor" OFF)

if(CLAD_VICTOR_SKIP_LICENSE)
    function(anki_build_target_license target)
        message(WARNING "skipping license for ${target} because CLAD_VICTOR_SKIP_LICENSE is ${CLAD_VICTOR_SKIP_LICENSE}")
    endfunction()
endif()

# default directories
set(CLAD_OUTPUT_DIR_CPP "${CMAKE_SOURCE_DIR}/generated/clad")
set(CLAD_OUTPUT_DIR_PYTHON "${CMAKE_SOURCE_DIR}/generated/cladPython")
set(CLAD_OUTPUT_DIR_CSHARP "${CMAKE_SOURCE_DIR}/generated/cladCSharp")

add_subdirectory(tools/message-buffers)
set(CLAD_BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/tools/message-buffers")
set(CLAD_VICTOR_EMITTER_DIR "${CMAKE_CURRENT_SOURCE_DIR}/victorEmitters")
add_subdirectory(clad)

if(CLAD_VICTOR_EMIT_CPP AND NOT CLAD_OUTPUT_DIR_CPP)
    message(FATAL_ERROR "CLAD_OUTPUT_DIR_CPP is not set, but CLAD_VICTOR_EMIT_CPP is ON.")
endif()

if(CLAD_VICTOR_EMIT_PYTHON AND NOT CLAD_OUTPUT_DIR_PYTHON)
    message(FATAL_ERROR "CLAD_OUTPUT_DIR_PYTHON is not set, but CLAD_VICTOR_EMIT_PYTHON is ON.")
endif()

if(CLAD_VICTOR_EMIT_CSHARP AND NOT CLAD_OUTPUT_DIR_CSHARP)
    message(FATAL_ERROR "CLAD_OUTPUT_DIR_CSHARP is not set, but CLAD_VICTOR_EMIT_CSHARP is ON.")
endif()
