#!/usr/bin/env python2

# script which runs all tests in the given gtest executable in multiple processes

import subprocess
from Queue import Queue
import threading
# from pprint import pprint
import multiprocessing
import argparse
import os
import xml.etree.ElementTree as ET
import tempfile
import time
import operator
import glob
from collections import OrderedDict

startTime = time.time()

parser = argparse.ArgumentParser(description='Run gtest tests in parallel')
parser.add_argument('--path', metavar='path',
                    default='.', help='The path to run the executable in (default .)')
parser.add_argument('--executable', metavar='executable',
                    default='UnitTest', help='name of the executable to run')
parser.add_argument('--gtest_filter', help='Filter which tests to run')
parser.add_argument('--gtest_path', const=True, action='store', dest='gtestPath', nargs='?',
                    default=None, help='location of gtest libs for DYLD_*_PATH')
parser.add_argument('--work_path', const=True, action='store', dest='workPath', nargs='?',
                    default=None, help='location of ANKIWORKROOT')
parser.add_argument('--config_path', const=True, action='store', dest='configPath', nargs='?',
                    default=None, help='location of ANKICONFIGROOT')
parser.add_argument('--gtest_output', const=True, action='store', dest='gtestOutput', nargs='?',
                    default=None, help='location of GTEST_OUTPUT')
parser.add_argument('--shuffle', const=True, action='store_const',
                    default=False, help='Randomize the order of the tests (may be slower)')
parser.add_argument('--repeat', default=1, type=int,
                    metavar='N', help='Repeat each test N times')
parser.add_argument('--stdout', const=True, action='store_const',
                    default=False, help='Show stdout from the unit tests')
parser.add_argument('--stdout_file', const=True, action='store_const',
                    default=False, help='Save out to file, must have xml_dir and xml_basename')
parser.add_argument('--stdout_fail', const=True, action='store_const',
                    default=False, help='Show stdout for chunks which fail')
parser.add_argument('--silent', const=True, action='store_const',
                    default=False, help='Supresses running stats')
parser.add_argument('--xml_dir', nargs='?', default=None, metavar='dir',
                    help='Directory to output xml results, relative to path argument (default = .)')
parser.add_argument('--xml_basename', nargs='?', metavar='name',
                    default='googleTest_',
                    help='base name for xml output. Files will be named like "name0.xml".Default is googleTest_')
parser.add_argument('--debug', default=None, metavar='scriptfile',
                    help='Run executable in gdb with the following script (must automatically exit).' )
parser.add_argument('--dry-run', const=True, action='store_const', default=False,
                    help='Just print the work that would be done, don\'t do it')

args = parser.parse_args()
#print args

if args.xml_dir == None:
    args.xml_dir = args.path

## set this to false to run each test suite in a process, otherwise it
## may split them up. False should run faster, but setting this to
## true might help if there are some odd bugs
singleProcPerSuite = False

## default behavior will look for .xml files resulting from previous runs, and use that timing information to
## optimize the performace of this run. If you set this to false, this behavior will be disabled
useOldTimingData = True

# change this value to over-ride how many processes to run
numProcs = multiprocessing.cpu_count()

# first get the list of all the tests, and the number of tests within each one

pathname = args.path
exe = "./" + args.executable

listArgs = [exe, "--gtest_list_tests"]
if args.gtest_filter != None:
    listArgs.append('--gtest_filter='+args.gtest_filter)

if args.stdout:
    out = None
else:
    out = open(os.devnull, 'w')

if args.gtestPath:
    os.environ['DYLD_FRAMEWORK_PATH'] = args.gtestPath
    os.environ['DYLD_LIBRARY_PATH'] = args.gtestPath
if args.workPath:
    os.environ['ANKIWORKROOT'] = args.workPath
if args.configPath:
    os.environ['ANKICONFIGROOT'] = args.configPath
if args.gtestOutput:
    os.environ['GTEST_OUTPUT'] = args.gtestOutput

listedTests = subprocess.check_output(listArgs, cwd=pathname)
testCases = {}

testName = None
numTests = 0
tests = []

def getTimedTestList(timeDict, path):
    numAdded = 0    
    for filename in glob.glob(os.path.join(path, "basestationGoogleTest_*.xml")):
        tree = ET.parse(filename)
        root = tree.getroot()
        for testcase in root.iter('testcase'):
          testTime = float(testcase.get('time'))
          testName = "%s.%s" % (testcase.get('classname'), testcase.get('name'))
          if testName in timeDict:
              timeDict[testName] = testTime
              numAdded += 1
    return numAdded

for line in listedTests.split('\n'):
    if len(line) > 1 and line[0] != ' ' and line[0] != '\n' and line[0] != '\t' and line[-1] == '.':
        if testName != None:
            testCases[testName] = tests
        testName = line[:-1]
        tests = []
    elif len(line) > 2:
        t = line.strip()
        if len(t) > 0 and testName != None:
            tests.append(t)
            numTests = numTests + 1

if testName != None:
    testCases[testName] = tests

if singleProcPerSuite:
    work = [x + ".*" for x in testCases]
else:
    if useOldTimingData:
        testTimes = OrderedDict()
        i = 0
        for caseName in testCases:
            for testName in testCases[caseName]:
                testTimes[ caseName+'.'+testName ] = 0.0

        numFound = getTimedTestList(testTimes, pathname)

        if numFound < 0.9 * len(testTimes):
            print "NOTE: Only have times for %d / %d tests, falling back to standard ordering" % (numFound, len(testTimes))
            useOldTimingData = False

    if useOldTimingData:
        # create work by figuring out how to split close to perfectly per thread
        totalTime = sum([testTimes[x] for x in testTimes])
        perProcTime = totalTime / numProcs

        if not args.silent:
            print "each proc should take %f time" % perProcTime

        # add wiggle room because we can't split well and we don't want an extra chunk
        perProcTime *= 1.1

        work = [""]
        lastWorkTime = 0.0

        for testName in testTimes:
            t = testTimes[testName]
            if t + lastWorkTime > perProcTime:
                # create new work entry
                if not args.silent:
                    print "created chunk %d, should take %f" % (len(work), lastWorkTime)
                lastWorkTime = t
                work.append(testName)
            else:
                # use old work entry
                lastWorkTime += t
                work[-1] += ':' + testName

    else:
        # flatten out into fully named tests
        filters = []
        i = 0
        for caseName in testCases:
            for testName in testCases[caseName]:
                filters.append(caseName+'.'+testName)

        if args.shuffle:
            print "shuffling tests"
            from random import shuffle
            shuffle(filters)

        # split the cases up into secions
        numTestsPerProc = 10
        work = [reduce(lambda a,b: a+":"+b, filters[i:i+numTestsPerProc]) for i in range(0, len(filters), numTestsPerProc)]


print "running", numTests, "tests in", numProcs, "processes (",len(work),"chunks )"

consoleLock = threading.Lock()

# these globals are all protected by the console lock
global allPassed
global workDone
global running
global chunkID
global fails
allPassed = True
workDone = 0
running = 0
chunkID = 0
fails = []
testTimes = []

def runTestCase(testCase):
    global running
    global workDone
    global chunkID
    global fails
    global failedFiles
    myChunkID = -1
    with consoleLock:
        running = running + 1
        chunkID = chunkID + 1
        myChunkID = chunkID



    outputXmlArg = "%s/%s%d.xml" % (
        args.xml_dir,
        args.xml_basename,
        myChunkID)
    outputStdArg = "%s/%s%d.txt" % (
        args.xml_dir,
        args.xml_basename,
        myChunkID)
    if args.stdout_file:
        # if not args.silent:
        #     print "case ", myChunkID, " redirecting output to ", outputStdArg
        thisStdOut = open(outputStdArg, 'w')
        tmpfileName = outputStdArg
    elif args.stdout_fail:
        thisStdOut = tempfile.NamedTemporaryFile(delete=False)
        tmpfileName = thisStdOut.name
    else:
        thisStdOut = out


    # prepare work root
    workRoot=None
    if 'ANKIWORKROOT' in os.environ:
        workRoot = os.environ['ANKIWORKROOT']
    if not workRoot:
        workRoot=args.xml_dir
    if not workRoot:
        workRoot=pathname
    workRoot=workRoot + "/case" + str(myChunkID)
    if not os.path.exists(workRoot):
        os.mkdir(workRoot)
    procEnv=os.environ.copy()
    procEnv['ANKIWORKROOT']=workRoot


    # if not args.silent:
    #     print "case ", myChunkID, " running filter: ", testCase      
    #print "case ", myChunkID, " ", exe, "--gtest_filter="+testCase, "--gtest_output=xml:"+outputXmlArg+".tmp", "--gtest_repeat=%d" % args.repeat, "-d1"
    procArgs = [exe, "--gtest_filter="+testCase, "--gtest_output=xml:"+outputXmlArg+".tmp",
                "--gtest_repeat=%d" % args.repeat, "-d1"]

    if args.debug != None:
        a = ['gdb', '-x', args.debug, '-args']
        a.extend(procArgs)
        procArgs = a

    ret = subprocess.call(procArgs,
                          cwd=pathname, stdout=thisStdOut, env=procEnv)


    if args.stdout_file:
        thisStdOut.close()
    elif args.stdout_fail:
        thisStdOut.close()

    with consoleLock:
        workDone = workDone + 1
        t = time.time() - startTime
        if not args.silent:
            print "completed",workDone,'/',len(work),"chunks (%6.2f%%) [%d running]" % ((100.0*float(workDone)/len(work)), running), "time =",t
        if ret != 0:
            global allPassed
            allPassed = False
        running = running - 1

    if ret != 0:
        if not args.silent:
            print "FAILURE on chunk", myChunkID, "return code =",ret
        if args.stdout_fail:
            with open(tmpfileName, 'r') as infile:
                print infile.read()
            #print "\n\n\n"

    try:
        xmlTree = ET.parse(outputXmlArg+".tmp")
        os.remove(outputXmlArg+".tmp")

        # gtest includes all the filter-excluded tests with
        # "notrun" status, which confuses TeamCity, so remove
        # those
        root = xmlTree.getroot()
        suitesToRemove = []
        for suite in root:
            removeSuite = True
            removeCases = []
            for case in suite:
                if case.attrib['status'] == "run":
                    removeSuite = False
                else:
                    removeCases.append(case)

                try:
                    name = case.attrib['classname'] + '.' + case.attrib['name']
                    dt = float(case.attrib['time'])
                    testTimes.append( (dt, name) )
                except ValueError:
                    pass

                for failure in case:
                    testname = "%s.%s" % (
                        suite.attrib['name'],
                        case.attrib['name'])
                    fails.append(testname + " details can be found in " + outputStdArg )
                    if not args.silent:
                        print "FAILED: "+testname
                        # re-format message so it looks like a compiler error
                        print failure.attrib['message'].replace('\n', ': failure\n', 1)

            if removeSuite:
               suitesToRemove.append(suite)
            else:
                for case in removeCases:
                    suite.remove(case)

        for suite in suitesToRemove:
            root.remove(suite)

        xmlTree.write(outputXmlArg)
    except IOError:
        fails.append("one of the following tests crashed (details can be found in " + outputStdArg + "): ")
        for test in testCase.split(':'):
            fails.append(" - "+test)
        # fails.append("There was a crash. See above")

def worker(idx):
    while True:
        item = q.get()
        ret = runTestCase(item)
        q.task_done()

if not args.dry_run:
    q = Queue()
    for i in range(numProcs):
        t = threading.Thread(target=worker, args=[i])
        t.daemon = True
        t.start()

    for testCase in work:
        q.put(testCase)

    q.join()

    if allPassed:
        t = time.time() - startTime
        print "all good!", numTests, "tests finished in", t, "seconds"
        print "The slowest 10 tests were:"
        for dt, name in sorted(testTimes, key=operator.itemgetter(0), reverse=True)[:10]:
            print "  %5.2f %s" % (dt, name)
        exit(0)
    else:
        print "\nerror: the following tests failed:"
        for f in fails:
            print f
        exit(1)
else:
    print "The following filters would run:"
    idx = 1
    for testCase in work:
        print idx, '\n  ' + '\n  '.join(testCase.split(':'))
        idx += 1

