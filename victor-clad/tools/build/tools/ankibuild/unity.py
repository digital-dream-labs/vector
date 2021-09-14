#!/usr/bin/env python2

from __future__ import print_function

import argparse
import errno
import fnmatch
import json
import md5
import os
import plistlib
import re
import socket
import subprocess
import sys
import time
import yaml
import hashlib

# ankibuild
import builder
import shutil
import util
import unity_ios
import unity_ios_xcode

class UnityApp(object):
    def __init__(self, unity_app_dir):
        self.app_dir = unity_app_dir

    def patch_unityengine_ui(self, patch_file_name):
        if self.bundle_version() != '5.6.0f3':
            # if Unity version is not 5.6.0f3, ignore
            return

        # md5 of original UnityEngine.UI.dll
        unityengine_ui_original_md5 = '89cd897feaae4f6fa9a6d02b91e78fa1'

        # md5 of patched UnityEngine.UI.dll
        unityengine_ui_patched_md5 = '1cc4e5cf2d4c8b0be67b2d6cb4358306'

        # file path to UnityEngine.UI.dll that needs to be patched
        # (.../Unity.app/Contents/UnityExtensions/Unity/GUISystem/Standalone/UnityEngine.UI.dll)
        file_name = os.path.join(self.app_dir, 'Contents', 'UnityExtensions', 'Unity', 'GUISystem', 'Standalone', 'UnityEngine.UI.dll')

        if os.path.isfile(file_name):
            # if the dll exists, read the contents and get the md5 hash
            # (this should never happen)
            with open(file_name) as f:
                contents = f.read()    
                md5_hash = hashlib.md5(contents).hexdigest()
        else:
            # otherwise, print a warning that the dll doesn't exist
            md5_hash = ''
            print('[UNITY PATCH] Warning! No Standalone/UnityEngine.UI.dll file found.')

        if md5_hash == unityengine_ui_patched_md5:
            # hashes match, so we are already patched, nothing further to do
            print('[UNITY PATCH] UnityEngine.UI.dll is patched to resolve 5.6.0f3 Unity JIT bug.')
            return
        else:
            # hashes don't match, so dll needs patching

            # patch file does not exist
            # (this should never happen)
            if not os.path.isfile(patch_file_name):
                print('[UNITY PATCH] Error! Patch is needed for UnityEngine.UI.dll, but no patch file was found.')
                return

            with open(patch_file_name) as f:
                contents = f.read()    
                md5_hash = hashlib.md5(contents).hexdigest()

                # verify that patched file is the expected patch
                if md5_hash == unityengine_ui_patched_md5:
                    # patched file is the correct file

                    # backup old UnityEngine.UI.dll and *.mdb file by adding '.backup'
                    os.rename(file_name, file_name + '.backup')
                    os.rename(file_name + '.mdb', file_name + '.mdb.backup')

                    # copy patched version to Unity app directory
                    shutil.copy2(patch_file_name, file_name)

                    # check to see if mdb patch file exists
                    # (it should!)
                    if os.path.isfile(patch_file_name + '.mdb'):
                        shutil.copy2(patch_file_name + '.mdb', file_name + '.mdb')
                    else:
                        print('[UNITY PATCH] Warning! Could not patch the UnityEngine.UI.dll.mdb debug file.')

                    print('[UNITY PATCH] Successfully patched UnityEngine.UI.dll to prevent JIT bug for Unity 5.6.0f3')
                else:
                    print('[UNITY PATCH] Error! Patched UnityEngine.UI.dll does not match expected hash. Won\'t patch.')

    def version(self):
        info_plist = os.path.join(self.app_dir, 'Contents', 'Info.plist')
        if not os.path.isfile(info_plist):
            return None

        get_version_cmd = [PLIST_BUDDY, '-c', "Print :CFBundleVersion", info_plist]
        bundle_version = subprocess.check_output(get_version_cmd)

        # truncate version to n.n.n
        m = re.search('(\d+\.\d+\.\d+)', bundle_version)
        version = m.group(1)

        return version

    def bundle_version(self):
        info_plist = os.path.join(self.app_dir, 'Contents', 'Info.plist')
        if not os.path.isfile(info_plist):
            return None

        info = plistlib.readPlist(info_plist)
        bundle_version = info.get('CFBundleVersion')
        return bundle_version

    def build_number(self):
        info_plist = os.path.join(self.app_dir, 'Contents', 'Info.plist')
        if not os.path.isfile(info_plist):
            return None

        get_build_number_cmd = [PLIST_BUDDY, '-c', "Print :UnityBuildNumber", info_plist]
        build_number = subprocess.check_output(get_build_number_cmd).strip()

        return build_number

    def get_version_info(self):
        version = self.version()
        build_number = self.build_number()
        return { 'version': version, 'build_number': build_number }


class UnityBuildConfig(object):

    @staticmethod
    def default_unity_exe():
        return os.path.join('/', 'Applications', 'Unity', 'Unity.app', 'Contents', 'MacOS', 'Unity')

    @staticmethod
    def default_log_file():
        return os.path.join(util.Git.repo_root(), 'build', 'Unity', 'UnityBuild.log')

    @staticmethod
    def get_unity_project_version(project_dir):
        project_version_path = os.path.join(project_dir, 'ProjectSettings', 'ProjectVersion.txt')
        version = None
        with open(project_version_path, 'r') as f:
            contents = f.read()
            m = re.search('(\d+\.\d+\.\d+[pf]?\d*)', contents)
            if m:
                version = m.group(1).strip()
        return version

    @staticmethod
    def find_unity_project(root_dir):

        # use unix find command to search for ProjectSettings.asset, because it is about
        # 4-5X faster than os.walk
        find_project_dir = None
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

        # slow- os.walk method
        #for root, dirnames, filenames in os.walk(root_dir):
        #    settings_file_matches = fnmatch.filter(filenames, 'ProjectSettings.asset')
        #    if settings_file_matches:
        #        project_dir = os.path.abspath(os.path.join(os.path.dirname(settings_file_matches[0])))
        #        break
        return project_dir


    def __init__(self):
        self.project_dir = None
        self.config = 'release'
        self.build_product = None
        self.platform = 'mac'
        self.build_dir = os.path.join(util.Git.repo_root(), 'build', 'Unity')
        self.sdk = 'macosx'
        self.log_file = os.path.join(self.build_dir, 'UnityBuild.log')
        self.force = False
        self.script_engine = 'mono2x'
        self.script_debugging = False
        self.script_profiling = False
        self.asset_destination_path = None
        self.build_type = 'PlayerAndAssets'
        self.build_number = '1'
        self.verbose = False
        self.unity_exe = UnityBuildConfig.default_unity_exe()
        self.destination_file = None
        self.sku = None

    def set_options(self, options):
        options_dict = vars(options)
        for k,v in options_dict.iteritems():
            if hasattr(self, k):
                setattr(self, k, v)

        if not options_dict.has_key('log_file'):
            self.log_file = os.path.join(os.path.dirname(self.build_dir), 'UnityBuild.log')

    def parse_arguments(self, argv):
        anki_builder = builder.Builder()

        parser = argparse.ArgumentParser(parents=[anki_builder.argument_parser()],
                                     description='Run a Unity build')
    

        parser.add_argument('--sdk', action="store", choices=('iphoneos', 'iphonesimulator'), default="iphoneos")
        parser.add_argument('--script-engine', action="store", choices=('mono2x', 'il2cpp'), default="mono2x")
        parser.add_argument('--build-type', action="store", choices=('PlayerAndAssets', 'OnlyAssets', 'OnlyPlayer'),
                            default="PlayerAndAssets", dest='build_type')
        parser.add_argument('--build-number', metavar='string', default='1', required=False,
                            help='Set the Android build number')
        parser.add_argument('--asset-destination-path', action="store",
                            default="Assets/StreamingAssets/cozmo_resources", dest='asset_destination_path',
                            help="where to copy assets to")
        parser.add_argument('--script-debugging', action="store_true", default=False,
                        help="Enable Mono script debugging in UnityEngine")
        parser.add_argument('--script-profiling', action="store_true", default=False,
                        help="Enable Mono script profiling in UnityEngine")
        parser.add_argument('--xcode-build-dir', action="store",
                        help="Path for Unity-generated Xcode project")
        parser.add_argument('--build-product', action="store",
                        help="Path to platform-specific Unity build product")
        parser.add_argument('--force', action='store_true', default=False, help='Build regardless of dependency checks.') 
        parser.add_argument('--with-unity', metavar="UNITY_EXE", action='store', 
                        dest='unity_exe',
                        default=UnityBuildConfig.default_unity_exe(),
                        help="Path to Unity executable")
        parser.add_argument('--sku',
                        action=builder.ArgParseUniqueStore,
                        help="SKU to be used during Unity asset build. Leave blank to build all assets")

        parser.add_argument('project_dir', action="store", help="path to Unity project")
        parser.add_argument('--xcode-destination-file', action="store", default=None, dest='destination_file', 
                        help="Path to xcode project file if we need to copy build results")

        parser.set_defaults(build_dir=os.path.join(util.Git.repo_root(), 'build', 'Unity'),
                            log_file=os.path.join(util.Git.repo_root(), 'build', 'Unity', 'UnityBuild.log'),
                            platform='mac',
                            config='release')
        options = parser.parse_args(argv)

        self.set_options(options)

class UnityBuild(object):
    """Wrapper class for building through Unity
    """

    def __init__(self, build_config):
        self.build_config = build_config
        self.plugins = []
        self._register_plugins()

    def _register_plugins(self):
        """Register plugins to perform specific build actions"""
        if self.build_config.platform == "ios" and self.build_config.destination_file is not None:
            self.plugins.append(unity_ios_xcode.UnityXcodeBuildPluginIOS())

    def check_log_file_for_autobuild_not_found(self, logFile):
        ret = subprocess.call(['egrep', '-q', "executeMethod class 'AutoBuild' could not be found.", logFile], cwd=".")
        return ret == 0

    def unity_source_files(self):
        # nearly everything can affect the build
        #src_patterns = [
        #    '*.cs',
        #    '*.unity',
        #    '*.prefab',
        #    '*.asset',
        #    '*.shader',
        #    '*.csproj',
        #    '*.unityproj'
        #]

        # We need to skip the ProjectSettings.asset file, since it can be
        # modified during the build.
        # Also skipping GraphicsSettings.asset, because it gets touched (but not modified)
        # per a Unity bug; waiting to upgrade for fix
        # https://issuetracker.unity3d.com/issues/projectsettings-slash-graphicssettings-dot-asset-changes-everytime-platform-is-changed-polluting-version-control
        excludes = set(['ProjectSettings.asset',
                        'EditorUserBuildSettings.asset',
                        'GraphicsSettings.asset',
                        '.DS_Store'])

        src_files = []
        for startFolder in ['Assets', 'ProjectSettings']:
            for root, dirnames, filenames in os.walk(os.path.join(self.build_config.project_dir, startFolder)):
                #for p in src_patterns:
                    #for filename in fnmatch.filter(filenames, p):
                for filename in filenames:
                    if filename in excludes:
                        continue
                    src_files.append(os.path.join(root, filename))

        return src_files

    def need_to_build(self):
        src_files = self.unity_source_files()
        build_product = self.build_config.build_product
        
        is_up_to_date = util.File.is_up_to_date(build_product, src_files)
        return (not is_up_to_date)

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

    def call_into_unity(self):
        message = ""
        # we don't need this, as unity will build currently opened project
        #message += '-projectPath ' + projectPath
        # we don't need this, as there is no way to redirect log after unity start up
        #message += " -logFile " + options.logFile
        if self.build_config.script_debugging:
            message += " --debug "
        # script profiling is believed to work only when script debugging is enabled (pauley)
        if self.build_config.script_profiling:
            message += " --profile "
        message += " --platform " + self.build_config.platform
        message += " --config " + self.build_config.config
        message += " --sdk " + self.build_config.sdk
        message += " --build-path " + self.build_config.build_dir
        message += " --script-engine " + self.build_config.script_engine
        message += " --asset-path " + self.build_config.asset_destination_path
        message += " --build-type " + self.build_config.build_type
        message += " --build-number " + self.build_config.build_number
        if self.build_config.sku is not None:
            message += " --sku " + self.build_config.sku
        # adding a space at the end of our message makes sure that our last arg does not
        # get filled with the tail of our message buffer
        message += " "

        logFilePath = os.path.expanduser('~/Library/Logs/Unity/Editor.log')
        handleUnityLog = open(logFilePath, 'r')
        handleUnityLog.seek(0, os.SEEK_END)

        unityOpen = True
        buildCompleted = False
        result = 0 # success
        try:
            server_address = ('localhost', 48888)
            sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            sock.connect(server_address)
            print('sending build request to unity %s' % message)
            sock.sendall(message)

            # now wait for response
            while 1:
                data = sock.recv(4096)
                if not data: break

                # Autobuild service will send the response code in the first byte
                if not buildCompleted:
                    result = ord(data[0])
                    data = data[1:]

                print("received data:", data)
                buildCompleted = True
                # no need to exit, as server will close the connection when done
        except socket.error as (socket_errno, msg):
            if (socket_errno == errno.ECONNREFUSED):
                unityOpen = False
                print("unity not started, running command line")
            else:
                print("error " + str(socket_errno) + " message: " + msg, file=sys.stderr)
        except Exception as e:
            print("Unexpected error:", e)

        # now copy the log data
        util.File.mkdir_p(os.path.dirname(self.build_config.log_file))
        handleBuildLog = open(self.build_config.log_file, 'w')
        logData = handleUnityLog.read()
        handleBuildLog.write(logData)
        handleUnityLog.close()
        handleBuildLog.close()
        return (unityOpen, buildCompleted, result, self.build_config.log_file)
                
    def run_unity_command_line(self):
        exe=self.build_config.unity_exe
        procArgs = [exe, "-batchmode", "-quit", "-nographics"]
        procArgs.extend(["-projectPath", self.build_config.project_dir])
        procArgs.extend(["-executeMethod", 'CommandLineBuild.Build'])
        procArgs.extend(["-logFile", self.build_config.log_file])
        procArgs.extend(["--platform", self.build_config.platform])
        procArgs.extend(["--config", self.build_config.config])
        procArgs.extend(["--sdk", self.build_config.sdk])
        procArgs.extend(["--build-path", self.build_config.build_dir])
        procArgs.extend(["--script-engine", self.build_config.script_engine])
        procArgs.extend(["--asset-path", self.build_config.asset_destination_path])
        procArgs.extend(["--build-type", self.build_config.build_type])
        procArgs.extend(["--build-number", self.build_config.build_number])
        if self.build_config.sku is not None:
            procArgs.extend(["--sku", self.build_config.sku])

        if self.build_config.script_debugging:
            procArgs.extend(["--debug"])
        # script profiling is believed to work only when script debugging is enabled (pauley)
        if self.build_config.script_profiling:
            procArgs.extend(["--profile"])
 
        projectPath = self.build_config.project_dir

        print(' '.join(procArgs))
        util.File.mkdir_p(self.build_config.build_dir)
        util.File.mkdir_p(os.path.dirname(self.build_config.log_file))
        ret = subprocess.call(procArgs, cwd=projectPath)
        print("ret = " + str(ret))

        # It was observed (see ANDRIVE-128), that a clean build may fail
        # due to a bug in Unity.  The marker of this failure is the AutoBuild class
        # not being found.
        # If this occurs, run the build one more time as a workaround
        if ret != 0 and self.check_log_file_for_autobuild_not_found(self.build_config.log_file):
            print('WARNING:  TRIGGERING A BUILD AGAIN IN run_unity_command_line')
            ret = subprocess.call(procArgs, cwd=projectPath)

        return (ret, self.build_config.log_file)

    def run(self):
        result = 0

        if not (self.build_config.force or self.need_to_build()):
            return result

        self.make_build_dir()
        util.File.mkdir_p(os.path.dirname(self.build_config.log_file))

        # try to call into unity
        (unityOpen, buildCompleted, result, logFilePath) = self.call_into_unity()

        # if unity is not open, run command line
        if (not unityOpen):
            (result, logFilePath) = self.run_unity_command_line()

        self.parse_log_file(logFilePath)

        if result != 0:
            return result

        # if we are only copying assets do not do other checks. just return result.
        if self.build_config.build_type == "OnlyAssets":
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

class UnityUtil(object):

    @staticmethod
    def get_created_time_from_meta_file(path, default=int(time.time())):
        if not os.path.isfile(path):
            return default
        ydata = {}
        try:
            with open(path, 'r') as yf:
                ydata = yaml.safe_load(yf)
        except:
            pass
        if "timeCreated" in ydata:
            return ydata["timeCreated"]
        return default

    @staticmethod
    def generate_meta_files(path, verbose=False, overwrite_existing=False):
        meta_file_template = """fileFormatVersion: 2
guid: %(path_md5)s
folderAsset: %(is_folder_asset)s
timeCreated: %(creation_time_secs)d
licenseType: Pro
DefaultImporter:
  userData:
  assetBundleName:
  assetBundleVariant:
"""
        for dir_name, subdir_list, file_list in os.walk(path):
            dir_rel_path=os.path.relpath(dir_name, os.path.join(path, '..'))

            dir_meta_filename = os.path.join(dir_name + '.meta')
            if not (overwrite_existing or os.path.exists(dir_meta_filename)):
                if(verbose):
                    print(dir_rel_path)
                meta_file_path=os.path.join(dir_name + '.meta')
                meta_file_path_tmp=os.path.join(dir_name + '.meta.tmp')
                create_time_secs = UnityUtil.get_created_time_from_meta_file(meta_file_path)
                with open(meta_file_path_tmp, 'w+') as dir_meta_file:
                    dir_meta_file.write(meta_file_template % { 
                        'path_md5': md5.new(dir_rel_path).hexdigest(),
                        'is_folder_asset': 'yes',
                        'creation_time_secs': create_time_secs
                    })
                util.File.update_if_changed(meta_file_path, meta_file_path_tmp)
            for file_name in [i for i in file_list if not re.match(r'.*\.meta$', i)]:
                file_path = os.path.join(dir_name, file_name)
                file_rel_path = os.path.join(dir_rel_path, file_name)
                meta_filename = file_path + '.meta'
                if(verbose):
                    print(file_rel_path)
                meta_file_path = file_path + '.meta'
                meta_file_path_tmp = file_path + '.meta.tmp'
                create_time_secs = UnityUtil.get_created_time_from_meta_file(meta_file_path)
                if not (overwrite_existing or os.path.exists(meta_filename)):
                    with open(meta_file_path_tmp, 'w+') as meta_file:
                        meta_file.write(meta_file_template % {
                            'path_md5': md5.new(file_rel_path).hexdigest(),
                            'is_folder_asset': 'no',
                            'creation_time_secs': create_time_secs
                        })
                    util.File.update_if_changed(meta_file_path, meta_file_path_tmp)


# def main():
if __name__ == '__main__':

    util.Profile.begin(__file__, sys.argv[1:])

    argv = sys.argv[1:]

    util.Profile.begin(__file__+"/UnityBuildConfig", sys.argv[1:])
    config = UnityBuildConfig()
    config.parse_arguments(argv)

    if config.verbose:
        util.ECHO_ALL = True

    if not config.project_dir:
        # Search for unity project- pick the first one.
        default_project_dir = UnityBuildConfig.find_unity_project(util.Git.repo_root())
        if default_project_dir is None:
            print("Error: Could not find a Unity project")
            sys.exit(1)
        print(default_project_dir)
        config.project_dir = default_project_dir

    # platform-dependent build_product
    if not config.build_product:
        if config.platform == 'ios':
            config.build_product = os.path.join(config.build_dir, 'Unity-iPhone.xcodeproj')
        elif config.platform == 'mac':
            config.build_product = os.path.join(config.build_dir, 'UnityPlayerOSX.app')
        elif config.platform == 'android':
            # Note this path is not used for cozmo
            config.build_product = os.path.join(config.build_dir, 'OverDrive/libs/unity-classes.jar')

    if (config.platform == 'mac'):
        config.build_dir = os.path.join(config.build_dir, 'UnityPlayerOSX.app')

    util.Profile.end(__file__+"/UnityBuildConfig", sys.argv[1:])

    util.Profile.begin(__file__+"/UnityBuild", sys.argv[1:])
    builder = UnityBuild(config)

    ret = builder.run()
    util.Profile.end(__file__+"/UnityBuild", sys.argv[1:])

    util.Profile.end(__file__, sys.argv[1:])

    if ret == 0:
        print("Unity compile completed correctly.")
    else:
        print("UNITY COMPILE ERROR", file=sys.stderr)
        sys.exit(ret)

    sys.exit(ret)
