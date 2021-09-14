# Victor Unit Tests

Created by David Mudie Last updated Aug 25, 2018

Victor runs a set of automated unit tests to validate functionality after each build.  These tests are implemented in C++ using the Google C++ Testing framework and run under "ctest".

## Googletest Primer

Check out this primer on the Google testing framework before writing any unit tests: https://github.com/google/googletest/blob/master/googletest/docs/primer.md.

## Cmake Test Harness: ctest

ctest is a test harness integrated with cmake.  Unit test programs are defined as targets in CMakeLists.txt. cmake will automatically generate configuration files for ctest.

ctest is distributed with cmake but must be added to your $PATH.  If you don't already have cmake in your $PATH, use the "envsetup.sh" script to add it.

```
#!/usr/bin/env bash
$ ctest --version
ctest: command not found
$ source project/victor/envsetup.sh
...
$ ctest --version
ctest version 3.8.1
```

ctest is agnostic to specific features of the gtest framework, so it doesn't know about the individual test cases that make up a gtest executable.  It will count each executable as one test, regardless of the number of test cases run within that executable.

ctest's default behavior is to log test output and display a summary of results.  If you want to see detailed output, run "ctest -V".

## Running Specific Tests
You can use an environment variable GTEST_FILTER to restrict the list of tests:

```
#!/usr/bin/env bash
$ cd victor/_build/mac/Debug/test
 
$ export GTEST_FILTER="BlockWorld.*"
$ ctest -V
1: Note: Google Test filter = BlockWorld.*
1: [==========] Running 8 tests from 1 test case.
1: [----------] Global test environment set-up.
1: [----------] 8 tests from BlockWorld
...
```

If you are using an IDE, you can add GTEST_FILTER="blah" to your IDE environment, or add it to the corresponding set_tests_properties() directive in CMakeLists.txt.

## Running Disabled Tests
You can use an environment variable GTEST_ALSO_RUN_DISABLED_TESTS to run tests that are marked as disabled:

```
#!/usr/bin/env bash
$ cd victor/_build/mac/Debug/test
 
$ export GTEST_ALSO_RUN_DISABLED_TESTS=0
$ ctest -V
1: [==========] Running 97 tests from 24 test cases.
...
 
$ export GTEST_ALSO_RUN_DISABLED_TESTS=1
$ ctest -V
1: [==========] Running 108 tests from 25 test cases.
...
```

If you are using an IDE, you can add GTEST_ALSO_RUN_DISABLED_TESTS=1 to your IDE environment, or add it to the corresponding set_tests_properties() directive in CMakeLists.txt.

## Unit Test Scripts

Victor build servers use a set of helper scripts to run the unit tests. You can use them too.

```
#!/usr/bin/env bash
./project/buildServer/steps/unittestsUtil.sh
./project/buildServer/steps/unittestsCoretech.sh
./project/buildServer/steps/unittestsEngine.sh
```

Test output is logged in the build tree. If a test fails, additional output is sent to the terminal.

You can use environment variables to specify additional parameters:

```
#!/usr/bin/env bash
GTEST_FILTER="MoodManager.EmotionEventReadJson" ./project/buildServer/steps/unittestsEngine.sh
``

or:

```
#!/usr/bin/env bash
GTEST_FILTER="MoodManager.*" ./project/buildServer/steps/unittestsEngine.sh
```

## Unit Tests With Xcode
cmake can generate an Xcode project to build unit test executables. You can run the unit tests from xcode by selecting the 'test_engine' executable from the target drop down.

Engine tests expect to run in a directory with a link to resource files.  Cmake doesn't generate the working directory automatically, so you must set it by hand each time you generate the project. Open the scheme editor (Xcode → Product → Scheme → Edit Scheme)  and go to Run → Options tab. Enable Use custom working directory and set the directory to victor/_build/mac/Debug-Xcode/test.

Changing the command-line arguments using “Edit Scheme” (also at bottom of that dropdown) lets you supply filters for the specific tests you want to run once you know what’s failing. This makes it fast and easy just to repeatedly run the test you want to fix (and set breakpoints there).


e.g.: Navigate to Xcode → Product → Scheme → Edit Scheme... → Run → Arguments:

Arguments Passed On Launch: --gtest_filter=BlockWorld.*

## Unit Tests With VSCode
Unit tests can also be debugged from VSCode. This conveniently offers inline debugging without doing anything beyond a normal debug build by attaching via lldb to the test_engine binary which is created as part of the normal Mac, Debug build. The settings for VSCode debugging are maintained in the Victor repository in victor/templates/vscode/launch.json. 

To use VSCode for debugging, first set up VSCode using the instructions at victor/docs/development/vscode.md. If you've already done this, you may need to update settings by copying the tracked config files file from victor/templates/vscode/ to your local victor/.vscode folder to ensure the settings are up to date.

Build the project for Mac, Debug from the command line.

In VSCode, click on the debug panel in the Action bar (or open it via View→Debug, or cmd+shift+d). In the target selection drop down at the top of the Debug window, select the lldb test_engine configuration. Add breakpoints as desired, then click the run arrow.

To run specific unit tests in debug, open victor/.vscode/launch.json and edit the "lldb test_engine" config, setting the "GTEST_FILTER" environment variable to include and/or exclude tests, as described above. The example below will run all tests under the label "UserIntentsTransitions", exclusive of the "SetKeepAway" test.

```
"name": "lldb test_engine",
...
"env":{
"GTEST_FILTER": "UserIntentsTransitions.*:-UserIntentsTransitions.SetKeepAway:",
...
},...
```

## Unit Test Semantics
Anki unit tests use gtest macros such as ASSERT_TRUE and ASSERT_FALSE to report test status.  

Some unit tests generate error messages, so the presence of an error in the test log is NOT considered a failure.

Assertion checks (eg DEV_ASSERT or POSIX assert) will cause the test to exit immediately.  This is reported as a test failure unless caught by ASSERT_EXIT or ASSERT_DEATH.

Verify checks (eg ANKI_VERIFY) may log an error, but this is NOT considered a test failure.

Note that webots tests (separate from unit tests) are usually run with "--fail-on-error", which causes them to scan the log for errors. If a webots test log contains any errors, it will be reported as a failure!