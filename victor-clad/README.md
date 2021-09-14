# Victor CLAD

The SDK CLAD is everything that will be exposed from Victor to Apps or SDK Users. For an explanation of SDK Users, and the general purpose of this directory, see [this page](docs/SDK/Victor+SDK+Design).

## Getting Started

For anyone in the `victor` repo, this code should be pulled in directly by CMake, so you won't have to do anything extra. This is achieved by including the `CMakeLists.txt` file into a top-level CMake file in Victor. Any other repo using the sdk clad may do the same thing. The only thing you need to specify is the output location of the generated clad files with the `CLAD_OUTPUT_DIR_CPP`, `CLAD_OUTPUT_DIR_CSHARP` or `CLAD_OUTPUT_DIR_PYTHON` CMake Variables.

_Example CMake Inclusion:_
``` cmake
set(CLAD_VICTOR_EMIT_PYTHON ON CACHE BOOL "Turn on generation of python output")
set(CLAD_VICTOR_EMIT_CSHARP ON CACHE BOOL "Turn on generation of c# output")
add_subdirectory(victor-clad)
```

For repos that don't build with cmake you can use the configure.py script to specify what languages to build, and those files will be output under the `generated` directory at the root of this repo. 

_Example Configure Script Usage:_
``` bash
./configure.py --cpp --python --csharp # will build c++, python and charp libraries
```

## Considerations

There are a few things to be aware of when modifying this repo

- The CLAD messages in this repo should _only_ be the ones exposed outside of the robot.
- When a change is made it must be tested with both the Victor repo and just the `configure.py` script to build.
- Changes should be made in the Victor repo and pushed up through the victor-clad repo then pulled down to chewie. Victor is the main owner of this interface.
