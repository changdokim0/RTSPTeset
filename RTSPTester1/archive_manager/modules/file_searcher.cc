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
 * @file   file_searcher.cpp
 * @brief
 * @author    cd.kim@hanwha.com
 *
 * @date    2024.07.11
 * @version    1.0
 */

#include "file_searcher.h"

FileSearcher::FileSearcher() {

}

FileSearcher::~FileSearcher() {

}

std::optional<std::vector<std::string>> FileSearcher::GetFilesAndFolders(const std::string directory, bool is_directory) {
  std::vector<std::string> result;
  try {
    if (!fs::is_directory(directory)) {
      return std::nullopt;
    }
    
    for (const auto& entry : fs::directory_iterator(directory)) {
      if (is_directory == true && fs::is_directory(entry)) {
        result.push_back(entry.path().generic_string());
      } else if (is_directory == false && fs::is_regular_file(entry)) {
        result.push_back(entry.path().filename().string());
      }
    }
  } catch (const std::exception& e) {
    SPDLOG_ERROR("GetFilesAndFolders Error code {}", e.what());
    return std::nullopt;
  }
  std::sort(result.begin(), result.end());
  return result;
}

std::string FileSearcher::GetFolderFrTime(long long data_time) {
  std::string file_time;
  std::time_t timestamp = static_cast<std::time_t>(data_time);
  std::tm* timeinfo = std::gmtime(&timestamp);
  std::tm local_time;
  if (timeinfo == nullptr) {
    auto time_point = std::chrono::system_clock::from_time_t(timestamp);
    auto utc_time = std::chrono::system_clock::to_time_t(time_point);
#ifdef _WIN32
    gmtime_s(&local_time, &utc_time);  // Windows
#else
    gmtime_r(&utc_time, &local_time);  // POSIX
#endif
    timeinfo = &local_time;
  }

  char time_str[40] = {0,};
  std::snprintf(time_str, sizeof(time_str), "%04d/%02d/%02d/%02d", timeinfo->tm_year + 1900, timeinfo->tm_mon + 1, timeinfo->tm_mday, timeinfo->tm_hour);
  file_time = time_str;
  return file_time;
}

std::optional<std::string> FileSearcher::GetNearestFile(fs::path path, std::string file_name, MediaProfile profile, ArchiveReadType archive_read_type) {
  std::optional<std::string> near_filename = std::nullopt;
  std::optional<std::vector<std::string>> filesAndFolders = GetFilesAndFolders(path.string(), false);
  std::optional<std::string> prev_filename = std::nullopt;
  for (auto filename : filesAndFolders.value()) {
    if (filename.find(profile.ToString()) == std::string::npos)
      continue;

    int result = filename.compare(file_name);
    if (result == 0) {
      return filename;
    } else if (result > 0) {
      if (archive_read_type == kArchiveReadNext) {
        near_filename = filename;
        break;
      } else {
        near_filename = prev_filename;
        break;
      }
    }
    prev_filename = filename;
  }

  if (archive_read_type == kArchiveReadPrev)
    near_filename = prev_filename;

  return near_filename;
}

std::optional<std::string> FileSearcher::GetNearestFolder(fs::path base_path, fs::path search_path, fs::path init_path, int path_level, int init_level,
                                                          ArchiveReadType archive_read_type) {
  if (path_level <= 0)
    return std::nullopt;

  fs::path full_path = base_path / search_path;
  full_path = full_path.generic_string();
  std::optional<std::string> folder_name = std::nullopt;
  std::optional<std::vector<std::string>> files_folders = GetFilesAndFolders(full_path.string(), true);
  if (files_folders == std::nullopt) {
    return GetNearestFolder(base_path, search_path.parent_path(), init_path, --path_level, init_level, archive_read_type);
  }

  fs::path t_path = init_path;
  for (int i = 0; i < init_level - path_level; i++)
    t_path = t_path.parent_path();

  std::vector<std::string> file_path_segments;
  for (auto file_path_segment : files_folders.value()) {
    int pos = file_path_segment.find(base_path.string());
    if (pos < 0) continue;
    file_path_segment.erase(pos, base_path.string().size() + 1);
    file_path_segments.push_back(file_path_segment);
  }

  std::vector<std::string> result;
  if (archive_read_type == kArchiveReadNext) {
    result = std::vector<std::string>(file_path_segments.begin(), std::partition(file_path_segments.begin(), file_path_segments.end(),
                                                                                    [&](const std::string& s) { return s > t_path.string(); }));
  } else {
    result = std::vector<std::string>(file_path_segments.begin(), std::partition(file_path_segments.begin(), file_path_segments.end(),
                                                                                    [&](const std::string& s) { return s < t_path.string(); }));
  }
  if (result.size() == 0)
    return GetNearestFolder(base_path, search_path.parent_path(), init_path, --path_level, init_level, archive_read_type);
  std::sort(result.begin(), result.end());

  if (archive_read_type == kArchiveReadNext) {
    for (auto filename : result) {
      if (!fs::is_empty(base_path / filename)) {
        folder_name = filename;
        break;
      }
    }
  } else {
    folder_name = result.back();
    for (auto it = result.rbegin(); it != result.rend(); ++it) {
      if (!fs::is_empty(base_path / *it)) {
        folder_name = *it;
        break;
      }
    }
  }

  if (folder_name == std::nullopt)
    return GetNearestFolder(base_path, search_path.parent_path(), init_path, --path_level, init_level, archive_read_type);

  if (path_level == init_level)
    return folder_name;
  else
    return GetNearestFolder(base_path, folder_name.value(), init_path, ++path_level, init_level, archive_read_type);
}

// Just send the closest file from the entire drive.
std::optional<std::vector<Archive_FileInfo>> FileSearcher::GetNearestFileNames(fs::path drive, fs::path base_directory, std::string session_id,
                                                                          unsigned int data_time, MediaProfile profile,
                                                                               ArchiveReadType archive_read_type, bool is_include_curfile) {
  std::vector<Archive_FileInfo> paths;

  fs::path base_path = drive/base_directory;
  fs::path media_path = base_path / session_id;
  media_path = media_path.generic_string();
  // 1. Collect data from the currently selected hour and the next hour
  // 2. If there is no nearby file, find the file in the nearest folder and send it to

  // 1. Collect data from the currently selected hour and the next hour(prev hour)
  int next_term = 60 * 60;
  if (kArchiveReadPrev == archive_read_type)
    next_term *= -1;

  for (int i = 0; i < 2; i++) {
    unsigned int search_time = data_time + (i * next_term); 
    std::string filename_frtime = GetFolderFrTime(search_time);
    fs::path dir_path = base_path / session_id / filename_frtime;
    dir_path = dir_path.generic_string();

    try {
      if (fs::is_directory(dir_path)) {
        //std::cout << "test" << std::endl;
        std::optional<std::vector<std::string>> search_files = GetFilesAndFolders(dir_path.string(), false);
        if (search_files == std::nullopt || search_files->size() <= 0)
          continue;

        for (const auto& file_name : search_files.value()) {
          if (file_name.find(profile.ToString()) == std::string::npos)
            continue;

          int timestamp = ParseFileNameToTimestamp(file_name);
          if (timestamp > 0) {
            paths.push_back(Archive_FileInfo(file_name, dir_path.string(), timestamp));
          }
        }
      }
    } catch (const std::exception& e) {
      SPDLOG_ERROR("GetNearestFileNames Error code {}", e.what());
      return std::nullopt;
    }
  }

  if (kArchiveReadNext == archive_read_type) {
    Archive_FileInfo first_item;
    paths.erase(std::remove_if(paths.begin(), paths.end(),
                               [data_time, & first_item](Archive_FileInfo x) { 
        bool ret = ParseFileNameToTimestamp(x.file_name_) < data_time; 
        if (ret)
          first_item = x;
        return ret; 
      }), paths.end());

    if (first_item.time_stamp_ != 0){
      if (std::find_if(paths.begin(), paths.end(), [&first_item](Archive_FileInfo& obj) { return obj.file_name_.compare(first_item.file_name_) == 0; }) == paths.end()) {
        if (is_include_curfile)
          paths.insert(paths.begin(), first_item);
      }
    }
  } else if (kArchiveReadPrev == archive_read_type) {
    paths.erase(std::remove_if(paths.begin(), paths.end(), [data_time](Archive_FileInfo x) { return ParseFileNameToTimestamp(x.file_name_) > data_time; }), paths.end());
  }

  if (paths.size() >= 1)
    return paths;


  // 2. If there is no nearby file, find the file in the nearest folder and send it to
  int path_level = 0;
  std::string path2 = GetFolderFrTime(data_time);
  for (char ch : path2)
    if (ch == '/')
      path_level++;

  path_level++;
  std::optional<std::vector<std::string>> search_files = std::nullopt;
  fs::path search_path = path2;
  while (true) {
    std::optional<std::string> near_folder_name =
        GetNearestFolder(media_path, search_path.parent_path(), search_path, path_level, path_level, archive_read_type);

    if (near_folder_name == std::nullopt) {
      return std::nullopt;  //pre, next files do not exist.
    }

    search_files = GetFilesAndFolders(media_path.string() + "/" + near_folder_name.value(), false);
    if (search_files == std::nullopt) {
      search_path = near_folder_name.value();
      continue;
    }

      search_files.value().erase(std::remove_if(search_files.value().begin(), search_files.value().end(),
                                              [&](const std::string& s) { return s.find(profile.ToString()) == std::string::npos; }),
                               search_files.value().end());

    if (search_files.value().size() <= 0) {
      search_path = near_folder_name.value();
      continue;
    } else {
      for (const auto& file_name : search_files.value()) {
        int timestamp = ParseFileNameToTimestamp(file_name);
        if (timestamp > 0) {
          std::string path = media_path.string() + "/" + near_folder_name.value();
          paths.push_back(Archive_FileInfo(file_name, path, timestamp));
        }
      }
      break;
    }
  }
  if (paths.size() > 0)
    return paths;
  else
    return std::nullopt;
  //return search_files;
}

std::string FileSearcher::GetDatFileName(std::filesystem::path save_path, std::string session_id, std::string filetime, MediaProfile profile,
                                         long long begin_timestamp_msec) {
  std::string file_time;
  std::time_t timestamp = static_cast<std::time_t>(begin_timestamp_msec/ 1000);
  std::tm* timeinfo = std::gmtime(&timestamp);
  std::tm local_time;
  if (timeinfo == nullptr) {
    auto time_point = std::chrono::system_clock::from_time_t(timestamp);
    auto utc_time = std::chrono::system_clock::to_time_t(time_point);
#ifdef _WIN32
    gmtime_s(&local_time, &utc_time);  // Windows
#else
    gmtime_r(&utc_time, &local_time);  // POSIX
#endif
    timeinfo = &local_time;
  }

  char time_str[40] = {0,};
  std::snprintf(time_str, sizeof(time_str), "%04d%02d%02d%02d%02d%02d%03d", timeinfo->tm_year + 1900, timeinfo->tm_mon + 1, timeinfo->tm_mday,
                timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec, (unsigned int)(begin_timestamp_msec % 1000));
  std::string filename_time = time_str;

  return (save_path/session_id).string() + "/" + filetime + "/" + filename_time + "_" + profile.ToString() + ".dat";
}

unsigned int FileSearcher::TimestampFromFile(std::string file_path) {
  fs::path path(file_path);
  std::string filename = path.filename().string();
  std::vector<std::string> pathComponents;
  for (const auto& part : path.parent_path()) {
    pathComponents.push_back(part.string());
  }

  if (pathComponents.size() < 4)
    return 0;

  int year, month, day, hour, minute;
  std::istringstream(pathComponents[pathComponents.size() - 4]) >> year;
  std::istringstream(pathComponents[pathComponents.size() - 3]) >> month;
  std::istringstream(pathComponents[pathComponents.size() - 2]) >> day;
  std::istringstream(pathComponents[pathComponents.size() - 1]) >> hour;
  std::istringstream(filename.substr(0, 2)) >> minute;

  if (year <= 0) {
    SPDLOG_ERROR("Failed TimestampFromFile");
  }
  std::tm timeInfo = {0};
  timeInfo.tm_year = year - 1900;
  timeInfo.tm_mon = month - 1;
  timeInfo.tm_mday = day;
  timeInfo.tm_hour = hour;
  timeInfo.tm_min = minute;
  timeInfo.tm_sec = 0;

  std::time_t timestamp = std::mktime(&timeInfo);
  return static_cast<unsigned int>(timestamp % UINT_MAX);
}

unsigned int FileSearcher::ParseFileNameToTimestamp(std::string filename) {
  if (filename.size() < 15)
    return 0;

  int year = 0, month = 0, day = 0, hour = 0, minute = 0, second = 0;
  try {
    year = std::stoi(filename.substr(0, 4));
    month = std::stoi(filename.substr(4, 2));
    day = std::stoi(filename.substr(6, 2));
    hour = std::stoi(filename.substr(8, 2));
    minute = std::stoi(filename.substr(10, 2));
    second = std::stoi(filename.substr(12, 2));
  } catch (const std::invalid_argument& e) {
    std::cerr << "Error: " << e.what() << std::endl;
    return 0;
  }
  if (year < 1500 || year > 2200 || month < 1 || month > 12 || day < 1 || day > 31 || hour < 0 || hour > 24 || minute < 0 || minute > 60 || second < 0 ||
      second > 60)
    return 0;

  std::tm time_info = {0};
  time_info.tm_year = year - 1900;
  time_info.tm_mon = month - 1;
  time_info.tm_mday = day;
  time_info.tm_hour = hour;
  time_info.tm_min = minute;
  time_info.tm_sec = second;
#ifdef _WIN32
  std::time_t gm_timestamp = _mkgmtime(&time_info);
#else
  std::time_t gm_timestamp = timegm(&time_info);
#endif
  return static_cast<unsigned int>(gm_timestamp % UINT_MAX);
}