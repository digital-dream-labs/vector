import os
import re
import shutil
import sys

import util
import xcodeProject

class UnityXcodeBuildPluginIOS(object):
    """Plugin for updating an XCode project from a Unity Build"""


    def post_build(self, unity_build):
        """Run after Unity build succeeds"""
        
        print(unity_build.build_config.destination_file)
        print(unity_build.build_config.build_dir)
        utility = xcodeProject.XcodeProjectUtility()
        utility.FixupIosUnityProject(unity_build.build_config.destination_file, unity_build.build_config.build_dir, "UnityBuild")

        return True

