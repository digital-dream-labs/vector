/**
 * File: archiveUtil.h
 *
 * Author: Lee Crippen
 * Created: 4/4/2016
 *
 * Description: Utility wrapper around needed archive file creation functionality.
 *
 * Copyright: Anki, Inc. 2016
 *
 */
#ifndef __Basestation_Util_File_ArchiveUtil_H_
#define __Basestation_Util_File_ArchiveUtil_H_

#include <vector>
#include <string>

// Forward declarations

namespace Anki {
namespace Vector {
  
class ArchiveUtil {
public:
  // Static methods for creating and expanding archive files. Only tar.gz files supported
  static bool CreateArchiveFromFiles(const std::string& outputPath,
                                     const std::string& filenameBase,
                                     const std::vector<std::string>& filenames);
  
  static bool CreateFilesFromArchive(const std::string& archivePath,
                                     const std::string& outputDirectory);
  
  // Removes an initial part of a filename (does nothing if the filename has no path separators)
  static std::string RemoveFilenameBase(const std::string& filenameBase, const std::string& filename);
  
private:
  static const char* GetArchiveErrorString(int errorCode);

};
  
} // end namespace Vector
} // end namespace Anki


#endif //__Basestation_Util_File_ArchiveUtil_H_

