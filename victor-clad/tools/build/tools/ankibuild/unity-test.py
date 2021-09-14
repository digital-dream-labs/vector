#!/usr/bin/env python2

from __future__ import print_function

import argparse
import os
import re
import subprocess
import sys

# anki
import builder
import util


class UnityBuildConfig(object):

    @staticmethod
    def default_unity_exe():
        return os.path.join('/', 'Applications', 'Unity', 'Unity.app', 'Contents', 'MacOS', 'Unity')

    @staticmethod
    def find_unity_project(root_dir):

        try:
            find_project_dir = subprocess.check_output("find %s -iname 'ProjectSettings.asset'" % root_dir, shell=True)
        except CalledProcessError:
            find_project_dir = None

        project_dir = None
        if (find_project_dir is not None) and (len(find_project_dir) > 0):
            first_dir = os.path.dirname(find_project_dir.split("\n")[0])
            project_dir = os.path.abspath(os.path.join(first_dir, '..'))
        else:
            project_dir = None

        return project_dir

    def __init__(self):
        self.project_dir = None
        self.build_dir = os.path.join(util.Git.repo_root(), 'build', 'Unity')
        self.log_file = os.path.join(self.build_dir, 'UnityTest.log')
        self.unity_exe = UnityBuildConfig.default_unity_exe()

    def set_options(self, options):
        options_dict = vars(options)
        for k,v in options_dict.iteritems():
            if hasattr(self, k):
                setattr(self, k, v)

        if not options_dict.has_key('log_file'):
            self.log_file = os.path.join(os.path.dirname(self.build_dir), 'UnityTest.log')

    def parse_arguments(self, argv):
        anki_builder = builder.Builder()

        parser = argparse.ArgumentParser(parents=[anki_builder.argument_parser()],
                                     description='Run Unity Integration Tests')

        parser.add_argument('--unity-exe', action="store", help="path to Unity")
        parser.add_argument('project_dir', action="store", help="path to Unity project")

        parser.set_defaults(build_dir=os.path.join(util.Git.repo_root(), 'build', 'Unity'),
                            log_file=os.path.join(util.Git.repo_root(), 'build', 'Unity', 'UnityTest.log'))
        options = parser.parse_args(argv)
        self.set_options(options)

class UnityBuild(object):
    """Wrapper class for building through Unity
    """

    def __init__(self, build_config):
        self.build_config = build_config
        self.plugins = []

    def print_compile_error(self, line):
        """Prints a GCC style/Xcode-friendly error line
            <filename>:<linenumber>: error | warn | note : <message>\n
        """
        regex = re.compile(r"(.+)\((\d+),(\d+)\): (error|warn) .*: (.+)", re.I)

        match = regex.match(line.strip())

        if match is None:
            print(line, file=sys.stderr)
            return

        path = match.group(1)
        line_num = match.group(2)
        line_char = match.group(3)
        msg_type = match.group(4)
        msg = match.group(5)

        full_path = os.path.join(self.build_config.project_dir, path)

        formatted_msg = "%s:%s: %s : %s\n" % (full_path, line_num, msg_type, msg)
        print(formatted_msg, file=sys.stderr)

    def parse_log_file(self, logFile):
        lines = [line.strip() for line in open(logFile)]
        reerror = re.compile(r"(error)|(warn)", re.I)
        reCompileStart = re.compile(r"-----CompilerOutput:-stderr----------", re.I)
        reCompileEnd = re.compile(r"-----EndCompilerOutput---------------", re.I)
        hasCompileOut = filter(lambda x:reCompileStart.search(x), lines)
        inCompilerOutSection = False
        for line in lines:
            if reCompileEnd.search(line):
                inCompilerOutSection = False
                break

            if hasCompileOut:
                if inCompilerOutSection:
                    self.print_compile_error(line)
            else:
                if reerror.search(line):
                    self.print_compile_error(line)

            if reCompileStart.search(line):
                inCompilerOutSection = True

    def make_build_dir(self):
        build_root = os.path.dirname(self.build_config.build_dir)
        if not os.path.exists(build_root):
            os.makedirs(build_root)

    def run_smoketest_command_line(self):
        exe = self.build_config.unity_exe
        procArgs = [exe, "-batchmode"]
        procArgs.extend(["-projectPath", self.build_config.project_dir])
        procArgs.extend(["-executeMethod", 'Anki.TestScenario.RunTest'])
        procArgs.extend(["-logFile", self.build_config.log_file])

        projectPath = self.build_config.project_dir

        print(' '.join(procArgs))
        util.File.mkdir_p(self.build_config.build_dir)
        util.File.mkdir_p(os.path.dirname(self.build_config.log_file))
        ret = subprocess.call(procArgs, cwd=projectPath)
        print("ret = " + str(ret))

        return (ret, self.build_config.log_file)

    def run(self):
        result = 0

        self.make_build_dir()
        util.File.mkdir_p(os.path.dirname(self.build_config.log_file))

        (result, logFilePath) = self.run_smoketest_command_line()

        self.parse_log_file(logFilePath)

        if result != 0:
            return result

        # Make sure a Build directory was created, otherwise fail the build
        if result == 0:
            result = 0 if os.path.isdir(self.build_config.build_dir) else 2
            if result != 0:
                print("'" + self.build_config.build_dir + "' does not exist")
                return result

        for plugin in self.plugins:
            if hasattr(plugin, 'post_build'):
                plugin_success = plugin.post_build(self)
                if not plugin_success:
                    return 1

        return result

# def main():
if __name__ == '__main__':

    argv = sys.argv[1:]

    config = UnityBuildConfig()
    config.parse_arguments(argv)

    if not config.project_dir:
        # Search for unity project- pick the first one.
        default_project_dir = UnityBuildConfig.find_unity_project(util.Git.repo_root())
        if default_project_dir is None:
            print("Error: Could not find a Unity project")
            sys.exit(1)
        print(default_project_dir)
        config.project_dir = default_project_dir

    config.build_dir = os.path.join(config.build_dir, 'UnityPlayerOSX.app')

    builder = UnityBuild(config)

    ret = builder.run()

    if ret == 0:
        print("Unity test runner completed correctly.")
    else:
        print("UNITY TEST RUNNER ERROR", file=sys.stderr)
        sys.exit(ret)

    sys.exit(ret)
