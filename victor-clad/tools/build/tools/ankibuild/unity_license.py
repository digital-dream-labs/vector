#!/usr/bin/env python2

from __future__ import print_function

import fnmatch
import os
import subprocess
import sys

import util

class UnityLicense(object):
    """Encapsulate Unity License info & method to activate/deactivate a license key"""

    @staticmethod
    def license_path():
        return os.path.join('/', 'Library', 'Application Support', 'Unity')

    def __init__(self, license_key):
        self.license_key = license_key

    def dump_error_log(self):
        editor_log = os.path.join(os.environ['HOME'], 'Library', 'Logs', 'Unity', 'Editor.log')
        with open(editor_log, 'r') as log:
            log_data = log.read()
            print(log_data, file=sys.stderr)

    def has_license(self):
        license_path = UnityLicense.license_path()

        has_license = False
        if not os.path.isdir(license_path):
            return has_license

        # Search for a file named 'Unity_v5.x.ulf'
        for filename in os.listdir(license_path):
            if fnmatch.fnmatch(filename, 'Unity_v5.x.ulf'):
                has_license = True

        return has_license

    def activate(self, unity_exe):

        # If there is an active license, release it!
        if self.has_license():
            # This should not happen, print a warning
            print("[WARNING]: Attempting to activate Unity when a license already exists", file=sys.stderr)
            self.deactivate(unity_exe)

        proc_args = [unity_exe, '-quit', '-batchmode']
        proc_args.extend(['-serial', self.license_key])

        # create the license folder if it does not exist, this is required by Unity
        license_path = UnityLicense.license_path()
        util.File.mkdir_p(license_path)

        ret = subprocess.call(proc_args)
        if (ret != 0):
            self.dump_error_log()

        return ret

    def deactivate(self, unity_exe):
        # Calling deactivate will fail if there is no current license
        # Check for a license first before calling this method
        if not self.has_license():
            return 0

        proc_args = [unity_exe, '-quit', '-batchmode']
        proc_args.extend(['-returnlicense'])

        ret = subprocess.call(proc_args)
        if (ret != 0):
            self.dump_error_log()

        return ret


if __name__ == '__main__':
    import argparse

    parser = argparse.ArgumentParser(description='Unity License wrapper script')
    # commands
    subparsers = parser.add_subparsers(help='commands', dest='command')
    subparsers.add_parser('activate', help='Activate license')
    subparsers.add_parser('deactivate', help='Deactivate license')

    parser.add_argument('--license-key', action='store',
                        help="Unity License Key")
    parser.add_argument('--with-unity', action='store',
                        default=os.path.join('/', 'Applications', 'Unity', 'Unity.app', 'Contents', 'MacOS', 'Unity'),
                        dest='unity_exe',
                        help="Path to Unity executable")
    
    
    options = parser.parse_args(sys.argv[1:])

    license = UnityLicense(options.license_key)

    ret = 0
    if (options.command == "activate"):
        ret = license.activate(options.unity_exe)
    elif (options.command == "deactivate"):
        ret = license.deactivate(options.unity_exe)

    if ret == 0:
        print("[SUCCESS] %s license" % options.command)
    else:
        print("[ERROR] %s license failed" % options.command)

    sys.exit(ret)
    
