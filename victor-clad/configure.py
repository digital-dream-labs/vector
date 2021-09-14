#!/usr/bin/env python3

import os
import shutil
import sys
import argparse
from subprocess import call

def clean(path):
    global verbose
    if verbose:
        print("Cleaning {}".format(path))
    shutil.rmtree(path)

def gen_src_list_files(gen_src_dir):
    global verbose
    if not os.path.exists(gen_src_dir):
        os.makedirs(gen_src_dir)
    process = [sys.executable, "tools/build/tools/metabuild/metabuild.py"]
    if verbose:
        process.append("-v")    
    process.extend(["-o", gen_src_dir, "clad/BUILD.in"])
    if verbose:
        print("Running {}".format(process))
    return call(process)

def build(output_dir, useCpp, useCsharp, usePython):
    global verbose
    process = ["cmake", "-G", "Ninja", "-DCLAD_VICTOR_SKIP_LICENSE=ON"]
    process.append("-DCLAD_VICTOR_EMIT_CPP={}".format("ON" if useCpp else "OFF"))
    process.append("-DCLAD_VICTOR_EMIT_CSHARP={}".format("ON" if useCsharp else "OFF"))
    process.append("-DCLAD_VICTOR_EMIT_PYTHON={}".format("ON" if usePython else "OFF"))
    process.append("..")
    if verbose:
        print("Running {}".format(process))
    return call(process)

def configure(useCpp, useCsharp, usePython):
    # TODO: current dir
    global verbose, force_clean
    source_dir = os.path.dirname(os.path.realpath(__file__))
    os.chdir(source_dir)
    output_dir = os.path.join(source_dir, "generated")
    build_dir = os.path.join(source_dir, "_build")
    if force_clean:
        clean(output_dir)
        clean(build_dir)
    if not os.path.exists(output_dir):
        os.makedirs(output_dir)
    result = gen_src_list_files(os.path.join(output_dir, "cmake"))
    if result != 0:
        return False

    build_dir = os.path.join(source_dir, "_build")
    if not os.path.exists(build_dir):
        os.makedirs(build_dir)
    os.chdir(build_dir)
    result = build(output_dir, useCpp, useCsharp, usePython)
    if result != 0:
        return False

    process = ["cmake", "--build", "."]
    if verbose:
        print("Running {}".format(process))
    result = call(process)
    os.chdir(source_dir)
    return result is 0

def main():
    global verbose, force_clean
    parser = argparse.ArgumentParser(description='runs gyp to generate projects')
    parser.add_argument('-py', '--python', action='store_true', help='generate clad messages for use in python') # TODO: explain options
    parser.add_argument('--cpp', action='store_true', help='generate clad messages for use in c++')
    parser.add_argument('-cs', '--csharp', action='store_true', help='generate clad messages for use in c#')
    parser.add_argument('-v', '--verbose', action='store_true', help='show all commands as they get executed')
    parser.add_argument('-f', '--force', action='store_true', help='force rebuild')

    args = parser.parse_args()
    verbose, force_clean = args.verbose, args.force

    if not args.python and not args.cpp and not args.csharp:
        sys.exit('Must provide at least one language to output')

    return configure(args.cpp, args.csharp, args.python)

if __name__ == '__main__':
  if main():
    sys.exit(0)
  else:
    sys.exit(1)
