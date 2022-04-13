/**
 * File: factoryTestLogger
 *
 * Author: Kevin Yoon
 * Created: 6/14/2016
 *
 * Description: Exports structs to factory test (i.e. Playpen test) formatted log file
 *
 * Copyright: Anki, inc. 2016
 *
 */
#include "engine/factory/factoryTestLogger.h"
#include "engine/util/file/archiveUtil.h"
#include "coretech/common/engine/utils/data/dataPlatform.h"
#include "util/logging/logging.h"
#include "util/fileUtils/fileUtils.h"

#include <sstream>
#include <chrono>
#include <iomanip>
#include <unistd.h>

#define ARCHIVE_OLD_LOGS 1

namespace Anki {
namespace Vector {

  static const std::string _kLogTextFileName = "mfgData";
  static const std::string _kLogRootDirName = "factory_test_logs";
  static const std::string _kArchiveRootDirName = "factory_test_log_archives";
  static const Util::Data::Scope _kLogScope = Util::Data::Scope::Cache;
  static std::string _kPathToCopyLogTo = "/factory";
  
  static const int _kMaxEngineLogSizeBytes = 1500000;
  
  FactoryTestLogger::FactoryTestLogger(bool exportJson)
  : _logDir("")
  , _logFileName("")
  , _exportJson(exportJson)
  {
    
  }
  
  FactoryTestLogger::~FactoryTestLogger()
  {
    CloseLog();
  }
  
  std::string FactoryTestLogger::ChooseNextFileName(const std::string& dir, const std::string& name)
  {
    static constexpr uint32_t kNumberDigitsLength = 4;

    std::vector<std::string> dirNames;
    Util::FileUtils::ListAllDirectories(dir, dirNames);

    if(dirNames.empty())
    {
      std::ostringstream newNameStream;
      newNameStream << name
                    << "_-_"
                    << std::setfill('0')
                    << std::setw(kNumberDigitsLength)
                    << 0;
      return newNameStream.str();
    }

    auto listIter = dirNames.begin();
    while (listIter != dirNames.end())
    {
      // Remove entries not starting with prefix
      if (listIter->compare(0, name.length(), name) != 0)
      {
        listIter = dirNames.erase(listIter);
        continue;
      }

      listIter++;
    }
    
    // Otherwise:
    // Sort list of entries
    std::sort(dirNames.begin(), dirNames.end(), std::greater<std::string>());
    
    // Take the first name in the list
    const auto& entryToReplace = dirNames.front();
    
    // Pull out the iteration number, +3 for "_-_"
    const auto iterStrBegin = name.length() + 3;
    static constexpr uint32_t kMaxIterationNum = 9999;
    auto iterationNum = std::stoi(entryToReplace.substr(iterStrBegin, kNumberDigitsLength));

    if (iterationNum == kMaxIterationNum)
    {
      PRINT_NAMED_ERROR("FactoryTestLogger.ChooseNextFileName",
                        "Reached max number of iterations %d. Won't save more files.",
                        kMaxIterationNum);
      return "";
    }
    
    // use increased iteration number to make the new filename
    std::ostringstream newNameStream;
    newNameStream << name
                  << "_-_"
                  << std::setfill('0')
                  << std::setw(kNumberDigitsLength)
                  << ++iterationNum;
    return newNameStream.str();
  }

  bool FactoryTestLogger::StartLog(const std::string& logName, bool appendDateTime, Util::Data::DataPlatform* dataPlatform)
  {
    // Generate new log dir name
    std::string newLogDir = "";
    if (dataPlatform) {
      newLogDir = Util::FileUtils::FullFilePath({dataPlatform->pathToResource(_kLogScope, _kLogRootDirName)});
      newLogDir += "/" + ChooseNextFileName(dataPlatform->pathToResource(_kLogScope, _kLogRootDirName), logName) + "/";
    } else {
      newLogDir = Util::FileUtils::FullFilePath({_kLogRootDirName});
      newLogDir += "/" + ChooseNextFileName(_kLogRootDirName, logName) + "/";
    }

    // TODO(Al): Uncomment if we get correct time on robot
    // Append date time to log name
    // if (appendDateTime) {
    //   newLogDir += "_-_" + GetCurrDateTime() + "/";
    // }

    
    
    // Check if it already exists
    if (Util::FileUtils::DirectoryExists(newLogDir)) {
      if (_logDir == newLogDir) {
        PRINT_NAMED_WARNING("FactoryTestLogger.StartLog.DirIsCurrentLog",
                            "Aborting current log %s because why are you trying to start it again?", newLogDir.c_str());
        CloseLog();
      } else {
        PRINT_NAMED_WARNING("FactoryTestLogger.StartLog.DirExists",
                            "Ignoring log %s because it already exists", newLogDir.c_str());
      }
      return false;
    } else {
      CloseLog();
    }

    _logDir = newLogDir;
    PRINT_NAMED_INFO("FactoryTestLogger.StartLog.CreatingLogDir", "%s", _logDir.c_str());
    Util::FileUtils::CreateDirectory(_logDir);
    _logFileName = Util::FileUtils::FullFilePath({_logDir, _kLogTextFileName + (_exportJson ? ".json" : ".txt")});
    
    if (_logFileHandle.is_open()) {
      PRINT_NAMED_WARNING("FactoryTestLogger.FileUnexpectedlyOpen", "");
      _logFileHandle.close();
    }
    
    PRINT_NAMED_INFO("FactoryTestLogger.StartLog.CreatingLogFile", "%s", _logFileName.c_str());
    _logFileHandle.open(_logFileName, std::ofstream::out | std::ofstream::app);
    
    _json.clear();
    
    return true;
  }
  
  
  void FactoryTestLogger::CloseLog()
  {
    if (_logFileHandle.is_open()) {
      PRINT_NAMED_INFO("FactoryTestLogger.CloseLog.Closing", "%s", _logFileName.c_str());
      
      // If exporting json, write it to file here
      if (_exportJson) {
        // Use FastWriter to "compress" the json string (removes newlines, tabs, etc)
        Json::FastWriter writer;
        std::string json = writer.write(_json);
        _logFileHandle << json;
      }
      _logFileHandle.close();

      // Copy log to factory partition. This should overwrite any log already there
      // so it will always contain the most recent log
      PRINT_NAMED_INFO("FactoryTestLogger.CloseLog.Copying", 
                       "Copying log from %s to %s", 
                       _logFileName.c_str(),
                       _kPathToCopyLogTo.c_str());
      Util::FileUtils::CopyFile(_kPathToCopyLogTo, _logFileName);

      // The log file has been copied to the correct directory but now needs to be renamed
      const std::string oldFileName = _kPathToCopyLogTo + "/" + (_kLogTextFileName + (_exportJson ? ".json" : ".txt"));
      const std::string newFileName = _kPathToCopyLogTo + "/log0";
      int rc = std::rename(oldFileName.c_str(), 
                           newFileName.c_str());

      // Make sure files are written to disk
      sync();

      if(rc != 0)
      {
        PRINT_NAMED_ERROR("FactoryTestLogger.CloseLog.RenameFail",
                          "Failed to rename log from %s to %s",
                          oldFileName.c_str(),
                          newFileName.c_str());
      }
    }
    
    _logDir = "";
    _logFileName = "";
  }
  
  bool FactoryTestLogger::IsOpen() {
    return _logFileHandle.is_open();
  }
  
  bool FactoryTestLogger::Append(const FactoryTestResultEntry& data)
  {
    std::stringstream ss;
    if (_exportJson) {
      Json::Value& node = _json["PlayPenTest"];
      node["Result"]    = EnumToString(data.result);
      node["Time"]      = data.utcTime;
      std::stringstream shaStr;
      shaStr << std::hex << data.engineSHA1 << std::dec;
      node["SHA-1"]     = shaStr.str();
      node["StationID"] = data.stationID;
      for (auto i=0; i<data.timestamps.size(); ++i) {
        node["Timestamps"][i] = data.timestamps[i];
      }
      ss << "[PlayPenTest]\n" << node;
    } else {
      ss << "\n[PlayPenTest]"
         << "\nResult: "    << EnumToString(data.result)
         << "\nTime: "      << data.utcTime
         << "\nSHA-1: 0x"   << std::hex << data.engineSHA1 << std::dec
         << "\nStationID: " << data.stationID
         << "\nTimestamps: ";
      for(auto timestamp : data.timestamps) {
        ss << timestamp << " ";
      }
    }

    PRINT_NAMED_INFO("FactoryTestLogger.Append.FactoryTestResultEntry", "%s", ss.str().c_str());
    return AppendToFile(ss.str());
  }
  
  
  bool FactoryTestLogger::Append(const CameraCalibration& data)
  {
    std::stringstream ss;
    if (_exportJson) {
      Json::Value& node = _json["CameraCalibration"];
      node["fx"] = data.focalLength_x;
      node["fy"] = data.focalLength_y;
      node["cx"] = data.center_x;
      node["cy"] = data.center_y;
      node["skew"] = data.skew;
      node["nrows"] = data.nrows;
      node["ncols"] = data.ncols;
      for (auto i=0; i < data.distCoeffs.size(); ++i) {
        node["distortionCoeffs"][i] = data.distCoeffs[i];
      }
      ss << "[CameraCalibration]\n" << node;
    } else {
      ss << "\n[CameraCalibration]" << std::fixed
      << "\nfx: " << data.focalLength_x
      << "\nfy: " << data.focalLength_y
      << "\ncx: " << data.center_x
      << "\ncy: " << data.center_y
      << "\nskew: " << data.skew
      << "\nrows: " << data.nrows
      << "\ncols: " << data.ncols
      << "\ndistortionCoeffs: ";
 
      for(auto coeff : data.distCoeffs) {
        ss << coeff << " ";
      }
    }
    PRINT_NAMED_INFO("FactoryTestLogger.Append.CameraCalibration", "%s", ss.str().c_str());
    return AppendToFile(ss.str());
  }
  
  bool FactoryTestLogger::Append(const BirthCertificate& data)
  {
    std::stringstream ss;
    if (_exportJson) {
      Json::Value& node = _json["BirthCertificate"];
      node["AtFactory"] = static_cast<int>(data.atFactory);
      node["Factory"]   = static_cast<int>(data.whichFactory);
      node["Line"]      = static_cast<int>(data.whichLine);
      node["Model"]     = static_cast<int>(data.model);
      node["Year"]      = static_cast<int>(data.year);
      node["Month"]     = static_cast<int>(data.month);
      node["Day"]       = static_cast<int>(data.day);
      node["Hour"]      = static_cast<int>(data.hour);
      node["Minute"]    = static_cast<int>(data.minute);
      node["Second"]    = static_cast<int>(data.second);
      ss << "[BirthCertificate]\n" << node;
    } else {
      ss << "\n[BirthCertificate]"
         << "\nAtFactory: " << static_cast<int>(data.atFactory)
         << "\nFactory: "   << static_cast<int>(data.whichFactory)
         << "\nLine: "      << static_cast<int>(data.whichLine)
         << "\nModel: "     << static_cast<int>(data.model)
         << "\nYear: "      << static_cast<int>(data.year)
         << "\nMonth: "     << static_cast<int>(data.month)
         << "\nDay: "       << static_cast<int>(data.day)
         << "\nHour: "      << static_cast<int>(data.hour)
         << "\nMinute: "    << static_cast<int>(data.minute)
         << "\nSecond: "    << static_cast<int>(data.second);
    }
    PRINT_NAMED_INFO("FactoryTestLogger.Append.BirthCertificate", "%s", ss.str().c_str());
    return AppendToFile(ss.str());
  }

  bool FactoryTestLogger::Append(const CalibMetaInfo& data)
  {
    std::bitset<8> b(data.dotsFoundMask);
    
    std::stringstream ss;
    if (_exportJson) {
      Json::Value& node = _json["CalibMetaInfo"];
      node["ImagesUsed"] = b.to_string();
      ss << "[CalibMetaInfo]\n" << node;
    } else {
      ss << "\n[CalibMetaInfo]"
      << "\nImagesUsed: " << b.to_string();
    }
    PRINT_NAMED_INFO("FactoryTestLogger.Append.CalibMetaInfo", "%s", ss.str().c_str());
    return AppendToFile(ss.str());
  }
  
  void FactoryTestLogger::ParseIMUTempDuration(const IMUTempDuration& data,
                                                    Json::Value* json,
                                                    std::stringstream& ss)
  {
    if(_exportJson)
    {
      DEV_ASSERT(json != nullptr, "FactoryTestLogger.NullJson");
      
      Json::Value& node = (*json)["IMUTempDuration"];
      node["TempStart_c"] = data.tempStart_c;
      node["TempEnd_c"] = data.tempEnd_c;
      node["duration_ms"] = data.duration_ms;
      ss << "[IMUTempDuration]\n" << node;
    }
    else
    {
      ss << "\n[IMUTempDuration]"
      << "\nTempStart_c: " << data.tempStart_c
      << "\nTempEnd_c: " << data.tempEnd_c
      << "\nDuration_ms: " << data.duration_ms;
    }
  }
  
  bool FactoryTestLogger::Append(const IMUTempDuration& data)
  {
    std::stringstream ss;
    ParseIMUTempDuration(data, &_json, ss);
    PRINT_NAMED_INFO("FactoryTestLogger.Append.IMUTempDuration", "%s", ss.str().c_str());
    return AppendToFile(ss.str());
  }
  
  bool FactoryTestLogger::Append(const IMUInfo& data)
  {
    std::stringstream ss;
    if (_exportJson) {
      Json::Value& node = _json["IMUInfo"];
      node["DriftRate_degPerSec"] = data.driftRate_degPerSec;
      ParseIMUTempDuration(data.tempDuration, &node, ss);
      ss << "[IMUInfo]\n" << node;
    } else {
      ss << "\n[IMUInfo]" << std::fixed
      << "\nDriftRate_degPerSec: " << data.driftRate_degPerSec;
      ParseIMUTempDuration(data.tempDuration, nullptr, ss);
    }
    PRINT_NAMED_INFO("FactoryTestLogger.Append.IMUInfo", "%s", ss.str().c_str());
    return AppendToFile(ss.str());
  }
  
  bool FactoryTestLogger::AppendCliffValueOnDrop(const CliffSensorValue& data) {
    return AppendCliffSensorValue("CliffOnDrop", data);
  }
  
  bool FactoryTestLogger::AppendCliffValueOnGround(const CliffSensorValue& data) {
    return AppendCliffSensorValue("CliffOnGround", data);
  }
  
  bool FactoryTestLogger::AppendCliffValuesOnFrontDrop(const CliffSensorValues& data) {
    return AppendCliffSensorValues("CliffsOnFrontDrop", data);
  }
  
  bool FactoryTestLogger::AppendCliffValuesOnBackDrop(const CliffSensorValues& data) {
    return AppendCliffSensorValues("CliffsOnBackDrop", data);
  }
  
  bool FactoryTestLogger::AppendCliffValuesOnGround(const CliffSensorValues& data) {
    return AppendCliffSensorValues("CliffsOnGround", data);
  }
  
  bool FactoryTestLogger::AppendCliffSensorValue(const std::string& readingName, const CliffSensorValue& data)
  {
    std::stringstream ss;
    if (_exportJson) {
      Json::Value& node = _json[readingName];
      node["val"] = data.val;
      ss << "[" << readingName<< "]\n" << node;
    } else {
      ss << "\n[" << readingName << "]"
      << "\nval: " << data.val;
    }
    PRINT_NAMED_INFO("FactoryTestLogger.Append.CliffSensorValue", "%s", ss.str().c_str());
    return AppendToFile(ss.str());
  }
  
  bool FactoryTestLogger::AppendCliffSensorValues(const std::string& readingName, const CliffSensorValues& data)
  {
    std::stringstream ss;
    if (_exportJson) {
      Json::Value& node = _json[readingName];
      node["FR"] = data.FR;
      node["FL"] = data.FL;
      node["BR"] = data.BR;
      node["BL"] = data.BL;
      ss << "[" << readingName<< "]\n" << node;
    } else {
      ss << "\n[" << readingName << "]"
      << "\nFR: " << data.FR
      << "\nFL: " << data.FL
      << "\nBR: " << data.BR
      << "\nBL: " << data.BL;
    }
    PRINT_NAMED_INFO("FactoryTestLogger.Append.CliffSensorValues", "%s", ss.str().c_str());
    return AppendToFile(ss.str());
  }
      
  bool FactoryTestLogger::AppendCalibPose(const PoseData& data) {
    return AppendPoseData("CalibPose", data);
  }

  bool FactoryTestLogger::AppendObservedCubePose(const PoseData& data) {
    return AppendPoseData("ObservedCubePose", data);
  }
    
  bool FactoryTestLogger::AppendPoseData(const std::string& poseName, const PoseData& data)
  {
    std::stringstream ss;
    if (_exportJson) {
      Json::Value& node = _json[poseName];
      node["Rot_deg"][0] = RAD_TO_DEG(data.angleX_rad);
      node["Rot_deg"][1] = RAD_TO_DEG(data.angleY_rad);
      node["Rot_deg"][2] = RAD_TO_DEG(data.angleZ_rad);
      node["Trans_mm"][0] = data.transX_mm;
      node["Trans_mm"][1] = data.transY_mm;
      node["Trans_mm"][2] = data.transZ_mm;
      ss << "[" << poseName << "]\n" << node;
    } else {
      ss << "\n[" << poseName << "]" << std::fixed
         << "\nRot_deg: "  << RAD_TO_DEG(data.angleX_rad) << " " << RAD_TO_DEG(data.angleY_rad) << " " << RAD_TO_DEG(data.angleZ_rad)
         << "\nTrans_mm: " << data.transX_mm << " " << data.transY_mm << " " << data.transZ_mm;
    }
    PRINT_NAMED_INFO("FactoryTestLogger.Append.PoseData", "%s", ss.str().c_str());
    return AppendToFile(ss.str());
  }
  
  bool FactoryTestLogger::Append(const ExternalInterface::RobotCompletedFactoryDotTest& msg)
  {
    std::stringstream ss;
    if (_exportJson) {
      if(msg.success)
      {
        Json::Value& node = _json["CentroidInfo"];
        node["HeadAngle_deg"] = RAD_TO_DEG(msg.headAngle);
        node["UpperLeft"][0] = msg.dotCenX_pix[0];
        node["UpperLeft"][1] = msg.dotCenY_pix[0];
        node["LowerLeft"][0] = msg.dotCenX_pix[1];
        node["LowerLeft"][1] = msg.dotCenY_pix[1];
        node["UpperRight"][0] = msg.dotCenX_pix[2];
        node["UpperRight"][1] = msg.dotCenY_pix[2];
        node["LowerRight"][0] = msg.dotCenX_pix[3];
        node["LowerRight"][1] = msg.dotCenY_pix[3];
        ss << "[CentroidInfo]\n" << node;
        
        if(msg.didComputePose)
        {
          PoseData pd(msg.camPoseRoll_rad, msg.camPosePitch_rad, msg.camPoseYaw_rad,
                      msg.camPoseX_mm, msg.camPoseY_mm, msg.camPoseZ_mm);
          AppendPoseData("CamPose", pd);
        }
      }
    } else {
      if(msg.success)
      {
        ss << "\n[CentroidInfo]" << std::fixed
        << "\nHeadAngle_deg: " << RAD_TO_DEG(msg.headAngle)
        << "\nUpperLeft: "  << msg.dotCenX_pix[0] << " " << msg.dotCenY_pix[0]
        << "\nLowerLeft: "  << msg.dotCenX_pix[1] << " " << msg.dotCenY_pix[1]
        << "\nUpperRight: " << msg.dotCenX_pix[2] << " " << msg.dotCenY_pix[2]
        << "\nLowerRight: " << msg.dotCenX_pix[3] << " " << msg.dotCenY_pix[3];
        
        if(msg.didComputePose)
        {
          PoseData pd(msg.camPoseRoll_rad, msg.camPosePitch_rad, msg.camPoseYaw_rad,
                      msg.camPoseX_mm, msg.camPoseY_mm, msg.camPoseZ_mm);
          AppendPoseData("CamPose", pd);
        }
      }
    }
    
    PRINT_NAMED_INFO("FactoryTestLogger.Append.CentroidInfo", "%s", ss.str().c_str());
    return AppendToFile(ss.str());
  }
  
  bool FactoryTestLogger::Append(const std::string& name, const DistanceSensorData& data)
  {
    std::stringstream ss;
    if (_exportJson)
    {
      Json::Value& node = _json[name];
      Json::Value newNode;
      newNode["SensorDistance_mm"] = data.proxDistanceToTarget_mm;
      newNode["VisualDistance_mm"] = data.visualDistanceToTarget_mm;
      newNode["VisualAngleAway_rad"] = data.visualAngleAwayFromTarget_rad;
      node.append(newNode);
      ss << "[" << name << "]\n" << newNode;
    }
    else
    {
      ss << "\n[" << name << "]" << std::fixed
      << "\nSensorDistance_mm: " << data.proxDistanceToTarget_mm
      << "\nVisualDistance_mm: " << data.visualDistanceToTarget_mm
      << "\nVisualAngleAway_rad: " << data.visualAngleAwayFromTarget_rad;
    }
    PRINT_NAMED_INFO("FactoryTestLogger.Append.DistanceSensorData", "%s", ss.str().c_str());
    return AppendToFile(ss.str());
  }

  bool FactoryTestLogger::Append(const std::string& name, const RangeSensorData& data)
  {
    std::stringstream ss;
    if (_exportJson)
    {
      Json::Value& node = _json[name];
      Json::Value newNode;

      // Should be 32 of these one for each ROI
      for(const auto& range : data.rangeData.data)
      {
        std::string roiName = "Roi" + std::to_string(range.roi);
        Json::Value& rangeNode = newNode[roiName];

        // Each ROI can report multiple distance readings, 1 for each object
        // it detected
        for(const auto& iter : range.readings)
        {
          Json::Value dataNode;
          dataNode["SignalRate_mcps"] = iter.signalRate_mcps;
          dataNode["AmbientRate_mcps"] = iter.ambientRate_mcps;
          dataNode["Sigma_mm"] = iter.sigma_mm;
          dataNode["RawRange_mm"] = iter.rawRange_mm;
          dataNode["Status"] = iter.status;
          rangeNode["Data"].append(dataNode);
        }

        rangeNode["Roi"] = range.roi;
        rangeNode["NumObjects"] = range.numObjects;
        rangeNode["RoiStatus"] = range.roiStatus;
        rangeNode["SpadCount"] = range.spadCount;
        rangeNode["ProcessedRange_mm"] = range.processedRange_mm;
      }
      
      newNode["VisualDistance_mm"] = data.visualDistanceToTarget_mm;
      newNode["VisualAngleAway_rad"] = data.visualAngleAwayFromTarget_rad;
      newNode["HeadAngle_rad"] = data.headAngle_rad;
      node.append(newNode);

      ss << "[" << name << "]\n" << newNode;
    }
    else
    {
      // ss << "\n[" << name << "]" << std::fixed
      // << "\nSignalIntensity: " << data.proxSensorData.signalIntensity
      // << "\nAmbientIntensity: " << data.proxSensorData.ambientIntensity
      // << "\nSpadCount: " << data.proxSensorData.spadCount
      // << "\nSensorDistanceRaw_mm: " << data.proxSensorData.distance_mm
      // << "\nVisualDistance_mm: " << data.visualDistanceToTarget_mm
      // << "\nVisualAngleAway_rad: " << data.visualAngleAwayFromTarget_rad;
    }
    //PRINT_NAMED_INFO("FactoryTestLogger.Append.DistanceSensorData", "%s", ss.str().c_str());
    return AppendToFile(ss.str());
  }

  bool FactoryTestLogger::Append(const std::map<std::string, std::vector<FactoryTestResultCode>>& results)
  {
    std::stringstream ss;
    
    for(const auto& kv : results)
    {
      if(_exportJson)
      {
        Json::Value& node = _json["AllPlaypenResults"][kv.first];
        for(const auto& result : kv.second)
        {
          node.append(FactoryTestResultCodeToString(result));
        }
        ss << "[" << kv.first << "]\n" << node;
      }
      else
      {
        ss << "\n[" << kv.first << "]\n";
        for(const auto& result : kv.second)
        {
          ss << FactoryTestResultCodeToString(result) << ", ";
        }
      }
    }
    PRINT_NAMED_INFO("FactoryTestLogger.Append.AllPlaypenResults", "%s", ss.str().c_str());
    return AppendToFile(ss.str());
  }
  
  bool FactoryTestLogger::Append(const std::string& dataTypeName, const TouchSensorValues& data)
  {
    std::stringstream ss;
    
    if(_exportJson)
    {
      Json::Value& node = _json[dataTypeName];
      for(const auto& val : data.data)
      {
        node.append(val);
      }
      ss << "[" << dataTypeName << "]\n" << node;
    }
    else
    {
      ss << "\n[" << dataTypeName << "]\n";
      for(const auto& val : data.data)
      {
        ss << val << ", ";
      }
    }

    PRINT_NAMED_INFO("FactoryTestLogger.Append.TouchSensorValues", "%s", ss.str().c_str());
    return AppendToFile(ss.str());
  }

  bool FactoryTestLogger::Append(const std::string& name, const TouchSensorFilt& data)
  {
    std::stringstream ss;
    
    if(_exportJson)
    {
      Json::Value& node = _json[name];
      Json::Value newNode;
      newNode["min"] = data.min;
      newNode["max"] = data.max;
      newNode["stddev"] = data.stddev;
      node.append(newNode);
      ss << "[" << name << "]\n" << newNode;
    }
    else
    {
      ss << "\n[" << name << "]" << std::fixed
      << "\nMin: " << data.min
      << "\nMax: " << data.max
      << "\nStdDev: " << data.stddev;
    }

    PRINT_NAMED_INFO("FactoryTestLogger.Append.TouchSensorValues", "%s", ss.str().c_str());
    return AppendToFile(ss.str());
  }

  
  bool FactoryTestLogger::AppendToFile(const std::string& data) {
    
    // If log name was not actually defined yet, do nothing.
    if (!_logFileHandle.is_open()) {
      PRINT_NAMED_WARNING("FactoryTestLogger.Append.LogNotStarted", "Ignoring because log not started");
      _json.clear();
      return false;
    }
    if (!_exportJson) {
      _logFileHandle << data << std::endl;
    }
    return true;
  }

  bool FactoryTestLogger::AddFile(const std::string& filename, const std::vector<uint8_t>& data)
  {
    if (_logDir.empty()) {
      PRINT_NAMED_WARNING("FactoryTestLogger.AddFile.LogNotStarted", "Ignoring because log not started");
      return false;
    }
    
    if (filename.empty()) {
      PRINT_NAMED_WARNING("FactoryTestLogger.AddFile.EmptyFilename", "");
      return false;
    }
    
    std::string outFile = Util::FileUtils::FullFilePath({_logDir, filename});
    
    if (Util::FileUtils::FileExists(outFile)) {
      PRINT_NAMED_WARNING("FactoryTestLogger.AddFile.AlreadyExists",
                          "Ignoring because %s already exists", outFile.c_str() );
      return false;
    }
    
    PRINT_NAMED_INFO("FactoryTestLogger.AddFile",
                     "File: %s, size: %zu bytes",
                     outFile.c_str(), data.size());
    
    return Util::FileUtils::WriteFile(outFile, data);
  }

  bool FactoryTestLogger::CopyEngineLog(Util::Data::DataPlatform* dataPlatform)
  {
    if (_logDir.empty()) {
      PRINT_NAMED_WARNING("FactoryTestLogger.CopyEngineLog.LogNotStarted", "Ignoring because log not started");
      return false;
    }
    
    if (dataPlatform == nullptr) {
      PRINT_NAMED_WARNING("FactoryTestLogger.CopyEngineLog.NullDataPlatform", "");
      return false;
    }
    
    // Get directories inside CurrentGameLog. There should only ever be one.
    // TODO (Al): Get LOGNAME (log folder) from cozmoeEngineMain.cpp instead of duplicating it
    std::string srcDir = dataPlatform->pathToResource(Util::Data::Scope::CurrentGameLog, "vic-engine");
    std::vector<std::string> dirs;
    Util::FileUtils::ListAllDirectories(srcDir, dirs);

    if (dirs.empty()) {
      PRINT_NAMED_WARNING("FactoryTestLogger.CopyEngineLog.NoLogFound", "");
      return false;
    }
    
    if (dirs.size() > 1) {
      PRINT_NAMED_WARNING("FactoryTestLogger.CopyEngineLog.MoreLogDirsThanExpected", "%zu", dirs.size());
    }
    
    srcDir = Util::FileUtils::FullFilePath({srcDir, dirs.front(), "print"});
    std::vector<std::string> engineLogFiles = Util::FileUtils::FilesInDirectory(srcDir, true, ".log", true);

    if(engineLogFiles.empty())
    {
      PRINT_NAMED_WARNING("FactoryTestLogger.CopyEngineLog.NoEngineLogsFound",
                          "Did not find any engine logs in directory %s",
                          srcDir.c_str());
    }

    bool res = true;
    for (auto f : engineLogFiles) {
      if (!Util::FileUtils::CopyFile(_logDir, f, _kMaxEngineLogSizeBytes)) {
        PRINT_NAMED_WARNING("FactoryTestLogger.CopyEngineLog.Failed", "%s", f.c_str());
        res = false;
      }
    }
    
    return res;
  }
  
  
  uint32_t FactoryTestLogger::GetNumLogs(Util::Data::DataPlatform* dataPlatform)
  {
    // Get base directory of log directories
    std::string baseDirectory = _kLogRootDirName;
    if (dataPlatform) {
      baseDirectory = dataPlatform->pathToResource(_kLogScope, _kLogRootDirName);
    }
    
    // Get all log directories
    std::vector<std::string> directoryList;
    Util::FileUtils::ListAllDirectories(baseDirectory, directoryList);
    
    return static_cast<uint32_t>(directoryList.size());
  }

  uint32_t FactoryTestLogger::GetNumArchives(Util::Data::DataPlatform* dataPlatform)
  {
    // Get base directory of log directories
    std::string baseDirectory = _kArchiveRootDirName;
    if (dataPlatform) {
      baseDirectory = dataPlatform->pathToResource(_kLogScope, _kArchiveRootDirName);
    }
    
    // Get all log directories
    std::vector<std::string> directoryList;
    Util::FileUtils::ListAllDirectories(baseDirectory, directoryList);
    
    return static_cast<uint32_t>(directoryList.size());
  }
  
  bool FactoryTestLogger::ArchiveAndDelete(const std::string& archiveName, const std::string& logBaseDir)
  {
    auto filePaths = Util::FileUtils::FilesInDirectory(logBaseDir, true, nullptr, true);
    if (ArchiveUtil::CreateArchiveFromFiles(archiveName, logBaseDir, filePaths)) {
      
      // Delete original logs
      Util::FileUtils::RemoveDirectory(logBaseDir);
      
      return true;
    }
    
    PRINT_NAMED_WARNING("FactoryTestLogger.ArchiveAndDelete.Failed",
                        "ArchiveName: %s, LogBaseDir: %s",
                        archiveName.c_str(), logBaseDir.c_str());
    return false;
  }
  
  bool FactoryTestLogger::ArchiveLogs(Util::Data::DataPlatform* dataPlatform)
  {
    // Get base directory of log directories
    std::string logBaseDirectory = _kLogRootDirName;
    std::string archiveBaseDirectory = _kArchiveRootDirName;
    if (dataPlatform) {
      logBaseDirectory = dataPlatform->pathToResource(_kLogScope, _kLogRootDirName);
      archiveBaseDirectory = dataPlatform->pathToResource(_kLogScope, _kArchiveRootDirName);
    }
    
    // Make sure output directory exists
    Util::FileUtils::CreateDirectory(archiveBaseDirectory, false, true);
    
    // Generate name of new archive based on current date-time
    std::string archiveName = GetCurrDateTime() + ".tar.gz";
    
    // Create archive
    if (!ArchiveAndDelete(archiveBaseDirectory + "/" + archiveName, logBaseDirectory)) {
      return false;
    }
    
#if ARCHIVE_OLD_LOGS
    archiveName = "old_" + GetCurrDateTime() + ".tar.gz";
    logBaseDirectory = dataPlatform->pathToResource(Util::Data::Scope::Cache, _kLogRootDirName);
    if (!ArchiveAndDelete(archiveBaseDirectory + "/" + archiveName, logBaseDirectory)) {
      return false;
    }
#endif
    
    
    
    return false;
  }
  
  std::string FactoryTestLogger::GetCurrDateTime() const
  {
    auto time_point = std::chrono::system_clock::now();
    time_t nowTime = std::chrono::system_clock::to_time_t(time_point);
    auto nowLocalTime = localtime(&nowTime);
    char buf[50];
    strftime(buf, sizeof(buf), "%F_%H-%M-%S", nowLocalTime);
    return std::string(buf);
  }
  
} // end namespace Vector
} // end namespace Anki
