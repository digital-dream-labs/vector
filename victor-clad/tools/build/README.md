# build-tools
A kitchen sink repo for building various small Anki projects

## Breakpad
Breakpad is a Google project for crash reporting.  In
[build-breakpad.sh](build-scripts/build-breakpad.sh), we are able to
build Breakpad for Linux, macOS, and Android.  We build these variants
on
[TeamCity](https://build.ankicore.com/project.html?projectId=Breakpad&tab=projectOverview).
The Android build artifacts are then imported into the
[CrashReportingAndroid](https://github.com/anki/CrashReportingAndroid)
repo.  The Linux build artifacts are used by the [HockeyApp
project](https://build.ankicore.com/project.html?projectId=HockeyApp&tab=projectOverview)
on TeamCity to process the Android symbol files before uploading to
HockeyApp.  The macOS artifacts are currently unused.  To build a new
revision of Breakpad, edit the build configuration settings on
TeamCity to adjust the `BREAKPAD_REVISION_TO_BUILD` parameter.

## Anki Android SDK
Google provides the Android SDK for Java development on the Android
platform.  However, the base distribution does not contain enough to
compile and package an Android application.  In
[build-android-sdk-zip.sh](build-scripts/build-android-sdk-zip.sh), we
download the base tools distribution and then install our required
packages (build-tools, platforms, etc.).  In addition, we download and
build [Facebook's buck tool](https://github.com/facebook/buck) that we
use for some components in OverDrive and Cozmo.  At the end we create
an approximately 1 gigabyte `.zip` file with everything.  The build is
executed on
[TeamCity](https://build.ankicore.com/project.html?projectId=AnkiAndroidSdk&tab=projectOverview).
After the `.zip` file is created, it is uploaded to Amazon's S3 service
using the
[upload-android-sdk-to-s3-build-assets.sh](build-scripts/upload-android-sdk-to-s3-build-assets.sh)
and
[upload-to-s3-build-assets.sh](build-scripts/upload-to-s3-build-assets.sh)
scripts.  The build configuration on TeamCity sets the appropriate
environment variables to authenticate with S3.

Both Cozmo and OverDrive specify which revision of the Anki Android
SDK they need in their top level `.buckconfig` files in the custom
`anki` section.  The `android.py` script duplicated in each project
knows how to find, download, install, and verify the Anki Android SDK.

To create a new revision of the Anki Android SDK, edit the
[build-android-sdk-zip.sh](build-scripts/build-android-sdk-zip.sh).
You will need to increment the `ANKI_ANDROID_SDK_VERSION` so that you
don't overwrite a previous version.  Be sure to build and test locally
before committing your changes and building on TeamCity.  Once you are
done, you can update Cozmo or OverDrive to use the new version.  You
will need to update the top level `.buckconfig` file and add the size
and sha1 hash information in the `android.py` file (see the
`install_sdk` function).
