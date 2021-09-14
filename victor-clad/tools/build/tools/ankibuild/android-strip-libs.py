#!/usr/bin/env python2

import argparse
import os
import shutil
import subprocess
import sys

def parse_args(argv):
    parser = argparse.ArgumentParser(description="Copy & strip shared libraries")
    parser.add_argument('--ndk-toolchain',
                        action='store',
                        help="Path to Android NDK toolchain")
    parser.add_argument('--symbolicated-libs-dir',
                        action='store',
                        default='obj',
                        help="Directory to store unstripped libs prior to stripping")
    parser.add_argument('lib_files',
                        action='append',
                        default=[],
                        nargs='+',
                        help="Files to strip")


    
    options = parser.parse_args(argv[1:])
    return options

def move(src_lib, target_lib):
    """Move source lib to target lib preserving timestamps and file metadata"""
    dirname = os.path.dirname(target_lib)
    if not os.path.exists(dirname):
        os.makedirs(dirname)
    
    # Move using copy, followed by remove.
    # This ensures that we own the file to avoid problems with strip
    shutil.copy(src_lib, target_lib)
    shutil.copystat(src_lib, target_lib)
    os.remove(src_lib)

def strip(src_lib, stripped_lib, options):
    """Strip unneeded symbols (including debug symbols) from source lib in place.
       Return True on success, False is an error occurred.
    """
    strip_exe = os.path.join(options.ndk_toolchain, 'bin', 'arm-linux-androideabi-strip')
    cmd = [strip_exe, '-v', '-p', '--strip-unneeded', '-o', stripped_lib, src_lib]
    result = subprocess.call(cmd)
    return (result == 0)

def is_file_up_to_date(target, dep):
    if not os.path.exists(dep):
        return False
    if not os.path.exists(target):
        return False

    target_time = os.path.getmtime(target)
    dep_time = os.path.getmtime(dep)

    return target_time >= dep_time

def main(argv):
    options = parse_args(argv)

    if not options.lib_files:
        print "No files to strip"
        return False 

    success = True
    target_dir = options.symbolicated_libs_dir
    libs = options.lib_files[0]
    for src_lib in libs:
        basename = os.path.basename(src_lib)
        target_lib = os.path.join(target_dir, basename)
        if not is_file_up_to_date(target_lib, src_lib):
            print("[android-strip-libs] preserve symbols %s -> %s" % (src_lib, target_lib))
            move(src_lib, target_lib)
            print("[android-strip-libs] strip %s" % src_lib)
            success = strip(target_lib, src_lib, options)
            if not success:
                break

    return success

if __name__ == '__main__':
    result = main(sys.argv)
    if not result:
        sys.exit(1)
    else:
        sys.exit(0)
