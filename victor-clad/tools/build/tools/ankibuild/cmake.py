#!/usr/bin/env python2

from __future__ import print_function

import argparse
import os
import platform
import re
import string
import subprocess
import sys

# ankibuild
import toolget

CMAKE = 'cmake'

def get_cmake_version_from_command(cmake_exe):
    version = None
    if cmake_exe and os.path.exists(cmake_exe):
        output = subprocess.check_output([cmake_exe, '--version'])
        if not output:
            return None
        m = re.match('^cmake version (\d+\.\d+\.\d+)', output)
        if not m:
            return None
        version = m.group(1)

    return version

def find_anki_cmake_exe(version):
    d = toolget.get_anki_tool_dist_directory(CMAKE)
    d_ver = os.path.join(d, version)

    for root, dirs, files in os.walk(d_ver):
        if os.path.basename(root) == 'bin':
            if 'cmake' in files:
                return os.path.join(d_ver, root, 'cmake') 

    return None

def install_cmake(version):
    platform_map = {
        'darwin': 'Darwin-x86_64',
        'linux': 'Linux-x86_64'
    }

    platform_name = platform.system().lower()

    (major, minor, patch) = version.split('.')
    cmake_short_ver = "{}.{}".format(major, minor)
    cmake_url_prefix = "https://cmake.org/files/v{}".format(cmake_short_ver)

    cmake_platform = platform_map[platform_name]
    cmake_basename = "cmake-{}-{}".format(version, cmake_platform)
    cmake_archive_url = "{}/{}.tar.gz".format(cmake_url_prefix, cmake_basename)
    cmake_hash_url = "{}/cmake-{}-SHA-256.txt".format(cmake_url_prefix, version)

    cmake_downloads_path = toolget.get_anki_tool_downloads_directory(CMAKE)
    cmake_dist_path = toolget.get_anki_tool_dist_directory(CMAKE)

    toolget.download_and_install(cmake_archive_url,
                                 cmake_hash_url,
                                 cmake_downloads_path,
                                 cmake_dist_path,
                                 cmake_basename,
                                 cmake_basename,
                                 version,
                                 "CMake")

def find_or_install_cmake(required_ver, cmake_exe=None):
    if not cmake_exe:
        try:
            cmake_exe = subprocess.check_output(['which', 'cmake'])
        except subprocess.CalledProcessError as e:
            pass
  
    needs_install = True
    if cmake_exe:
        version = get_cmake_version_from_command(cmake_exe)
        if version == required_ver:
            needs_install = False

    if needs_install:
        cmake_exe = find_anki_cmake_exe(required_ver)
        version = get_cmake_version_from_command(cmake_exe)
        if version == required_ver:
            needs_install = False
        
    if needs_install:
        install_cmake(required_ver)
        return find_anki_cmake_exe(required_ver)
    else:
        return cmake_exe

def setup_cmake(required_ver):
    cmake_exe = find_or_install_cmake(required_ver)
    if not cmake_exe:
        raise RuntimeError("Could not find cmake version {0}"
                            .format(required_ver))
    return cmake_exe

def parseArgs(scriptArgs):
    version = '1.0'
    parser = argparse.ArgumentParser(description='finds or installs android ndk/sdk', version=version)
    parser.add_argument('--install-cmake',
                        action='store',
                        dest='required_version',
                        nargs='?',
                        default="3.9.6")
    (options, args) = parser.parse_known_args(scriptArgs)
    return options


def main(argv):
    options = parseArgs(argv)
    if options.required_version:
        path = find_or_install_cmake(options.required_version)
        if not path:
            return 1
        print("%s" % path)
        return 0

if __name__ == '__main__':
    ret = main(sys.argv)
    sys.exit(ret)

