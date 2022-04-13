/**
 * File: archiveUtil.cpp
 *
 * Author: Lee Crippen
 * Created: 4/4/2016
 *
 * Description: Utility wrapper around needed archive file creation functionality.
 *
 * Copyright: Anki, Inc. 2016
 *
 */
#include "engine/util/file/archiveUtil.h"
#include "util/fileUtils/fileUtils.h"
#include "util/global/globalDefinitions.h"
#include "util/logging/logging.h"
#include "util/math/numericCast.h"

#define ANKI_HAS_LIBARCHIVE 0

#if ANKI_HAS_LIBARCHIVE
  #include "archive.h"
  #include "archive_entry.h"
#endif 

#include <sys/stat.h>
#include <fstream>

namespace Anki {
namespace Vector {


bool ArchiveUtil::CreateArchiveFromFiles(const std::string& outputPath,
                                         const std::string& filenameBase,
                                         const std::vector<std::string>& filenames)
{
#if ANKI_HAS_LIBARCHIVE
  struct archive* newArchive = archive_write_new();
  if (nullptr == newArchive)
  {
    PRINT_NAMED_ERROR("ArchiveUtil.CreateArchiveFromFiles", "Could not alloc new archive");
  }
  
  int errorCode = ARCHIVE_OK;
  errorCode = archive_write_add_filter_gzip(newArchive);
  if (ARCHIVE_OK != errorCode)
  {
    PRINT_NAMED_ERROR("ArchiveUtil.CreateArchiveFromFiles", "Error %s setting up archive", GetArchiveErrorString(errorCode));
    archive_write_free(newArchive);
    return false;
  }
  
  errorCode = archive_write_set_format_pax_restricted(newArchive);
  if (ARCHIVE_OK != errorCode)
  {
    PRINT_NAMED_ERROR("ArchiveUtil.CreateArchiveFromFiles", "Error %s setting up archive", GetArchiveErrorString(errorCode));
    archive_write_free(newArchive);
    return false;
  }
  
  errorCode = archive_write_open_filename(newArchive, outputPath.c_str());
  if (ARCHIVE_OK != errorCode)
  {
    PRINT_NAMED_ERROR("ArchiveUtil.CreateArchiveFromFiles", "Error %s opening file for archive", GetArchiveErrorString(errorCode));
    archive_write_close(newArchive);
    archive_write_free(newArchive);
    return false;
  }
  
  struct archive_entry* entry = archive_entry_new();
  if (nullptr == entry)
  {
    PRINT_NAMED_ERROR("ArchiveUtil.CreateArchiveFromFiles", "Could not alloc new entry");
    archive_write_close(newArchive);
    archive_write_free(newArchive);
    return false;
  }
  
  char buff[8192];
  
  for (auto& filename : filenames)
  {
    struct stat st;
    stat(filename.c_str(), &st);
    archive_entry_clear(entry);
    auto newFilename = RemoveFilenameBase(filenameBase, filename);
    archive_entry_set_pathname(entry, newFilename.c_str());
    archive_entry_set_size(entry, st.st_size);
    archive_entry_set_filetype(entry, AE_IFREG);
    archive_entry_set_perm(entry, 0644);
    
    errorCode = archive_write_header(newArchive, entry);
    if (ARCHIVE_OK != errorCode)
    {
      PRINT_NAMED_ERROR("ArchiveUtil.CreateArchiveFromFiles", "Error %s writing file header", GetArchiveErrorString(errorCode));
      archive_entry_free(entry);
      archive_write_close(newArchive);
      archive_write_free(newArchive);
      return false;
    }
    
    std::ifstream file(filename, std::ios::binary);
    file.read(buff, sizeof(buff));
    auto len = file.gcount();
    while ( len > 0 ) {
      archive_write_data(newArchive, buff, len);
      file.read(buff, sizeof(buff));
      len = file.gcount();
    }
  }
  
  archive_entry_free(entry);
  archive_write_close(newArchive);
  archive_write_free(newArchive);
  
  return true;
#endif
  return false;
}
  
std::string ArchiveUtil::RemoveFilenameBase(const std::string& filenameBase, const std::string& filename)
{
  auto lastSep = filename.find_last_of('/');
  // We don't want to mess with a filename that has no path separators in it
  if (!filenameBase.empty() && lastSep != std::string::npos)
  {
    std::size_t lastMatchChar = std::string::npos;
    for (int i=0; i < filenameBase.length() && i <= lastSep; i++)
    {
      if (filenameBase[i] != filename[i])
      {
        break;
      }
      lastMatchChar = i;
    }
    
    if (lastMatchChar != std::string::npos)
    {
      return filename.substr(lastMatchChar+1);
    }
  }
  return filename;
}

#if ANKI_HAS_LIBARCHIVE
// Declaring this helper used in CreateFilesFromArchive
static int copy_data(struct archive *ar, struct archive *aw);
#endif

bool ArchiveUtil::CreateFilesFromArchive(const std::string& archivePath,
                                         const std::string& outputPath)
{
#if ANKI_HAS_LIBARCHIVE
  struct archive * read_archive = archive_read_new();
  if (nullptr == read_archive)
  {
    PRINT_NAMED_ERROR("ArchiveUtil.CreateFilesFromArchive", "Could not alloc read_archive");
    return false;
  }
  
  // Set up the finishing task list (to close things properly after error or at end)
  auto finishingTasks = std::vector<std::function<void()>>();
  auto doFinishingTasks = [&finishingTasks] () {
    for (const auto& task : finishingTasks) { task(); }
  };
  
  // Add in the finishing task to clean up the read_archive object
  finishingTasks.push_back( [read_archive] () {
    int errorCode = ARCHIVE_OK;
    errorCode = archive_read_close(read_archive);
    if (ARCHIVE_OK != errorCode)
    {
      PRINT_NAMED_ERROR("ArchiveUtil.CreateFilesFromArchive",
                        "Could not close read_archive: %s",
                        GetArchiveErrorString(errorCode));
    }
    
    errorCode = archive_read_free(read_archive);
    if (ARCHIVE_OK != errorCode)
    {
      PRINT_NAMED_ERROR("ArchiveUtil.CreateFilesFromArchive",
                        "Could not free read_archive: %s",
                        GetArchiveErrorString(errorCode));
    }
  });
  
  // Configure the read_archive object
  int errorCode = ARCHIVE_OK;
  errorCode = archive_read_support_format_tar(read_archive);
  if (ARCHIVE_OK != errorCode)
  {
    PRINT_NAMED_ERROR("ArchiveUtil.CreateFilesFromArchive",
                      "Could not set support_format_tar: %s",
                      GetArchiveErrorString(errorCode));
    doFinishingTasks();
    return false;
  }
  
  errorCode = archive_read_support_filter_gzip(read_archive);
  if (ARCHIVE_OK != errorCode)
  {
    PRINT_NAMED_ERROR("ArchiveUtil.CreateFilesFromArchive",
                      "Could not set support_filter_gzip: %s",
                      GetArchiveErrorString(errorCode));
    doFinishingTasks();
    return false;
  }
  
  // Create the extract_archive object
  struct archive * extract_archive = archive_write_disk_new();
  if (nullptr == extract_archive)
  {
    PRINT_NAMED_ERROR("ArchiveUtil.CreateFilesFromArchive", "Could not alloc extract_archive");
    doFinishingTasks();
    return false;
  }
  
  // Add in the finishing task to clean up the extract_archive object
  finishingTasks.push_back( [extract_archive] () {
    int errorCode = ARCHIVE_OK;
    errorCode = archive_write_close(extract_archive);
    if (ARCHIVE_OK != errorCode)
    {
      PRINT_NAMED_ERROR("ArchiveUtil.CreateFilesFromArchive",
                        "Could not close extract_archive: %s",
                        GetArchiveErrorString(errorCode));
    }
    
    errorCode = archive_write_free(extract_archive);
    if (ARCHIVE_OK != errorCode)
    {
      PRINT_NAMED_ERROR("ArchiveUtil.CreateFilesFromArchive",
                        "Could not free extract_archive: %s",
                        GetArchiveErrorString(errorCode));
    }
  });
  
  // Configure the extract_archive object
  // Use the default options
  const int desiredOptions = 0;
  errorCode = archive_write_disk_set_options(extract_archive, desiredOptions);
  if (ARCHIVE_OK != errorCode)
  {
    PRINT_NAMED_ERROR("ArchiveUtil.CreateFilesFromArchive",
                      "Could not call disk_set_options: %s with options %d",
                      GetArchiveErrorString(errorCode), desiredOptions);
    doFinishingTasks();
    return false;
  }
  
  errorCode = archive_write_disk_set_standard_lookup(extract_archive);
  if (ARCHIVE_OK != errorCode)
  {
    PRINT_NAMED_ERROR("ArchiveUtil.CreateFilesFromArchive",
                      "Could not call disk_set_standard_lookup: %s",
                      GetArchiveErrorString(errorCode));
    doFinishingTasks();
    return false;
  }
  
  // Try to open the archive
  errorCode = archive_read_open_filename(read_archive, archivePath.c_str(), 10240);
  if (ARCHIVE_OK != errorCode)
  {
    PRINT_NAMED_ERROR("ArchiveUtil.CreateFilesFromArchive",
                      "Could not open filename %s: %s",
                      archivePath.c_str(), archive_error_string(read_archive));
    doFinishingTasks();
    return false;
  }
  
  // Create the archive_entry object
  struct archive_entry* entry = archive_entry_new();
  if (nullptr == entry)
  {
    PRINT_NAMED_ERROR("ArchiveUtil.CreateFilesFromArchive", "Could not alloc entry");
    doFinishingTasks();
    return false;
  }
  
  // Add in the finishing task to clean up the entry object
  finishingTasks.push_back( [entry] () { archive_entry_free(entry); });
  
  // Loop through the archive and extract everything
  for (;;)
  {
    archive_entry_clear(entry);
    // Read the next entry header. Use the '2' version to pull info into our allocated entry object
    errorCode = archive_read_next_header2(read_archive, entry);
    if (ARCHIVE_EOF == errorCode)
    {
      break;
    }
    else if (errorCode < ARCHIVE_WARN)
    {
      PRINT_NAMED_ERROR("ArchiveUtil.CreateFilesFromArchive",
                        "Header read failed with fatal error: %s",
                        archive_error_string(read_archive));
      doFinishingTasks();
      return false;
    }
    else if (errorCode < ARCHIVE_OK)
    {
      PRINT_NAMED_WARNING("ArchiveUtil.CreateFilesFromArchive",
                          "Header read failed with nonfatal error: %s",
                          archive_error_string(read_archive));
    }
    
    // Update the pathname where we want to put this next entry
    const char* curPathname = archive_entry_pathname(entry);
    std::string destPathname = Util::FileUtils::FullFilePath( {outputPath, curPathname } );
    archive_entry_set_pathname(entry, destPathname.c_str());
    
    // Write the header to the extract archive
    errorCode = archive_write_header(extract_archive, entry);
    if (errorCode < ARCHIVE_OK)
    {
      PRINT_NAMED_WARNING("ArchiveUtil.CreateFilesFromArchive",
                          "Problem writing entry to extract archive: %s",
                          archive_error_string(extract_archive));
    }
    else if (archive_entry_size(entry) > 0)
    {
      // Copy out the data through the extract_archive object
      errorCode = copy_data(read_archive, extract_archive);
      if (errorCode < ARCHIVE_WARN)
      {
        PRINT_NAMED_ERROR("ArchiveUtil.CreateFilesFromArchive",
                          "copy_data failed: %s",
                          archive_error_string(extract_archive));
        doFinishingTasks();
        return false;
      }
      else if (errorCode < ARCHIVE_OK)
      {
        PRINT_NAMED_WARNING("ArchiveUtil.CreateFilesFromArchive",
                            "copy_data problem: %s",
                            archive_error_string(extract_archive));
      }
    }
    
    // Close up the entry
    errorCode = archive_write_finish_entry(extract_archive);
    if (errorCode < ARCHIVE_WARN)
    {
      PRINT_NAMED_ERROR("ArchiveUtil.CreateFilesFromArchive",
                        "write_finish_entry failed: %s",
                        archive_error_string(extract_archive));
      doFinishingTasks();
      return false;
    }
    else if (errorCode < ARCHIVE_OK)
    {
      PRINT_NAMED_WARNING("ArchiveUtil.CreateFilesFromArchive",
                          "write_finish_entry problem: %s",
                          archive_error_string(extract_archive));
    }
  }
  
  doFinishingTasks();
  return true;
#endif
  return false;
}

#if ANKI_HAS_LIBARCHIVE
static int copy_data(struct archive *ar, struct archive *aw)
{
  int errorCode = ARCHIVE_OK;
  const void *buff;
  size_t size;
  la_int64_t offset;
  
  for (;;) {
    errorCode = archive_read_data_block(ar, &buff, &size, &offset);
    if (errorCode == ARCHIVE_EOF)
    {
      return ARCHIVE_OK;
    }
    if (errorCode < ARCHIVE_OK)
    {
      PRINT_NAMED_INFO("ArchiveUtil.copy_data",
                       "Problem with read_data_block: %s",
                       archive_error_string(ar));
      return errorCode;
    }
    
    errorCode = Util::numeric_cast<int>(archive_write_data_block(aw, buff, size, offset));
    if (errorCode < ARCHIVE_OK)
    {
      PRINT_NAMED_INFO("ArchiveUtil.copy_data",
                       "Problem with write_data_block: %s",
                       archive_error_string(aw));
      return errorCode;
    }
  }
}
#endif

const char* ArchiveUtil::GetArchiveErrorString(int errorCode)
{
  switch (errorCode)
  {
    #if ANKI_HAS_LIBARCHIVE
    case ARCHIVE_EOF: return "ARCHIVE_EOF";
    case ARCHIVE_OK: return "ARCHIVE_OK";
    case ARCHIVE_RETRY: return "ARCHIVE_RETRY";
    case ARCHIVE_WARN: return "ARCHIVE_WARN";
    case ARCHIVE_FAILED: return "ARCHIVE_FAILED";
    case ARCHIVE_FATAL: return "ARCHIVE_FATAL";
    #endif
    default: return "UNKNOWN";
  }
}

} // end namespace Vector
} // end namespace Anki
