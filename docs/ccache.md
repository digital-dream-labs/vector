# Faster builds with ccache

[ccache] is a C/C++ compiler cache.  It wraps calls to the compiler to cache results and detect previous compilation results.

[ccache]: https://ccache.samba.org/

## Installation

ccache versions >= `3.2.x` support `clang`.

### Mac

Install the latest version with `homebrew`

```
brew install ccache
```

## Using ccache

CMake will currently detect whether `ccache` is installed on your system and automatically start using it.  If you don't want to customize your setup, you don't have to do anything.

## Configuration

### Basic

The build system uses wrapper scripts:

- [cmake-launch-c.in](/project/build-scripts/cmake-launch-c.in)

- [cmake-launch-cxx.in](/project/build-scripts/cmake-launch-cxx.in)

Each of these contain the following settings:

```
export CCACHE_CPP2=true
export CCACHE_MAXSIZE=10G
export CCACHE_HARDLINK=true
```

All of these are optional, except for `CACHE_CPP2=true`, which is required for `clang`.

### Advanced

`ccache` creates a configuration file in `${HOME}/.ccache/ccache.conf` on your computer.  This file contains many settings that control ccache.  See the website for a [detailed description of config settings](https://ccache.samba.org/manual.html#_configuration_settings).

There are 2 important settings that you might want to change:

- `cache_dir`

If you're using hard links and you build code on a partition that is separate from the one where you home directory is located, you need to either disable hard links, or move the cache dir to the same partition where the build occurs so that ccache can hard link to the cache dir.

- `base_dir`

This can be set to a folder that is the parent of the root repo directory. This will cause `ccache` to replace absolute dirs in cached files with relative dirs to `base_dir`.  This allows sharing cached results with builds of the same project in different directories.  See the docs for a more thorough explanation of [compiling in different directories](https://ccache.samba.org/manual.html#_compiling_in_different_directories). By default, `base_dir` is set to the root of the repo, which should do the right thing in most cases.

## Debugging ccache

You can cause `ccache` to output a log file in order to determine the reasons for cache hits or misses. Export the following variable to your shell before building:

```
export CCACHE_LOGFILE=/tmp/ccache.log
```

## Disabling ccache

You can bypass `ccache` by exporting the following variable in your shell before building:

```
export CCACHE_DISABLE=1
```
