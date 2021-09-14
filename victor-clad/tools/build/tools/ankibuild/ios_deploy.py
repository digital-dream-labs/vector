
"ios-deploy script running."

import os.path
import util
import sys
import textwrap

def _run(*args):
    output = util.File.evaluate(['which', 'ios-deploy'], ignore_result=True)
    if not output or output.isspace():
        sys.exit(textwrap.dedent('''\
            ERROR: ios-deploy not found. ios-deploy is used to run ios apps on device.
            To install, enter the following commands into the OSX terminal:
            brew install node
            npm install -g ios-deploy'''))
    _kill_processes()
    util.File.execute(['ios-deploy'] + list(args), ignore_result=True)

def _kill_processes():
    util.File.execute(['killall', 'lldb'], ignore_result=True)
    util.File.execute(['killall', 'ios-deploy'], ignore_result=True)

def install(app_path):
    # install the app without debugging
    _run('--bundle', app_path)
    #_run('--nostart', '--debug', '--bundle', app_path)

def uninstall(app_bundle_id):
    # just uninstall the app
    _run('--uninstall_only', '--bundle_id', app_bundle_id)

def debug(app_path):
    # install and run the app
    _run('--debug', '--bundle', app_path)
    #_run('--noinstall', '--debug', '--bundle', app_path)
    _kill_processes()

def noninteractive(app_path):
    _run('--noninteractive', '--debug', '--bundle', app_path)

def just_launch(app_path):
    # this currently does not work (installs, but does not run)
    _run('--justlaunch', '--debug', '--bundle', app_path)
    _kill_processes()
