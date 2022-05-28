/**
 * File: savedSessionManager.cpp
 *
 * Author: paluri
 * Created: 3/7/2018
 *
 * Description: Saved and load public key / session key information
 *
 * Copyright: Anki, Inc. 2018
 *
 **/

#include "savedSessionManager.h"
#include <sodium.h>
#include <sys/stat.h>
#include "log.h"
#include "cutils/properties.h"
#include "switchboardd/pairingMessages.h"
#include "util/fileUtils/fileUtils.h"

namespace Anki {
namespace Switchboard {

const std::string SavedSessionManager::kRtsKeyPath = "/dev/block/bootdevice/by-name/switchboard";
const std::string SavedSessionManager::kRtsKeyDataFile = "/data/data/com.anki.victor/persistent/switchboard/sessions";

const uint8_t SavedSessionManager::kMaxNumberClients = (uint8_t)255;
const uint32_t SavedSessionManager::kNativeBufferSize = 262144; // 256 * 1024 bytes -- (256kb)
const uint32_t SavedSessionManager::kMagicVersionNumber = 2; // MAGIC number that can't change
const uint32_t SavedSessionManager::kInvalidVersionNumber = (uint32_t) -1;
const char* SavedSessionManager::kPrefix = "ANKIBITS";

int SavedSessionManager::MigrateKeys() {
  // If we can successfully load data from kRtsKeyDataFile, then migration is complete
  RtsKeys rtsKeys = LoadRtsKeys();
  if (rtsKeys.keys.version == kMagicVersionNumber) {
    return 0;
  }

  Log::Write("Migrating keys from %s to %s", kRtsKeyPath.c_str(), kRtsKeyDataFile.c_str());

  rtsKeys = LoadRtsKeysFactory();

  if (rtsKeys.keys.version != kMagicVersionNumber) {
    Log::Error("Failed to read valid data from %s.", kRtsKeyPath.c_str());
    // If the data from the switchboard partition is invalid, reset the structure before
    // writing to kRtsKeyDataFile
    rtsKeys = {.keys = {}};
  }

  int rc = SaveRtsKeys(rtsKeys);
  if (rc) {
    return rc;
  }

  // After successfully moving the data to the kRtsKeyDataFile, reset the
  // data in the switchboard partition to have only the minimally necessary information
  std::string name;
  if (rtsKeys.keys.id.hasName) {
    // Copy existing name if we have it
    char *c = &(rtsKeys.keys.id.name[0]);
    char *end = &(rtsKeys.keys.id.name[sizeof(rtsKeys.keys.id.name)]);
    while (c < end && *c) {
      name.push_back(*c);
      c++;
    }
  }

  rc = ClearRtsKeysFactory(name);
  if (rc) {
    Log::Error("Failed to clear %s", kRtsKeyPath.c_str());
  }
  return 0;
}

std::string SavedSessionManager::GetRobotName() {
  char vicName[PROPERTY_VALUE_MAX] = {0};
  (void)property_get("anki.robot.name", vicName, "");

  return std::string(vicName);
}

bool SavedSessionManager::IsValidRtsKeysData(const std::vector<uint8_t>& data) {
  const struct RtsKeysData* keysData = reinterpret_cast<const struct RtsKeysData*>(data.data());

  // Make sure we have the minimum amount of data required
  if (data.size() < sizeof(*keysData)) {
    return false;
  }

  // Must start with ANKIBITS magic
  if (strncmp((char*) keysData->magic, kPrefix, strlen(kPrefix)) != 0) {
    return false;
  }

  // Must have magic version number 2
  if (kMagicVersionNumber != keysData->version) {
    return false;
  }

  // If a name is present, it cannot be empty
  if (keysData->id.hasName && !keysData->id.name[0]) {
    return false;
  }

  // Make sure we have enough data to cover all the clients
  size_t expectedLength = sizeof(*keysData) + (keysData->numKnownClients * sizeof(RtsClientData));
  if (data.size() < expectedLength) {
    return false;
  }
  return true;
}

RtsKeys SavedSessionManager::LoadRtsKeysFromFile(const std::string& fileName, size_t length) {
  RtsKeys savedData = { .keys = {.version = kInvalidVersionNumber} };

  if (!length) {
    length = sizeof(savedData.keys) + (kMaxNumberClients * sizeof(RtsClientData));
  }
  std::vector<uint8_t> data = Anki::Util::FileUtils::ReadFileAsBinary(fileName, 0, length);

  if (!IsValidRtsKeysData(data)) {
    Log::Error("%s does not have valid data", fileName.c_str());
    return savedData;
  }

  memcpy((char*)&savedData.keys, data.data(), sizeof(savedData.keys));

  for (auto i = 0; i < savedData.keys.numKnownClients; i++) {
    RtsClientData clientData;
    uint8_t* clientSrc = data.data() + sizeof(savedData.keys) + (i * sizeof(clientData));

    memcpy((uint8_t *)&(clientData), clientSrc, sizeof(clientData));
    savedData.clients.push_back(clientData);
  }

  return savedData;
}

RtsKeys SavedSessionManager::LoadRtsKeysFactory() {
  return LoadRtsKeysFromFile(kRtsKeyPath);
}

RtsKeys SavedSessionManager::LoadRtsKeys() {
  return LoadRtsKeysFromFile(kRtsKeyDataFile, SIZE_MAX);
}

int SavedSessionManager::SaveRtsKeysToFile(RtsKeys& saveData,
                                           const std::string& fileName,
                                           size_t fileLength) {

  // Make sure that we have the magic and version number set correctly
  memcpy(&saveData.keys.magic, kPrefix, strlen(kPrefix));
  saveData.keys.version = kMagicVersionNumber;

  // If we don't have a name, try to get it from properties
  if (!saveData.keys.id.hasName || !saveData.keys.id.name[0]) {
    (void) memset(saveData.keys.id.name, 0, sizeof(saveData.keys.id.name));
    std::string name = GetRobotName();
    if (!name.empty()) {
      (void) strncpy(saveData.keys.id.name, name.c_str(), sizeof(saveData.keys.id.name));
    }
  }
  // Make sure we don't claim to have a name if it is empty
  saveData.keys.id.hasName = saveData.keys.id.name[0] != '\0';

  // If somehow we hit max clients, start removing from the beginning
  if(saveData.clients.size() > kMaxNumberClients) {
    saveData.clients.erase(saveData.clients.begin(),
       saveData.clients.begin() + (saveData.clients.size() - kMaxNumberClients));
  }

  // Make sure we have a correct count of clients for serialization
  saveData.keys.numKnownClients = saveData.clients.size();

  // Serialize everything into a vector
  size_t length = sizeof(saveData.keys) + (saveData.clients.size() * sizeof(RtsClientData));

  // If the caller requested a file length larger than needed, we will honor it.
  // This is used to zero pad the file out to a desired size.
  if (fileLength > length) {
    length = fileLength;
  }
  std::vector<uint8_t> data;
  data.resize(length);
  memcpy(data.data(), &(saveData.keys), sizeof(saveData.keys));

  size_t offset = sizeof(saveData.keys);
  for (auto i = 0 ; i < saveData.clients.size(); i++) {
    RtsClientData* clientSrc = &(saveData.clients[i]);
    memcpy(data.data() + offset, clientSrc, sizeof(*clientSrc));
    offset += sizeof(*clientSrc);
  }

  // Write the data in one shot to the file
  bool success = Anki::Util::FileUtils::WriteFile(fileName, data);
  if (!success) {
    Log::Error("Failed to write key data to %s", fileName.c_str());
    return -2;
  }
  return 0;
}

int SavedSessionManager::ClearRtsKeysFactory(const std::string& name) {
  RtsKeys saveData = {.keys = {}};
  if (!name.empty()) {
    saveData.keys.id.hasName = true;
    strncpy(saveData.keys.id.name, name.c_str(), sizeof(saveData.keys.id.name));
  }

  return SaveRtsKeysToFile(saveData, kRtsKeyPath, kNativeBufferSize);
}

int SavedSessionManager::SaveRtsKeys(RtsKeys& saveData) {
  if (!Anki::Util::FileUtils::CreateDirectory(kRtsKeyDataFile, true)) {
    Log::Write("Could not create directory for %s.", kRtsKeyDataFile.c_str());
    return -1;
  }

  std::string tmpFileName = kRtsKeyDataFile + ".tmp";
  Anki::Util::FileUtils::DeleteFile(tmpFileName);

  int rc = SaveRtsKeysToFile(saveData, tmpFileName);
  if (rc) {
    return rc;
  }

  if (rename(tmpFileName.c_str(), kRtsKeyDataFile.c_str())) {
    Log::Error("Failed to rename %s to %s", tmpFileName.c_str(), kRtsKeyDataFile.c_str());
    Anki::Util::FileUtils::DeleteFile(tmpFileName);
    return -3;
  }
  return 0;
}

} // Switchboard
} // Anki
