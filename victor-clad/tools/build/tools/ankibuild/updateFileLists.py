#!/usr/bin/env python2

import fnmatch
import os
import os.path
import re
import subprocess
import sys
import argparse
import logging

#set up default logger
UtilLog = logging.getLogger('fileLists')
stdout_handler = logging.StreamHandler()
formatter = logging.Formatter('%(name)s - %(message)s')
stdout_handler.setFormatter(formatter)
UtilLog.addHandler(stdout_handler)

class FileListGenerator(object):

  def __init__(self, options):
    if options.verbose:
      UtilLog.setLevel(logging.DEBUG)
    self.options = options
    assert(options.projectRoot)


  def processFolder(self, folders, outputFiles, exclude_patterns=[]):
    settings = {
      'knownFiles' : [],
      'foundFiles' : [],
      'filesToAdd' : [],
      'filesToRemove' : [],
      'files': []
    }
    for fileName in outputFiles:
      settings['files'].append([fileName,[]])

    # define fnmatch-style shell glob patterns to exclude.
    fn_exclude_patterns = [
      '.*',
      '~*',
      '*~',
      '*.swp',
      '*.orig',
      '*.lst',
      'version.h',
    ]

    fn_exclude_patterns += exclude_patterns

    # compile fnmatch patterns into regular expressions
    re_exclude_patterns = [re.compile(fnmatch.translate(p)) for p in fn_exclude_patterns]

    # read in known sources
    # for targetKey, targetSettings in settings.iteritems():
    # open each .lst file and read in files into the 'knownFiles' key
    for targetListFileName, targetList in settings['files']:
      # UtilLog.debug('reading files from ' + targetListFileName)
      
      filename = os.path.join(self.options.projectRoot, targetListFileName) 
      if os.path.exists(filename):
        with open(filename, 'r') as file:
          for line in file:
            settings['knownFiles'].append(line.rstrip("\r\n"))
            targetList.append(line.rstrip("\r\n"))

    for folder in folders:

      #verify folder exists
      fullFolder = os.path.join(self.options.projectRoot, folder)
      if not os.path.exists(fullFolder):
        UtilLog.error('folder does not exists ' + fullFolder)
        assert(os.path.exists(fullFolder))

      outputDir = os.path.dirname(os.path.join(self.options.projectRoot, targetListFileName))

      # find all c/cpp/mm files in sources
      for dirPath, dirNames, fileNames in os.walk(fullFolder):
        
        relativePath = os.path.relpath(dirPath, outputDir)

        # for every found file that matches c/cpp/mm
        for fileName in fileNames:
          # ignore files that match any of the exclude patterns
          is_ignored = any((exclude_re.match(fileName) is not None) for exclude_re in re_exclude_patterns)
          if is_ignored:
            #UtilLog.debug("ignoring file: %s" % fileName)
            continue

          # Test for excludes against (file path relative to project root)
          # This should correctly match against excluded path components (e.g. "engine/tools/*")
          relFileName = os.path.join(os.path.relpath(dirPath, self.options.projectRoot), fileName)
          is_ignored = any((exclude_re.match(relFileName) is not None) for exclude_re in re_exclude_patterns)
          if is_ignored:
            # UtilLog.debug("ignoring file: %s" % relFileName)
            continue

          fullFilePath = os.path.join(relativePath, fileName)

          # look through 'knownFiles'
          foundInTarget = ''
          if (fullFilePath in settings['knownFiles']):
            # put file in 'foundFiles'
            settings['foundFiles'].append(fullFilePath)
          else:
            # UtilLog.debug('new file found: ' + fullFilePath)
            settings['filesToAdd'].append(fullFilePath)

    # now check if any know files has been removed
    for fileName in settings['knownFiles']:
      if ('foundFiles' not in settings or fileName not in settings['foundFiles']):
        # UtilLog.debug('missing file: ' + fileName)
        settings['filesToRemove'].append(fileName)


    modifications = False
    # finally write updated file lists
    if (settings['filesToAdd'] or settings['filesToRemove']):
      targeListFileName, targetList = settings['files'][0]

      # additions
      for fileName in settings['filesToAdd']:
        UtilLog.debug('adding ' + fileName + ' to ' + targeListFileName)
        targetList.append(fileName)

      # removals
      for fileName in settings['filesToRemove']:
        UtilLog.debug('removing ' + fileName + ' from ' + targeListFileName)
        # some targets have multiple list files, for now we only know how to remove from the FIRST
        # secondary list files need to be updated manually
        assert(fileName in targetList)
        targetList.remove(fileName)

      # write list changes to the file
      file = open(os.path.join(self.options.projectRoot, targeListFileName), 'w')
      for fileName in targetList:
        file.write(fileName+'\n')
      file.close()

      modifications = True

    return modifications


def folderParse(s):
    try:
        aList = s.split(',')
        lstFileCount = 0
        folders = []
        lstFiles = []
        for item in aList:
          print(item)
          if '.lst' in item:
            lstFiles.append(item)
          else:
            folders.append(item)
        if len(lstFiles) == 0:
          raise argparse.ArgumentTypeError("There must be at least one .lst file")
        if len(folders) == 0:
          raise argparse.ArgumentTypeError("There must be at least one folder")
        return [folders, lstFiles]
    except:
        raise argparse.ArgumentTypeError("pair of: list of folders, list of source files, all comma separated")

  
def parseArgs(scriptArgs):
  version = '1.0'
  parser = argparse.ArgumentParser(description='updates gyp list files based on the source files found on disk', version=version)
  parser.add_argument('--debug', '-d', '--verbose', dest='verbose', action='store_true',
                      help='prints debug output')
  parser.add_argument('--projectRoot', metavar='PROJECT_ROOT', dest='projectRoot', action='store', required=True,
                      help='prints debug output')
  parser.add_argument('--errorExitCode', '-e', dest='errorExitCode', action='store', default=0,
                      help='value to exit with if list files change')
  parser.add_argument('--sourcePair', '-s', dest="folderLists", type=folderParse, action='store', nargs='+', required=True,
                      help="pair of: list of folders, list of source files, all comma separated")
  parser.add_argument('--exclude', '-x', dest='exclude_globs', action='append', default=[],
                      help="file glob patterns to exclude")
  (options, args) = parser.parse_known_args(scriptArgs)

  projectRoot = options.projectRoot
  assert(projectRoot)
  return options

class Expando(object):
    pass


if __name__ == '__main__':

  # go to the script dir, so that we can find the project dir
  # in the future replace this with command line param
  # selfPath = os.path.dirname(os.path.realpath(__file__))
  # os.chdir(selfPath)
  # projectRoot = subprocess.check_output(['git', 'rev-parse', '--show-toplevel']).rstrip("\r\n")
  # options.projectRoot = projectRoot
  # options.verbose = True
  # generator.processFolder(['basestation/src/anki/cozmo/basestation', 'basestation/include/anki/cozmo'], ['project/gyp/basestation.lst'])

  # find project root path
  # projectRoot = subprocess.check_output(['git', 'rev-parse', '--show-toplevel']).rstrip("\r\n")
  options = parseArgs(sys.argv)
  # print options
  generator = FileListGenerator(options)
  modifications = False
  for folderList in options.folderLists:
    if generator.processFolder(folderList[0], folderList[1], options.exclude_globs):
      modifications = True
  
  if modifications:
    UtilLog.warn("Please run gyp to regenerate your project files")
    sys.exit(options.errorExitCode)
  else:
    UtilLog.debug("Source file lists - all good.")
    sys.exit(0)


