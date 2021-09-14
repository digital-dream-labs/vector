#!/usr/bin/env python2
import sys
import argparse
import os
import subprocess
import time
import md5
import re
import logging
import textwrap

# ankibuild
import util

#set up default logger
UtilLog = logging.getLogger('generate.meta.files')
stdout_handler = logging.StreamHandler()
formatter = logging.Formatter('%(name)s - %(message)s')
stdout_handler.setFormatter(formatter)
UtilLog.addHandler(stdout_handler)

def parseArgs(scriptArgs):
  version = '1.0'
  parser = argparse.ArgumentParser(description='generates unity meta files for a given folder', version=version)
  parser.add_argument('--debug', '--verbose', '-d', dest='verbose', action='store_true',
                      help='prints debug output')
  parser.add_argument('--workPath', dest='workPath', action='store', required=True,
                      help='location of csharp files that need to be processed')
  (options, args) = parser.parse_known_args(scriptArgs)
  return options

def writeMeta(path, rel_path, is_folder):
    "Writes the specified contents to the given path."
    UtilLog.debug(rel_path)
    contents = textwrap.dedent("""
    fileFormatVersion: 2
    guid: {path_md5}
    folderAsset: {is_folder_asset}
    timeCreated: {creation_time_secs}
    licenseType: Pro
    DefaultImporter:
      userData:
      assetBundleName:
      assetBundleVariant:
    """).format(
        path_md5=md5.new(rel_path).hexdigest(),
        is_folder_asset=('yes' if is_folder else 'no'),
        # use constant creation time so it doesn't keep changing
        creation_time_secs=1430000000)
    util.File.write(path, contents)

def generateMetaFiles(path, verbose=False):
  path = path.rstrip('/')
  if verbose:
    UtilLog.setLevel(logging.DEBUG)
  else:
    UtilLog.setLevel(logging.INFO)
  UtilLog.debug('[CLAD] Generating .meta files...')
  for dir_name, subdir_list, file_list in os.walk(path):
    dir_rel_path=os.path.relpath(dir_name, os.path.join(path, '..'))
    writeMeta(dir_name + '.meta', dir_rel_path, True)
    
    for file_name in file_list:
      if not re.match(r'.*\.meta$', file_name):
        file_path = os.path.join(dir_name, file_name)
        file_rel_path = os.path.join(dir_rel_path, file_name)
        writeMeta(file_path + '.meta', file_rel_path, False)

if __name__ == '__main__':
  options = parseArgs(sys.argv)
  result = generateMetaFiles(options.workPath, options.verbose)
  if not result:
    sys.exit(1)
    
