//
//
// Doxygen style documentation for the DAS events in update-engine.py
//
// Our Doxygen documentation generator is built around C/C++ macros
// and is not easily portable to python.  This file is NOT intended to
// be compiled into any shipping binary.  It is used solely for the
// code under `project/doxygen`.

#include "util/logging/DAS.h"

DASMSG(robot_ota_download_start, "robot.ota_download_start",
       "Start of trying to update robot Over-The-Air (OTA)");
DASMSG_SEND();

DASMSG(robot_ota_download_stalled, "robot.ota_download_stalled",
       "Network stall while downloading OTA file");

DASMSG(robot_ota_download_end, "robot.ota_download_end",
       "End of trying to update robot Over-The-Air (OTA)");
DASMSG_SET(s1, "", "success or fail");
DASMSG_SET(s2, "", "The OS version for the next boot");
DASMSG_SET(s3, "", "Error string for failed OTA");
DASMSG_SET(i1, "", "Error code for failed OTA");
DASMSG_SEND();
