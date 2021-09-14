#!/usr/bin/env python2

import os
import os.path
import shutil
import errno
import subprocess
import textwrap
import sys
import argparse
import time
import glob
import xml.etree.ElementTree as ET
from decimal import Decimal
import threading
import tarfile
import logging

#set up default logger
UtilLog = logging.getLogger('valgrind.test')
stdout_handler = logging.StreamHandler()
formatter = logging.Formatter('%(name)s - %(message)s')
stdout_handler.setFormatter(formatter)
UtilLog.addHandler(stdout_handler)


class WorkContext(object): pass


def mkdir_p(path):
  try:
    os.makedirs(path)
  except OSError as exc: # Python >2.5
    if exc.errno == errno.EEXIST and os.path.isdir(path):
      pass
    else: 
      raise



# build unittest executable
def build(options):
  # prepare paths
  project = os.path.join(options.projectRoot, 'generated/mac')
  derivedData = os.path.join(options.projectRoot, 'generated/mac/DerivedData')

  # prepare folders and clean output files
  mkdir_p (os.path.join(derivedData, options.buildType, 'testdata'))
  for filename in glob.glob(os.path.join(derivedData, options.buildType, options.targetName + 'Valgrind_*')):
    os.remove(filename)


  # prepare build command
  buildCommand = [
    'xcodebuild', 
    '-project', os.path.join(project, options.projectName + '.xcodeproj'),
    '-target', options.targetName + 'UnitTest',
    '-sdk', 'macosx',
    '-configuration', options.buildType,
    'SYMROOT=' + derivedData,
    'OBJROOT=' + derivedData,
    'build'
    ]

  # return true if build is good
  return subprocess.call(buildCommand) == 0



# runs single thread, work the tests queue while there is work to be done
def threadRunner(options, context, threadId):
  # while still running and there is work to be done
  while context.running:
    index = -1
    # acquire lock
    with context.lock:
      # if there is work to be done, claim it!
      if (context.nextWorkIndex < len(context.tests)):
        index = context.nextWorkIndex
        context.nextWorkIndex += 1
    # if work was claimed, run it
    if (index >= 0):
      context.results[index] = runTest(options, context.tests[index], '%03d' % (index))
      if (not context.results[index] and options.breakOption):
        # acquire lock
        with context.lock:
          context.nextWorkIndex = len(context.tests)
        break;
    else: 
      break



# runs all test groups, using multiple threads
# returns true if all tests suceeded correctly
def runAllTests(options, tests):

  # prepare the context
  context = WorkContext()
  context.running = True
  context.tests = tests
  context.results = [False] * len(tests)
  context.lock = threading.Lock()
  context.nextWorkIndex = 0

  # create and start threads
  context.threads = [threading.Thread(target=threadRunner, args=[options, context, i]) for i in range(0, int(options.jobs))]
  for thread in context.threads:
    thread.start()

  # threads working here
  for thread in context.threads:
    thread.join()

  # parse results
  returnValue = True
  failedCount = 0
  for result in context.results:
    if (not result):
      failedCount += 1
      returnValue = False

  # print summary
  if (not returnValue):
    buildFolder = os.path.join(options.projectRoot, 'generated/mac/DerivedData', options.buildType)
    issuesFound = subprocess.check_output('grep \{ ' + options.targetName + 'Valgrind_out_* | wc -l', 
      shell=True, cwd=buildFolder).rstrip("\r\n").strip()
    # do not covert these to UtilLog. these are needed for TeamCity
    print '##teamcity[buildStatisticValue key=\'ValgrindIssuesFound\' value=\'%s\']' % (issuesFound)
    print '##teamcity[buildStatisticValue key=\'ValgrindFailedTestGroups\' value=\'%d\']' % (failedCount)
    print '##teamcity[buildStatisticValue key=\'ValgrindTotalTestGroups\' value=\'%d\']' % (len(context.results))

  return returnValue



# runs single valgrind instance, and returns the result 
# returns True if not memory leaks were found
def runTest(options, testGroup, runId):
  # prepare the paths
  buildFolder = os.path.join(options.projectRoot, 'generated/mac/DerivedData', options.buildType)
  workDir = os.path.join(buildFolder , 'testdata' + runId)
  mkdir_p(workDir)
  # prepare the environment
  procEnv=os.environ.copy()
  procEnv['ANKIWORKROOT'] = workDir + '/'
  procEnv['ANKICONFIGROOT'] = buildFolder + '/'
  procEnv['DYLD_FRAMEWORK_PATH'] = buildFolder
  procEnv['DYLD_LIBRARY_PATH'] = buildFolder

  # prepare the command
  valgrinLogFile = os.path.join(buildFolder , options.targetName + 'Valgrind_out_' + runId + '.txt')
  runCommand1 = [
    'valgrind',
    '--gen-suppressions=all',
  ]
  runCommand2 = [
    '--log-file=' + valgrinLogFile,
    '--leak-check=full',
    '--show-leak-kinds=all',
    '--dsymutil=yes',
    '--track-origins=yes',
    '--num-callers=40',
    '--error-exitcode=1',
    os.path.join(buildFolder , options.targetName + 'UnitTest'),
    '--gtest_filter=' + ':'.join(testGroup),
    '--gtest_repeat=1',
    '-d' + str(options.loggingLevel),
    '-timing=valgrind'
  ]

  # find all suppresion files
  suppresionFolder = os.path.join(options.projectRoot, 'tools/valgrind')
  suppressions = []
  for root, dirs, files in os.walk(suppresionFolder):
    for file in files:
      if file.endswith(".sup"):
        suppressions.append('--suppressions=' + os.path.join(root, file))

  # build and print run command
  runCommand = runCommand1 + suppressions + runCommand2
  UtilLog.debug('valgrind run command ' + ' '.join(runCommand))

  # run valgrind
  gtestLogFileName = os.path.join(buildFolder, options.targetName + 'Valgrind_gtest_out_' + runId + '.txt')
  gtestLogFile = open(gtestLogFileName, 'w')
  startedTimeS = time.time()
  result = subprocess.call(runCommand, stdout = gtestLogFile, stderr = gtestLogFile, env=procEnv, cwd=buildFolder)
  ranForS = time.time() - startedTimeS
  gtestLogFile.close()

  UtilLog.info('run %s took %f seconds for filter %s' % (runId, ranForS, ':'.join(testGroup)))

  # parse outputfile
  logResult = parseValgrindOutput(options, valgrinLogFile, runId)

  if (result != 0 or logResult != True):
    UtilLog.error('ERRORS in run %s, result %d, logResult %d, test filter: %s' % (runId, result, logResult, ':'.join(testGroup)))
    return False

  # return true if there were no errors detected
  return True



# returns true if there were no errors in the log file
def parseValgrindOutput(options, logFile, runId):
  # read log file output
  fileHandle = open(logFile, 'r')
  lines = [line.strip() for line in fileHandle]
  fileHandle.close()

  # the summary will contain something like:
  # still reachable: 848 bytes in 13 blocks
  stillReachable = True
  for i in range(len(lines) - 9, len(lines)):
    line = lines[i]
    if ('still reachable' in line):
      numbers = [int(s) for s in line.split() if s.isdigit()]
      if (len(numbers) >= 2 and numbers[0] == 0 and numbers[1] == 0):
        stillReachable = False
        break
  if (stillReachable):
    UtilLog.warning('run %s summary' % (runId))
    for i in range(len(lines) - 9, len(lines) ):
      UtilLog.warning(lines[i])
    return False


  # last line fo valgrind output should be like this:
  # ERROR SUMMARY: 121 errors from 121 contexts
  line = lines[-1]
  numbers = [int(s) for s in line.split() if s.isdigit()]
  UtilLog.debug("valgrind numbers " + ' '.join(map(str,numbers)))
  # return false if there were errors found
  if (len(numbers) >= 2 and numbers[0] == 0 and numbers[1] == 0 and 'ERROR SUMMARY' in line):
    return True

  UtilLog.info('run %s summary' % (runId))
  for i in range(len(lines) - 9, len(lines)):
    UtilLog.info(lines[i])
  return False


# tarball valgrind output files together
def tarball(options):
  buildFolder = os.path.join(options.projectRoot, 'generated/mac/DerivedData', options.buildType)
  tar = tarfile.open(os.path.join(buildFolder, options.targetName + "Valgrind_out.tar.gz"), "w:gz")
  for fileName in glob.glob(os.path.join(buildFolder, options.targetName + "Valgrind_out*.txt")):
    tar.add(fileName, arcname=os.path.basename(fileName))
  for fileName in glob.glob(os.path.join(buildFolder, options.targetName + "Valgrind_gtest_out*.txt")):
    tar.add(fileName, arcname=os.path.basename(fileName))
  tar.close()



# returns list of tests that ran uless then maxTimeS
# assumes that unittests already ran and that the xml output was generated
def getTimedTestList(options, maxTimeS):
  buildFolder = os.path.join(options.projectRoot, 'generated/mac/DerivedData', options.buildType)
  tests = [[]]
  count = 0
  for file in glob.glob(os.path.join(buildFolder, options.targetName + "GoogleTest_*.xml")):
    tree = ET.parse(file)
    root = tree.getroot()
    for testcase in root.iter('testcase'):
      testTime = Decimal(testcase.get('time'))
      testName = "%s.%s" % (testcase.get('classname'), testcase.get('name'))
      UtilLog.debug('%f - %s' % (testTime, testName))
      if Decimal(testcase.get('time')) <= maxTimeS:
        count += 1
        if (count >= options.tests):
          count = 0
          tests.append([])
        tests[-1].append(testName)
  return tests


# returns list of all available gtests
def getAllTestList(options):
  buildFolder = os.path.join(options.projectRoot, 'generated/mac/DerivedData', options.buildType)
  procEnv=os.environ.copy()
  procEnv['DYLD_FRAMEWORK_PATH'] = buildFolder
  procEnv['DYLD_LIBRARY_PATH'] = buildFolder
  runCommand = [os.path.join(buildFolder , options.targetName + 'UnitTest'), '--gtest_list_tests' ]

  testClass = None
  tests = [[]]
  count = 0

  # ask gtest for a list of all tests
  listedTests = subprocess.check_output(runCommand, env=procEnv)
  # parse the output
  for line in listedTests.split('\n'):
    if len(line) > 1 and line[0] != ' ' and line[0] != '\n' and line[0] != '\t' and line[-1] == '.':
      testClass = line[:-1]
    elif len(line) > 2:
      testName = line.strip()
      if len(testName) > 0 and testClass != None:
        count += 1
        if (count >= options.tests):
          count = 0
          tests.append([])
        tests[-1].append('%s.%s' % (testClass, testName))
  return tests
      

# executes main script logic
def main(scriptArgs):

  # parse arguments
  version = '1.0'
  parser = argparse.ArgumentParser(
  	# formatter_class=argparse.ArgumentDefaultsHelpFormatter,
  	formatter_class=argparse.RawDescriptionHelpFormatter,
  	description='runs valgrind on set of unittest', 
  	epilog=textwrap.dedent('''
      available modes description:
      	short - runs all unittests that run under 100 milliseconds
      	all - runs all unittests
	'''),
  	version=version
  	)
  parser.add_argument('--debug', '-d', '--verbose', dest='debug', action='store_true',
                      help='prints debug output')
  parser.add_argument('--jobs', '-j', dest='jobs', action='store', default=7,
                      help='number of worker threads to use. (default: 7)')
  parser.add_argument('--tests', '-t', dest='tests', action='store', default=10,
                      help='number of tests per run. (default: 10)')
  parser.add_argument('--mode', '-m', dest='mode', action='store', default='all',
                      help='available modes [ short, all ]. (default: all)')
  parser.add_argument('--filter', '-f', dest='filter', action='store', default='',
                      help='providing google test filter overrides mode, tests, jobs. All listed tests will be run in single thread. (default: )')
  parser.add_argument('--break', dest='breakOption', action='store_true',
                      help='break out of run loop on first found memory leak')
  parser.add_argument('--buildType', '-b', dest='buildType', action='store', default='Debug',
                      help='build types [ Debug, Release ]. (default: Debug)')
  parser.add_argument('--projectRoot', dest='projectRoot', action='store',
                      help='location of the project root')
  parser.add_argument('--projectName', dest='projectName', action='store', required=True,
                      help='name of xcode project file')
  parser.add_argument('--targetName', dest='targetName', action='store', required=True,
                      help='name of xcode target')
  parser.add_argument('--logginglevel', '-l', dest='loggingLevel', action='store', default=4,
                      help='logging level to pass to unittests. (default: 4)')
  (options, args) = parser.parse_known_args(scriptArgs)

  if options.debug:
    UtilLog.setLevel(logging.DEBUG)
  else:
    UtilLog.setLevel(logging.INFO)

  # if no project root fund - make one up
  if not options.projectRoot:
    # go to the script dir, so that we can find the project dir
  	# in the future replace this with command line param
  	selfPath = os.path.dirname(os.path.realpath(__file__))
  	os.chdir(selfPath)

  	# find project root path
  	projectRoot = subprocess.check_output(['git', 'rev-parse', '--show-toplevel']).rstrip("\r\n")
  	options.projectRoot = projectRoot
  else:
    options.projectRoot = os.path.normpath(os.path.join(os.getcwd(), options.projectRoot))
  os.chdir(options.projectRoot)

  UtilLog.debug(options)

  # build the project first
  if not build(options):
    UtilLog.error("ERROR build failed")
    return 1

  # prepare list of tests to run
  tests = []
  if options.filter:
      tests = [ options.filter.split(':') ]
  elif options.mode == 'short':
    tests = getTimedTestList(options, 0.1)
  elif options.mode == 'all':
    tests = getAllTestList(options)
  else:
    UtilLog.error("ERROR invalid mode specified [%s]" % (options.mode))
    return 1

  if options.debug:
    UtilLog.debug('test groups')
    totalTests = 0
    for subGroup in tests:
      UtilLog.debug(' '.join(subGroup))
      totalTests += len(subGroup)
    UtilLog.debug('total groups %d, total tests %d' % (len(tests), totalTests))
  
  # exit if no tests were found
  if len(tests) == 0 or len(tests[-1]) == 0:
    UtilLog.error("ERROR no tests found to run")
    return 1

  # run the tests
  result = runAllTests(options, tests)
  tarball(options)

  if not result:
    UtilLog.error("ERROR valgrind reported errors")
    return 1

  return 0



if __name__ == '__main__':
  args = sys.argv
  sys.exit(main(args))
