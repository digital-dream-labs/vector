# Victor Build System Walk-through

One of the main goals for Victor is to have _one_ build system and to put everything into it.
Right now, the main build system is CMake. There are some key helper scripts, but they adhere to
rules such that the build system is always the source of truth about when output will be generated
from input. This document outlines the flow of information through the build. My hope is that after
reading this, anyone will be able to add new components to the build system.

## Build Script Procedure

This is the main entry-point is currently the script:

```
./project/victor/build-victor.sh
```

This script performs 3 main functions, in order:

1. Generates lists of files to build for each target

    Source file specifications for build targets are defined in `BUILD.in` files, which reside outside
    of CMake. These files are designed to work like `BUCK` and `BUILD` files from buck and bazel build systems.
    `BUILD.in` are python files that are evaluated using a sandboxed subset of python, using the self-contained
    python script [`metabuild.py`](/tools/build/tools/metabuild/metabuild.py).

    Primarily these functions
    provide a clear definition of what files will be included in each build target and to
    limit ad hoc glob specifications. They strictly enforce the constraint that files assigned to a build target
    must reside under the root of the target directory (the folder containing `CMakeLists.txt` and `BUILD.in`).
    This strategy prevents "out-of-tree" references to source files that results in hidden cross-target
    dependencies, which are the root of all evil from the perspective of the build system.

2. Configures CMake for the specified platform, generator and build-type

    If [`build-victor.sh`](/project/victor/build-victor.sh) determines that no build rules are present, and then invokes CMake in "configure" mode.
    This causes CMake to evaluate the graph of `CMakeLists.txt` files starting from the one in the root of the directory.
    This phase checks the compiler for the specified platform and then generated build rules that will evaluated
    by either `ninja` or `xcodebuild`.

3. Invokes the build command

    Once build rules exist, the native build system (e.g. `ninja`, `xcodebuild`) is invoked to perform actions.
    CMake automatically adds build rules to invoke the configure process if CMakeLists.txt files have changed.


## Components

- `CMakeLists.txt`

    Contain CMake build instructions for a project or module. Note that there is no strict requirement to have
    one target per `CMakeLists.txt` file. Some projects use this approach, but victor does not.  In practice,
    combining build configurations for multiple related targets into one `CMakeLists.txt` file is easier to understand.
    Also, because the source definitions are contained in `BUILD.in`, `CMakeLists.txt` typically only contains information
    about _build target_ dependencies and configuration.


- `cmake/*.cmake`

    These are custom support modules used inside `CMakeLists.txt` files. They are intended to be `include`ed in
    so that their contents are evaluated inside the scope of the current `CMakeLists.txt` file.


- `BUILD.in`

    Used to declare source files that belong to build targets. These are Pythonic files, evaluated in a 
    sandboxed python context. This process is performed by [`metabuild.py`](/tools/build/tools/metabuild/metabuild.py), which is called from inside
    [`build-victor.sh`](/project/victor/build-victor.sh). The syntax is modeled after [BUCK](http://buckbuild.com), and the `glob` and `subdir_glob`
    functions work the same way as described in the buck documentation.
    The output of each target in `BUILD.in` is a set of `.lst` files inside
    `generated/cmake`, named using the target name as the prefix. The helper functions used in `BUILD.in`
    automatically handle the platform-specific exclude rules that we typically use, for example `*_vicos.*`
    files are separated into a platform-specific `.lst` file, which makes it easy to include platform-specific
    sources from within CMake.


## CMake Overview

There are many ways to use CMake, but the general pattern of usage in victor follows more modern standards
of using per-target functions to configure targets rather then relying on changing any global CMake vars.
To the extent possible, the contents of `CMakeLists.txt` files are  written in a **declarative** style.
Tasks that require branching logic (if/else) or looping should be defined in helper `.cmake` files, using
functions or macros that provide a declarative interface.

The core unit of organization is a build target, typically a `library` or `executable`. Each target
can then be assigned PUBLIC or PRIVATE properties such as link dependencies, include dependencies,
compiler options, and preprocessor definitions. PRIVATE properties are only visible to the target, whereas
PUBLIC properties are inherited by targets that depend on the current target. In this way, it is fairly
straightforward to define the graph of dependencies.


## Helper scripts

The main purpose of the build system (CMake in this case) is to determine when steps should be run or generate
rules for an underlying build system to determine when steps should be run. Because of this, it is important
that any programs invoked by the build do not include hidden logic that controls when something runs.
In practice, this means that programs run by the build system should typically follow the
[compiler pattern](http://www.catb.org/esr/writings/taoup/html/ch11s06.html#id2958199). Programs should
produce predictable outputs from a specified set of inputs and when necessary, provide the ability to output
dependencies for each input element.  For an example of this, see the "make dependency" options for `gcc`/`clang`
(`-MD -MF <deps file>` options).


## Anatomy of a build target

As an example, let's dive into the build definition of the `victor_anim` lib used by the animation process
(`vic-anim`).

```
#
# animProcess/BUILD.in
#
cxx_project(
    name = 'victor_anim',
    srcs = cxx_src_glob(['src/cozmoAnim'],
                        excludes = ['*_vicos.*',
                                    'vicos/**/*',
                                    '*_mac.*',
                                    'cozmoAnimMain.cpp']),
    platform_srcs = [
        ('vicos', glob(['vicos/**/*.cpp',
                          '**/*_vicos.cpp'])),
        ('mac', glob(['**/*_mac.cpp']))
    ],
    headers = cxx_header_glob(['.']),
    platform_headers = [
    ]
)

cxx_project(
    name = 'vic-anim',
    srcs = glob(['src/cozmoAnim/cozmoAnimMain.cpp']),
    platform_srcs = [
    ],
    headers = [],
    platform_headers = [
    ]
)

```

[`metabuild.py`](/tools/build/tools/metabuild/metabuild.py) interprets this file and outputs:

```
generated/cmake/
├── vic-anim.headers.lst
├── vic-anim.srcs.lst
├── victor_anim.headers.lst
├── victor_anim.srcs.lst
├── victor_anim_mac.srcs.lst
└── victor_anim_vicos.srcs.lst
```

The corresponding `CMakeLists.txt` file uses this definition to create a `library` target and then
configures the library to define link and include deps, compiler options and preprocessor definitions.

from [`animProcess/CMakeLists.txt`](/animProcess/CMakeLists.txt)

```
# Include helper function for constructing targets from .lst files
# This include the file: cmake/anki_build_cxx_library.cmake
include(anki_build_cxx_library)

# Create C++ library target using the source files in `generated/cmake/victor_anim*.lst` files
# This creates uses the current platform setting to include the correct set `.lst` files
anki_build_cxx_library(victor_anim ${ANKI_SRCLIST_DIR} STATIC)

# Define the list of libraries that victor_anim should link against
# All of the link dependencies are other target defined in different 
target_link_libraries(victor_anim
  PUBLIC
  victor_web_library
  robot_interface  # Needs to be public for cozmoConfig.h
  DAS
  PRIVATE
  util
  util_audio
  canned_anim_lib_anim
  # cti
  cti_common_robot
  cti_vision
  cti_messaging_robot
  robot_clad_cpplite
  cloud_clad_cpp
  audio_engine
  audio_multiplexer_robot
  osState
  micdata
  # vendor
  ${OPENCV_LIBS}
  ${FLATBUFFERS_LIBS}
  signal_essence
  ${TEXT2SPEECH_LIBS}
  ${SENSORY_LIBS}
  # platform
  ${PLATFORM_LIBS}
  # Aubio beat detection
  ${AUBIO_LIBS}
  PUBLIC
  victor_web_library
  robot_interface  # Needs to be public for cozmoConfig.h
)

# Add a post build step on Vicos to strip the library if necessary
anki_build_strip(TARGET victor_anim)

# Define the preprocessor definitions for victor_anim
target_compile_definitions(victor_anim
  PRIVATE
  USE_DAS=$<BOOL:${USE_DAS}>
  ${PLATFORM_COMPILE_DEFS}
)

# Specify include paths for victor_anim target
target_include_directories(victor_anim
  PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
  $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/lib/util/source/3rd>
  ${PLATFORM_INCLUDES}
)

```

## Caveats

These steps in [`build-victor.sh`](/project/victor/build-victor.sh) are intended to only run when necessary. If file lists exist, they will not be updated.
If CMake determines that all build metadata and commands/rules are up-to-date, it will skip the configure
phase.  The build command will typically always run.

Unfortunately, file list generation is done directly by [`metabuild.py`](/tools/build/tools/metabuild/metabuild.py), which is outside of CMake.
This means that if new files are added or old files are deleted, CMake currently won't be able to find them unless
you "force" the build script to re-generate them. This can be done by running:

```
./project/victor/build-victor.sh -f
```

It is important to keep in mind that CMake itself is a meta-build system.
It has a "configure" phase, where build rules are defined and a "build" phase where the rules are
evaluated/executed. For the case of `ninja`, the configure step checks for an appropriate compiler and evaluates
the contents of the graph of `CMakeLists.txt` files to generate ninja build rules. The final build phase simply
invokes ninja, executes the steps generated by the configure process.
