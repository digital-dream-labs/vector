# Victor Internationalization

Created by David Mudie Last updated Apr 08, 2019

This page provides a brief overview of the robot-side support for internationalization.

## Overview

Internationalization for Victor has been implemented by closely following the processes developed for Cozmo.

With Cozmo, localization is performed by the application on a user device. With Victor, the robot must be able to generate localized strings even when it is not connected to a user device, so localization must be performed on the robot itself.

## Locale Settings
The robot's current locale is managed as a customizable setting that is "owned" by the authorized user.

If the authorized user sets a new locale, the locale is written to a configuration file on the robot and stored as JDOCS property in the cloud.

When the robot boots up, it reads the current locale setting from disk.  When robot settings are synchronized with JDOCS in the cloud, the robot may receive a new setting.

## Localizable Strings
Localizable strings are stored in JSON files under /anki/resources on the robot.

String files for each locale are grouped into a single directory.

Each string file contains a top-level structure of smartling configuration tags, followed by a series of key-value pairs.

Each key-value pair consists of a key value plus a translated string for the current locale.

Translated strings may contain substitution markers like "{0}" or "{1}". These will be replaced with programmatic values (eg a count or customer name) when the string is prepared for use.

## Locale Component
Localizable string files are managed by the "Locale Component" in the robot engine process.

When the robot container changes locale, it will tell the Locale Component to load string tables for the new locale.

If the Locale Component does not support a given locale, it will fall back to loading US English (us-EN) as a default.

When a behavior needs to generate a localized string, it passes a key value to the locale component and gets back the corresponding translation for that key in the current locale.

If a string uses substitution markers, the behavior must provide values to be substituted. It is up to the behavior to manage order and numbering of string substitutions.

If a key is not found in the string table, the locale component will return the key itself as a localized value. This helps identify keys that have not yet been translated.

