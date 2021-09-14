#!/usr/bin/env python2

from __future__ import print_function

import argparse
import ConfigParser
import hashlib
import io
import os
import pipes
import platform
import re
import shutil
import string
import subprocess
import sys
import urllib
import zipfile

# ankibuild
import util

def get_toplevel_directory():
    toplevel = None
    try:
        toplevel = util.Git.repo_root()
    except KeyError:
        script_path = os.path.dirname(os.path.realpath(__file__))
        toplevel = os.path.abspath(os.path.join(script_path, os.pardir, os.pardir))
    return toplevel

def parse_buckconfig_as_ini(path):
    ini_lines = []
    with open(path) as f:
        for ini_line in f:
            ini_lines.append(ini_line.strip())
    ini_data = '\n'.join(ini_lines)

    config = ConfigParser.RawConfigParser(allow_no_value=True)
    config.readfp(io.BytesIO(ini_data))

    return config

def get_property_from_buckconfig(path, section, key):
    if not path or not os.path.isfile(path):
        return None
    config = parse_buckconfig_as_ini(path)
    try:
        return config.get(section, key)
    except ConfigParser.NoSectionError:
        return None

def get_ndk_version_from_buckconfig(path):
    return get_property_from_buckconfig(path, 'ndk', 'ndk_version')

def get_sdk_version_from_buckconfig(path):
    return get_property_from_buckconfig(path, 'anki', 'android_sdk_version')

def read_properties_from_file(path):
    properties = {}
    if path and os.path.isfile(path):
        with open(path) as f:
            for line in f:
                name, var = line.partition("=")[::2]
                properties[name.strip()] = str(var.strip())
    return properties

def get_pkg_revision_from_source_properties(path):
    properties = read_properties_from_file(path)
    return properties["Pkg.Revision"]

def get_ndk_version_from_release_text(path):
    with open(path) as f:
        return f.readline().split()[0].split('-')[0]

def get_ndk_version_from_ndk_dir(path):
    if not path:
        return None
    source_properties_path = os.path.join(path, 'source.properties')
    release_txt_path = os.path.join(path, 'RELEASE.TXT')
    if os.path.isfile(source_properties_path):
        return get_pkg_revision_from_source_properties(source_properties_path)
    elif os.path.isfile(release_txt_path):
        return get_ndk_version_from_release_text(release_txt_path)
    return None

def get_sdk_version_from_sdk_dir(path):
    if not path:
        return None
    source_properties_path = os.path.join(path, 'anki', 'source.properties')
    if os.path.isfile(source_properties_path):
        return get_pkg_revision_from_source_properties(source_properties_path)
    return None

def get_android_sdk_ndk_dir_from_local_properties():
    path_to_local_properties = os.path.join(get_toplevel_directory(), 'local.properties')
    properties = read_properties_from_file(path_to_local_properties)
    return properties

def write_local_properties():
    path_to_local_properties = os.path.join(get_toplevel_directory(), 'local.properties')
    properties = get_android_sdk_ndk_dir_from_local_properties()
    properties['ndk.dir'] = os.environ['ANDROID_NDK_ROOT']
    properties['sdk.dir'] = os.environ['ANDROID_HOME']
    with open(path_to_local_properties, 'wb') as f:
        for key, value in properties.items():
            f.write("{0}={1}\n".format(key,value))

def get_anki_android_directory():
    anki_android_dir = os.path.join(os.path.expanduser("~"), ".anki", "android")
    util.File.mkdir_p(anki_android_dir)
    return anki_android_dir

def get_anki_android_ndk_repository_directory():
    ndk_repo_directory_path = os.path.join(get_anki_android_directory(), 'ndk-repository')
    util.File.mkdir_p(ndk_repo_directory_path)
    return ndk_repo_directory_path

def get_anki_android_ndk_downloads_directory():
    ndk_download_directory_path = os.path.join(get_anki_android_directory(), 'ndk-downloads')
    util.File.mkdir_p(ndk_download_directory_path)
    return ndk_download_directory_path

def get_anki_android_sdk_repository_directory():
    sdk_repo_directory_path = os.path.join(get_anki_android_directory(), 'sdk-repository')
    util.File.mkdir_p(sdk_repo_directory_path)
    return sdk_repo_directory_path

def get_anki_android_sdk_downloads_directory():
    sdk_download_directory_path = os.path.join(get_anki_android_directory(), 'sdk-downloads')
    util.File.mkdir_p(sdk_download_directory_path)
    return sdk_download_directory_path

def find_ndk_root_for_ndk_version(version_arg):
    ndk_version_arg_to_version = {
        'r13b': '13.1.3345770',
        '13.1.3345770': '13.1.3345770',
        'r10e': 'r10e',
        'r14': '14.0.3770861',
        '14.0.3770861': '14.0.3770861',
        'r14b': '14.1.3816874',
        '14.1.3816874': '14.1.3816874',
        'r15b': '15.1.4119039',
        '15.1.4119039': '15.1.4119039',
    }
    version = ndk_version_arg_to_version[version_arg]
    ndk_root_env_vars = [
        'ANDROID_NDK_ROOT',
        'ANDROID_NDK_HOME',
        'ANDROID_NDK',
        'NDK_ROOT'
    ]
    for env_var in ndk_root_env_vars:
        env_var_value = os.environ.get(env_var)
        if env_var_value:
            ndk_ver = get_ndk_version_from_ndk_dir(env_var_value)
            if ndk_ver == version:
                return env_var_value
    local_properties = get_android_sdk_ndk_dir_from_local_properties()
    ndk_dir = local_properties.get('ndk.dir')
    if ndk_dir:
        ndk_ver = get_ndk_version_from_ndk_dir(ndk_dir)
        if ndk_ver == version:
            return ndk_dir

    # Check to see if it is installed via homebrew
    brew_path = os.path.join(os.sep, 'usr', 'local', 'opt', 'android-ndk')
    ndk_ver = get_ndk_version_from_ndk_dir(brew_path)
    if ndk_ver == version:
        return brew_path

    # Check to see if it is installed via Android Studio
    studio_ndk_path = os.path.join(os.path.expanduser("~"), "Library", "Android", "sdk", "ndk-bundle")
    ndk_ver = get_ndk_version_from_ndk_dir(studio_ndk_path)
    if ndk_ver == version:
        return studio_ndk_path

    ndk_repo_path = os.environ.get('ANDROID_NDK_REPOSITORY')
    if not ndk_repo_path:
        ndk_repo_path = get_anki_android_ndk_repository_directory()
        os.environ['ANDROID_NDK_REPOSITORY'] = ndk_repo_path
    if ndk_repo_path and os.path.isdir(ndk_repo_path):
        for d in os.listdir(ndk_repo_path):
            full_path_for_d = os.path.join(ndk_repo_path, d)
            if os.path.isdir(full_path_for_d):
                ndk_ver = get_ndk_version_from_ndk_dir(full_path_for_d)
                if ndk_ver == version:
                    return full_path_for_d
    return None

def find_android_home_for_sdk_version(version):
    android_home_env_vars = [
        'ANDROID_HOME',
        'ANDROID_ROOT',
    ]
    for env_var in android_home_env_vars:
        env_var_value = os.environ.get(env_var)
        if env_var_value:
            sdk_ver = get_sdk_version_from_sdk_dir(env_var_value)
            if sdk_ver == version:
                return env_var_value
    local_properties = get_android_sdk_ndk_dir_from_local_properties()
    sdk_dir = local_properties.get('sdk.dir')
    if sdk_dir:
        sdk_ver = get_sdk_version_from_sdk_dir(sdk_dir)
        if sdk_ver == version:
            return sdk_dir
    sdk_repo_path = os.environ.get('ANDROID_SDK_REPOSITORY')
    if not sdk_repo_path:
        sdk_repo_path = get_anki_android_sdk_repository_directory()
        os.environ['ANDROID_SDK_REPOSITORY'] = sdk_repo_path
    if sdk_repo_path and os.path.isdir(sdk_repo_path):
        for d in os.listdir(sdk_repo_path):
            full_path_for_d = os.path.join(sdk_repo_path, d)
            if os.path.isdir(full_path_for_d):
                sdk_ver = get_sdk_version_from_sdk_dir(full_path_for_d)
                if sdk_ver == version:
                    return full_path_for_d

    return None

def sha1sum(filename):
    sha1 = hashlib.sha1()
    with open(filename, 'rb') as f:
        for chunk in iter(lambda: f.read(65536), b''):
            sha1.update(chunk)
    return sha1.hexdigest()

def dlProgress(bytesWritten, totalSize):
    percent = int(bytesWritten*100/totalSize)
    sys.stdout.write("\r" + "  progress = %d%%" % percent)
    sys.stdout.flush()

def safe_rmdir(path):
    if os.path.isdir(path):
        shutil.rmtree(path)

def safe_rmfile(path):
    if os.path.isfile(path):
        os.remove(path)

def download_and_install_zip(url,
                             downloads_path,
                             repo_path,
                             base_name,
                             info,
                             version,
                             title):
    tmp_extract_path = os.path.join(downloads_path, base_name)
    final_extract_path = os.path.join(repo_path, base_name)
    safe_rmdir(tmp_extract_path)
    safe_rmdir(final_extract_path)
    final_path = tmp_extract_path + ".zip"
    download_path = final_path + ".tmp"
    safe_rmfile(final_path)
    safe_rmfile(download_path)
    download_size = info.get(version).get('size')
    download_hash = info.get(version).get('sha1')
    handle = urllib.urlopen(url)
    code = handle.getcode()
    if code >= 200 and code < 300:
        download_file = open(download_path, 'w')
        block_size = 1024 * 1024
        sys.stdout.write("\nDownloading {0} {1}:\n  url = {2}\n  dst = {3}\n"
                         .format(title, version, url, final_path))
        for chunk in iter(lambda: handle.read(block_size), b''):
            download_file.write(chunk)
            dlProgress(download_file.tell(), download_size)

        download_file.close()
        sys.stdout.write("\n")
        sys.stdout.write("Verifying that SHA1 hash matches {0}\n".format(download_hash))
        if sha1sum(download_path) == download_hash:
            os.rename(download_path, final_path)
            zip_ref = zipfile.ZipFile(final_path, 'r')
            sys.stdout.write("Extracting {0} from {1}.  This could take several minutes.\n"
                             .format(title, final_path))
            zip_ref.extractall(downloads_path)
            zip_info_list = zip_ref.infolist()
            # Fix permissions so we can execute the tools
            for zip_info in zip_info_list:
                extracted_path = os.path.join(downloads_path, zip_info.filename)
                perms = zip_info.external_attr >> 16L
                os.chmod(extracted_path, perms)
            zip_ref.close()
            os.rename(tmp_extract_path, final_extract_path)
        else:
            sys.stderr.write("Failed to validate {0} downloaded from {1}\n"
                             .format(download_path, url))
    else:
        sys.stderr.write("Failed to download {0} {1} from {2} : {3}\n"
                         .format(title, version, url, code))
    safe_rmfile(final_path)
    safe_rmfile(download_path)

def install_ndk(revision):
    ndk_revision_to_version = {
        '13.1.3345770': 'r13b',
        'r10e': 'r10e',
        'r13b': 'r13b',
        '14.0.3770861': 'r14',
        'r14' : 'r14',
        '14.1.3816874': 'r14b',
        'r14b': 'r14b',
        '15.1.4119039': 'r15b',
        'r15b': 'r15b',
    }

    ndk_info_darwin = {
        'r15b': {'size': 959321525, 'sha1': '05e3eec7e9ce1d09bb5401b41cf778a2ec19c819'},
        'r14b': {'size': 824705073, 'sha1': '2bf582c43f6da16416e66203d158a6dfaba4277c'},
        'r14': {'size': 824579088, 'sha1': 'd121c9e4f359ff65fb4d003bdd7dbe5dd9cf7295'},
        'r13b': {'size': 665967997, 'sha1': '71fe653a7bf5db08c3af154735b6ccbc12f0add5'},
        'r10e': {'size': 1083342126, 'sha1': '6be8598e4ed3d9dd42998c8cb666f0ee502b1294'},
    }

    ndk_info_linux = {
        'r15b': {'size': 974035125, 'sha1': '2690d416e54f88f7fa52d0dcb5f539056a357b3b'},
        'r14b': {'size': 840626594, 'sha1': 'becd161da6ed9a823e25be5c02955d9cbca1dbeb'},
        'r14': {'size': 840507097, 'sha1': 'eac8b293054671555cb636e350f1a9bc475c8f0c'},
        'r13b': {'size':687311866, 'sha1': '0600157c4ddf50ec15b8a037cfc474143f718fd0'},
        'r10e': {'size':1110915721, 'sha1': 'f692681b007071103277f6edc6f91cb5c5494a32'},
    }

    ndk_platform_info = {
        'darwin': ndk_info_darwin,
        'linux': ndk_info_linux,
    }
    platform_name = platform.system().lower()

    ndk_info = ndk_platform_info.get(platform_name)

    ndk_repo_path = os.environ.get('ANDROID_NDK_REPOSITORY')
    if not ndk_repo_path:
        ndk_repo_path = get_anki_android_ndk_repository_directory()
    util.File.mkdir_p(ndk_repo_path)

    ndk_downloads_path = get_anki_android_ndk_downloads_directory()

    version = ndk_revision_to_version[revision]
    url_prefix = "https://dl.google.com/android/repository/"
    ndk_base_name = "android-ndk-{0}".format(version)
    android_ndk_url = "{0}{1}-{2}-x86_64.zip".format(url_prefix, ndk_base_name, platform_name)

    ndk_tmp_extract_path = os.path.join(ndk_downloads_path, ndk_base_name)
    ndk_final_extract_path = os.path.join(ndk_repo_path, ndk_base_name)
    download_and_install_zip(android_ndk_url,
                             ndk_downloads_path,
                             ndk_repo_path,
                             ndk_base_name,
                             ndk_info,
                             version,
                             "Android NDK")

def install_sdk(version):
    sdk_info_darwin = {
        'r1': {'size': 841831972, 'sha1': 'cff532765b5d4b9abd83e322359a5d59f36c5960'},
        'r2': {'size': 916951631, 'sha1': '480d5d6006708c6a404bf4459d43edb59c932dcc'},
        'r3': {"size": 936119150, "sha1": '57805391ddb7040f43d891ce7e559b3f50d07a9c'},
    }

    sdk_info_linux = {
        'r1': {'size': 920378038, 'sha1': 'e4287400a8d15169e363b87a3c5c8656f2de6671'},
    }

    sdk_platform_info = {
        'darwin': sdk_info_darwin,
        'linux': sdk_info_linux,
    }

    platform_name = platform.system().lower()

    sdk_info = sdk_platform_info.get(platform_name)

    sdk_repo_path = os.environ.get('ANDROID_SDK_REPOSITORY')
    if not sdk_repo_path:
        sdk_repo_path = get_anki_android_sdk_repository_directory()
    util.File.mkdir_p(sdk_repo_path)

    sdk_downloads_path = get_anki_android_sdk_downloads_directory()

    platform_name_to_os_name = {
        'darwin': 'macosx',
        'linux': 'linux',
    }
    os_name = platform_name_to_os_name.get(platform_name)
    url_prefix = "https://sai-general.s3.amazonaws.com/build-assets/"
    sdk_base_name = "android-sdk-{0}-anki-{1}".format(os_name, version)
    android_sdk_url = "{0}{1}.zip".format(url_prefix, sdk_base_name)

    download_and_install_zip(android_sdk_url,
                             sdk_downloads_path,
                             sdk_repo_path,
                             sdk_base_name,
                             sdk_info,
                             version,
                             "Android SDK")

def get_required_ndk_version():
    buck_config_path = os.path.join(get_toplevel_directory(), '.buckconfig')
    required_ndk_ver = get_ndk_version_from_buckconfig(buck_config_path)
    return required_ndk_ver

def get_required_sdk_version():
    buck_config_path = os.path.join(get_toplevel_directory(), '.buckconfig')
    required_sdk_ver = get_sdk_version_from_buckconfig(buck_config_path)
    return required_sdk_ver

def find_ndk_root_dir(required_ndk_ver):
    ndk_root_dir = find_ndk_root_for_ndk_version(required_ndk_ver)
    return ndk_root_dir

def find_or_install_ndk(required_ndk_ver):
    ndk_root_dir = find_ndk_root_dir(required_ndk_ver)
    if not ndk_root_dir:
        install_ndk(required_ndk_ver)
        ndk_root_dir = find_ndk_root_dir(required_ndk_ver)
        if not ndk_root_dir:
            raise RuntimeError("Could not find Android NDK Root directory for version {0}"
                               .format(required_ndk_ver))
    return ndk_root_dir

def find_or_install_sdk(required_sdk_ver):
    android_home = find_android_home_for_sdk_version(required_sdk_ver)
    if not android_home:
        install_sdk(required_sdk_ver)
        android_home = find_android_home_for_sdk_version(required_sdk_ver)
        if not android_home:
            raise RuntimeError("Could not find Android HOME directory for version {0}"
                               .format(required_sdk_ver))
    return android_home

def setup_android_ndk_and_sdk(override_ndk_dir, override_android_home_dir):
    ndk_root_dir = override_ndk_dir
    if not ndk_root_dir:
        required_ndk_ver = get_required_ndk_version()
        ndk_root_dir = find_or_install_ndk(required_ndk_ver)

    os.environ['ANDROID_NDK_ROOT'] = ndk_root_dir
    print("ANDROID_NDK_ROOT: %s" % ndk_root_dir)

    android_home_dir = override_android_home_dir
    if not android_home_dir:
        required_sdk_ver = get_required_sdk_version()
        android_home_dir = find_or_install_sdk(required_sdk_ver)
    os.environ['ANDROID_HOME'] = android_home_dir
    print("ANDROID_HOME: %s" % android_home_dir)
    write_local_properties()

def get_path_to_unity_editor_plist():
    return os.path.expanduser("~/Library/Preferences/com.unity3d.UnityEditor5.x.plist")

def set_android_sdk_root_for_unity(override_android_home_dir):
    android_home = override_android_home_dir
    if not android_home:
        android_home = find_android_home_for_sdk_version(get_required_sdk_version())
    cmd = ["defaults",
           "write",
           get_path_to_unity_editor_plist(),
           "AndroidSdkRoot",
           android_home]
    subprocess.call(cmd)

def set_java_home_for_unity():
    java_home = util.File.evaluate("/usr/libexec/java_home")
    cmd = ["defaults",
           "write",
           get_path_to_unity_editor_plist(),
           "JdkPath",
           java_home]
    subprocess.call(cmd)

def parseArgs(scriptArgs):
    version = '1.0'
    parser = argparse.ArgumentParser(description='finds or installs android ndk/sdk', version=version)
    parser.add_argument('--install-ndk',
                        action='store',
                        dest='required_ndk_version',
                        nargs='?',
                        default=get_required_ndk_version(),
                        const=get_required_ndk_version())
    parser.add_argument('--install-sdk',
                        action='store',
                        dest='required_sdk_version',
                        nargs='?',
                        default=None,
                        const=get_required_sdk_version())
    (options, args) = parser.parse_known_args(scriptArgs)
    return options


def main(argv):
    options = parseArgs(argv)
    if options.required_sdk_version:
        sdk_path = find_or_install_sdk(options.required_sdk_version)
        if not sdk_path:
            return 1
        print("%s" % sdk_path)
        return 0
    if options.required_ndk_version:
        ndk_path = find_or_install_ndk(options.required_ndk_version)
        if not ndk_path:
            return 1
        print("%s" % ndk_path)
        return 0

if __name__ == '__main__':
    ret = main(sys.argv)
    sys.exit(ret)

