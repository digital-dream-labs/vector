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
#ifndef __Basestation_Factory_FactoryTestLogger_H_
#define __Basestation_Factory_FactoryTestLogger_H_

#include "coretech/common/engine/math/pose.h"
#include "clad/externalInterface/messageEngineToGame.h"
#include "clad/externalInterface/messageGameToEngine.h"
#include "clad/robotInterface/messageEngineToRobot.h"
#include "clad/robotInterface/messageRobotToEngine.h"
#include "json/json.h"
#include <string>
#include <vector>
#include <fstream>


namespace Anki {

// Forward declaration
namespace Util {
  namespace Data {
    class DataPlatform;
  }
}
  
namespace Vector {

class FactoryTestLogger {
public:
  FactoryTestLogger(bool exportJson = true);
  ~FactoryTestLogger();
  
  // Specify the name of the log (i.e. log folder)
  // Optionally, specify if you want to append the current date and time to the log name
  bool StartLog(const std::string& logName, bool appendDateTime = false, Util::Data::DataPlatform* dataPlatform = nullptr);
  void CloseLog();
  
  bool IsOpen();
  
  std::string GetLogName() { return _logDir; }
  
  // Appends struct as formatted entry to log file
  bool Append(const FactoryTestResultEntry& data);
  bool Append(const CameraCalibration& data);
  bool Append(const BirthCertificate& data);
  bool Append(const IMUInfo& data);
  bool Append(const IMUTempDuration& data);
  bool Append(const CalibMetaInfo& data);
  bool AppendCliffValueOnDrop(const CliffSensorValue& data);
  bool AppendCliffValueOnGround(const CliffSensorValue& data);
  bool AppendCliffValuesOnFrontDrop(const CliffSensorValues& data);
  bool AppendCliffValuesOnBackDrop(const CliffSensorValues& data);
  bool AppendCliffValuesOnGround(const CliffSensorValues& data);
  bool AppendCalibPose(const PoseData& data);
  bool AppendObservedCubePose(const PoseData& data);
  bool Append(const ExternalInterface::RobotCompletedFactoryDotTest& msg);
  bool Append(const std::map<std::string, std::vector<FactoryTestResultCode>>& results);
  bool Append(const std::string& dataTypeName, const TouchSensorValues& data);
  bool Append(const std::string& name, const TouchSensorFilt& data);
  
  
  // DistanceSensorData is added to an json array of data called "name". Separate data entries are
  // labelled as "seq_*". Call with an existing name to add to that array
  bool Append(const std::string& name, const DistanceSensorData& data);

  // RangeSensorData is added to an json array of data called "name". Separate data entries are
  // labelled as "seq_*". Call with an existing name to add to that array
  bool Append(const std::string& name, const RangeSensorData& data);

  // Adds a file with the given contents to the log folder
  bool AddFile(const std::string& filename, const std::vector<uint8_t>& data);
  
  // Copies the engine log (DAS msgs) file to log folder
  bool CopyEngineLog(Util::Data::DataPlatform* dataPlatform);

  // Returns the number of logs
  uint32_t GetNumLogs(Util::Data::DataPlatform* dataPlatform);
  
  // Returns the number of archived logs
  uint32_t GetNumArchives(Util::Data::DataPlatform* dataPlatform);
  
  // Archives all existing logs into a single file.
  // Deletes original log directories.
  bool ArchiveLogs(Util::Data::DataPlatform* dataPlatform);
  
  
private:
  
  std::string ChooseNextFileName(const std::string& dir, const std::string& name);

  bool AppendCliffSensorValue(const std::string& readingName, const CliffSensorValue& data);
  bool AppendCliffSensorValues(const std::string& readingName, const CliffSensorValues& data);
  bool AppendPoseData(const std::string& poseName, const PoseData& data);
  bool AppendToFile(const std::string& data);

  bool ArchiveAndDelete(const std::string& archiveName, const std::string& logBaseDir);
  
  void ParseIMUTempDuration(const IMUTempDuration& data, Json::Value* json, std::stringstream& ss);
  
  std::string GetCurrDateTime() const;
  
  std::string _logDir;
  std::string _logFileName;
  std::ofstream _logFileHandle;
  
  Json::Value _json;
  bool _exportJson;
};

} // end namespace Vector
} // end namespace Anki


#endif //__Basestation_Factory_FactoryTestLogger_H_
