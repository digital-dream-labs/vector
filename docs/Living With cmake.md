# Living With cmake

Created by David Mudie Last updated Jan 23, 2018

Tips and tricks for people new to 'cmake' and how it is used in our build process.

##  Best Practices

Before you start writing cmake, please read some words of wisdom:

https://schneide.wordpress.com/2016/04/08/modern-cmake-with-target_link_libraries/

https://schneide.wordpress.com/2017/11/06/4-tips-for-better-cmake/

## Anki Practices
Use BUILD.in files to identify source and header files for each target.

Use anki_build_cxx_library instead of cmake add_library.  The anki_build macros provide support for BUILD.in declarations and ensure consistent build settings.

Use anki_build_cxx_executable instead of cmake add_executable.  The anki_build macros provide support for BUILD.in declarations and ensure consistent build settings.

Each "leaf target" (library or executable) should have its own directory, its own BUILD.in, and its own CMakeLists.txt.  This makes it easy to find the directives for building a target, and helps to keep cmake files down to a manageable size.  Exception: If you are building two versions of the same target (e.g. coretech_engine vs coretech_robot), it's OK to put both targets in the same directory.

Third-party libraries and precompiled executables should be defined in an include file (eg gtest.cmake) that can be shared between projects.

If declarations have to be repeated, they should probably go into an include file. Don't cut-and-paste the same declarations into many files.

## Formatting
We don't have a cmake style guide, but your cmake should follow the standard Anki format:  two-space indents, eight-space tab stops, and indent with space instead of tabs.

Put a comment block in front of each "leaf target":

```
CMakeLists.txt
#
# cti_planning
#
anki_build_cxx_library(cti_planning ${ANKI_SRCLIST_DIR})
...
```

If a directive can fit on one line, leave it as one line:

```
CMakeLists.txt
set(PLATFORM_LIBS ${FOUNDATION})
```

If a directive CAN'T fit on one line, break it up like this:

```
CMakeLists.txt
set(PLATFORM_LIBS
  ${FOUNDATION}
  ${OPENCV_LIBS}
  ${ROUTING_HTTP_LIBS}
  ${WEBOTS_LIBS}
)
```

## SHARED vs STATIC
When building for victor, we usually prefer shared libraries to static libraries so processes can share the same code segment. This is our default.

If you don't have a reason to override the default, just omit the keyword completely.

## PRIVATE vs PUBLIC vs INTERFACE
Use keyword PRIVATE for declarations that affect your library but NOT anything that depends on it. Downstream targets will NOT automatically inherit these properties.

Use keyword PUBLIC for declarations that affect your library AND anything that depends on it.  Downstream targets will automatically inherit these properties.

Use keyword INTERFACE for declarations that DO NOT affect your library but DO affect anything that depends on it. You probably won't need this when adding new libraries, but it's used for linking with third-party libraries and external header files.

## Related Pages

cmake home page: https://cmake.org/

cmake FAQ: https://cmake.org/Wiki/CMake_FAQ

cmake help: https://cmake.org/cmake/help/v3.9/

"The Ultimate Guide to Modern CMake": https://rix0r.nl/blog/2015/08/13/cmake-guide/

Victor Build System Walkthrough: https://github.com/anki/victor/blob/master/docs/build-system-walkthrough.md

