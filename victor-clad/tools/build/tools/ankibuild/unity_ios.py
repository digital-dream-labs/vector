import os
import re
import shutil
import sys

import util

class UnityBuildPluginIOS(object):
    """Plugin for iOS-specific Unity build tasks"""

    def patch_register_mono_modules(self, input_io, output_io):
        """Patch RegisterMonoModules.cpp to work for Simulator builds"""
        
        stage = 'start'
        buf = []
        output = []

        for line in input_io:
            if stage == 'start':
                if re.search('^\#if \!\(TARGET_IPHONE_SIMULATOR\)', line):
                    stage = 'header'
                    continue
            elif stage == 'header':
                buf.append(line)
                if re.search('mono_dl_register_symbol', line):
                    output.extend(buf)
                    output.append("#if !(TARGET_IPHONE_SIMULATOR)\n")
                    del buf[:]
                    stage = 'scan'
                    continue
            elif stage == 'end':
                if re.search('^\#endif', line):
                    stage = 'done'
                    continue
            elif stage == 'footer':
                if re.search('mono_dl_register_symbol', line):
                    output.append("#endif\n")
                    output.append("\n")
                    stage = 'end'
            elif (stage == 'scan'):
                if re.search('void RegisterMonoModules', line):
                    stage = 'footer'

            if len(buf) == 0:
                output.append(line)

        output_io.write(''.join(output))

    def post_build(self, unity_build):
        """Run after Unity build succeeds"""
        result = True

        # patch RegisterMonoModules.cpp to work for Simulator builds
        cpp_file = os.path.join(unity_build.build_config.build_dir, 'Libraries', 'RegisterMonoModules.cpp')
        cpp_file_in = "%s.in" % cpp_file

        is_patched = os.path.isfile(cpp_file_in) and util.File.is_up_to_date(cpp_file_in, cpp_file)

        if not is_patched:
            # mv src cpp to cpp.in 
            shutil.move(cpp_file, cpp_file_in)

            io_in = open(cpp_file_in, 'r')
            io_out = open(cpp_file, 'w')
            cursor_in = io_out.tell()
            self.patch_register_mono_modules(io_in, io_out)
            cursor_out = io_out.tell()
            io_in.close()
            io_out.close()

            if cursor_out == cursor_in:
                result = False

            # check for patch failure
            if not os.path.isfile(cpp_file):
                result = False

        return result

