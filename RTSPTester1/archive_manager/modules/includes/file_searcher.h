/***********************************************************************************
 * Copyright(c) 2024 by Hanwha Vision, Inc.
 *
 * This software is copyrighted by, and is the sole property of Hanwha Vision.
 * All rights, title, ownership, or other interests in the software remain the
 * property of Hanwha Vision. This software may only be used in accordance with
 * the corresponding license agreement. Any unauthorized use, duplication,
 * transmission, distribution, or disclosure of this software is expressly
 * forbidden.
 *
 * This Copyright notice may not be removed or modified without prior written
 * consent of Hanwha Vision.
 *
 * Hanwha Vision reserves the right to modify this software without notice.
 *
 * Hanwha Vision, Inc.
 * KOREA
 * http://www.hanwhavision.co.kr
 *********************************************************************************/

/**
 * @file   file_searcher.h
 * @brief
 * @author    cd.kim@hanwha.com
 *
 * @date    2024.07.11
 * @version    1.0
 */


#pragma once
#define _CRT_SECURE_NO_WARNINGS
#include <filesystem>
#include <fstream>
#include <string>
#include <chrono>
#include <iostream>
#include <vector>
#include <filesystem>
#include <optional>
#include <sstream>

#include "reader_object.h"
#include "archive_define.h"

namespace fs = std::filesystem;

class ARCHIVE_MANAGER_API FileSearcher {
 public:
  FileSearcher();
  ~FileSearcher();

  static std::string GetFolderFrTime(long long data_time);
  static std::string GetDatFileName(std::filesystem::path save_path, std::string session_id, std::string filetime, MediaProfile profile,
                                    long long begin_timestamp_msec);
  static std::optional<std::vector<Archive_FileInfo>> GetNearestFileNames(fs::path drive, fs::path base_directory, std::string session_id,
                                                                       unsigned int data_time,
                                                       MediaProfile profile, ArchiveReadType archive_read_type, bool is_include_curfile = false);
  static std::optional<std::vector<std::string>> GetFilesAndFolders(const std::string directory, bool is_directory);
  static std::optional<std::string> GetNearestFile(fs::path path, std::string file_name, MediaProfile profile, ArchiveReadType archive_read_type);
  static std::optional<std::string> GetNearestFolder(fs::path base_path, fs::path search_path, fs::path init_path, int path_level, int init_level,
                                                     ArchiveReadType archive_read_type);

  static unsigned int TimestampFromFile(std::string file_path);
  static unsigned int ParseFileNameToTimestamp(std::string filename);

 private:
};
