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

GO = 'go'
DEFAULT_VERSION = '1.9.4'

def get_go_version_from_command(go_exe):
    version = None
    if go_exe and os.path.exists(go_exe):
        output = subprocess.check_output([go_exe, 'version'])
        if not output:
            return None
        m = re.match('^go version go(\d+\.\d+\.?\d*)', output)
        if not m:
            return None
        version = m.group(1)

    return version

def find_anki_go_exe(version):
    d = toolget.get_anki_tool_dist_directory(GO)
    d_ver = os.path.join(d, version)

    for root, dirs, files in os.walk(d_ver):
        if os.path.basename(root) == 'bin':
            if 'go' in files:
                return os.path.join(d_ver, root, 'go')

    return None

def install_go(version):
    platform_map = {
        'darwin': 'darwin-amd64',
        'linux': 'linux-amd64'
    }

    sha_map = {
        '1.9.4-darwin': '0e694bfa289453ecb056cc70456e42fa331408cfa6cc985a14edb01d8b4fec51',
        '1.9.4-linux': '15b0937615809f87321a457bb1265f946f9f6e736c563d6c5e0bd2c22e44f779'
    }

    platform_name = platform.system().lower()

    go_platform = platform_map[platform_name]
    go_archive_url = "https://dl.google.com/go/go{}.{}.tar.gz".format(version, go_platform)
    go_basename = "go-{}-{}".format(version, go_platform)
    go_downloads_path = toolget.get_anki_tool_downloads_directory(GO)
    go_dist_path = toolget.get_anki_tool_dist_directory(GO)
    go_hash = sha_map['{}-{}'.format(version, platform_name)]

    if go_hash is None:
        print("Error: Don't know hash for %s" % go_basename)
        sys.exit(1)

    toolget.download_and_install(go_archive_url,
                                 go_hash,
                                 go_downloads_path,
                                 go_dist_path,
                                 go_basename,
                                 "go",
                                 version,
                                 "go")

def find_or_install_go(required_ver, go_exe=None):
    if not go_exe:
        try:
            go_exe = subprocess.check_output(['which', 'go'])
        except subprocess.CalledProcessError as e:
            pass

    needs_install = True
    if go_exe:
        version = get_go_version_from_command(go_exe)
        if version == required_ver:
            needs_install = False

    if needs_install:
        go_exe = find_anki_go_exe(required_ver)
        version = get_go_version_from_command(go_exe)
        if version == required_ver:
            needs_install = False

    if needs_install:
        install_go(required_ver)
        return find_anki_go_exe(required_ver)
    else:
        return go_exe

def setup_go(required_ver):
    go_exe = find_or_install_go(required_ver)
    if not go_exe:
        raise RuntimeError("Could not find go version {0}"
                            .format(required_ver))
    return go_exe

def parseArgs(scriptArgs):
    version = '1.0'
    parser = argparse.ArgumentParser(description='finds or installs go', version=version)
    parser.add_argument('--install-go',
                        action='store',
                        dest='required_version',
                        nargs='?',
                        default=DEFAULT_VERSION)
    parser.add_argument('--check-version',
                        action='store',
                        dest='check_exe',
                        nargs=1)
    (options, args) = parser.parse_known_args(scriptArgs)
    return options


def main(argv):
    options = parseArgs(argv)
    if options.check_exe:
        exe = options.check_exe[0]
        checked_version = get_go_version_from_command(exe)
        if checked_version != DEFAULT_VERSION:
            print("Warning: custom Go installation at {} has version {}, desired is {}"
                .format(exe, checked_version, DEFAULT_VERSION))
        return 0
    if options.required_version:
        path = find_or_install_go(options.required_version)
        if not path:
            return 1
        print("%s" % path)
        return 0

if __name__ == '__main__':
    ret = main(sys.argv)
    sys.exit(ret)

