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
 * @file    reader_worker.cc
 * @brief
 * @author    cd.kim@hanwha.com
 *
 * @date    2024.06.19
 * @version    1.0
 */

#include "reader_worker.h"

ReaderWorker::ReaderWorker() {

}

ReaderWorker::~ReaderWorker() {

}

void ReaderWorker::SetData(std::filesystem::path save_path) {
  save_path_ = save_path;
}

void ReaderWorker::AddDrive(std::filesystem::path drive) {
  std::lock_guard<std::mutex> lock(drive_mtx_);
  drives_.push_back(drive);
}

void ReaderWorker::DelDrive(std::filesystem::path drive) {
  std::lock_guard<std::mutex> lock(drive_mtx_);
  auto it = std::find(drives_.begin(), drives_.end(), drive);
  if (it != drives_.end()) {
    drives_.erase(it);
  }
}

bool ReaderWorker::ParseHeader(std::string file_name, std::shared_ptr<ReaderObject> read_object) {
  if (read_object == nullptr)
    return false;

  if(!read_object.get()->FileOpen(file_name))
    return false;

  return read_object.get()->ParseMainHeader();
}

std::shared_ptr<std::deque<Archive_FileInfo>> ReaderWorker::GetNearestFile(std::vector<std::filesystem::path> drives, ChannelUUID channel_uuid,
                                                                           unsigned int time_stamp,
                                                                            MediaProfile profile_type, ArchiveReadType archive_read_type,
                                                                            bool is_include_curfile) {

  std::optional<std::filesystem::path> frame_full_path = std::nullopt;
  std::string frame_path;

  std::shared_ptr<std::deque<Archive_FileInfo>> files = std::make_shared<std::deque<Archive_FileInfo>>();
  for (const auto& drive : drives) {
    std::optional<std::vector<Archive_FileInfo>> file_names =
        FileSearcher::GetNearestFileNames(drive, save_path_, channel_uuid, time_stamp, profile_type, archive_read_type, is_include_curfile);

    if (file_names == std::nullopt)
      continue;

    for (const auto& path : file_names.value()) {
      files->push_back(path);
    }
  }
  if (files == nullptr || files->size() <= 0)
    return nullptr;


  std::sort(files->begin(), files->end(), [](const Archive_FileInfo& a, const Archive_FileInfo& b) { return a.time_stamp_ < b.time_stamp_; });
  if (is_include_curfile && kArchiveReadNext == archive_read_type) {
    int index = 0;
    for (auto it = files->begin(); it != files->end(); ++it) {
      if (it->time_stamp_ > time_stamp) {
        break;
      }
      index++;
    }
    for (int i = 1; i < index; i++) {
      files->pop_front();
    }
  }

  // Received processing
  // 1. Receive lists and use them only for up to 2 hours of the requested time. Delete data over 2 hours
  // 2. If there is no data within 2 hours, only the data for the next hour will be used.
  // The status of the data cannot be guaranteed when the above two conditions change.
  time_t time = getNextHourLastSecond(time_stamp, 1, archive_read_type);
  int in2hour_size = 0;
  if (archive_read_type == kArchiveReadNext) {
    in2hour_size = std::count_if(files->begin(), files->end(), [time](const Archive_FileInfo& obj) { return obj.time_stamp_ <= time; });
    if (in2hour_size != 0) {
      files->erase(std::remove_if(files->begin(), files->end(), [time](Archive_FileInfo x) { return x.time_stamp_ > time; }), files->end());
    } else {
      time = getNextHourLastSecond(files->begin()->time_stamp_, 0, archive_read_type);
      files->erase(std::remove_if(files->begin(), files->end(), [time](Archive_FileInfo x) { return x.time_stamp_ > time; }), files->end());
    }
  } else if (archive_read_type == kArchiveReadPrev) {
    in2hour_size = std::count_if(files->begin(), files->end(), [time](const Archive_FileInfo& obj) { return obj.time_stamp_ >= time; });
    if (in2hour_size != 0) {
      files->erase(std::remove_if(files->begin(), files->end(), [time](Archive_FileInfo x) { return x.time_stamp_ < time; }), files->end());
    } else {
      time = getNextHourLastSecond(files->begin()->time_stamp_, 0, archive_read_type);
      files->erase(std::remove_if(files->begin(), files->end(), [time](Archive_FileInfo x) { return x.time_stamp_ < time; }), files->end());
    }
  }

  return files;
}

bool ReaderWorker::Seek(ChannelUUID channel_uuid, unsigned long long time_stamp_msec, ArchiveReadType archive_read_type,
                        std::shared_ptr<ReaderObject> read_object) {
  if (time_stamp_msec < 10000) {
    SPDLOG_ERROR("Invalid time value received in seek function. [{}]", time_stamp_msec);
    return false;
  }

  {
    std::lock_guard<std::mutex> lock(drive_mtx_);
    if (drives_.size() == 0) {
      SPDLOG_ERROR("No drive is registered.");
      return false;
    }
  }

  std::vector<std::filesystem::path> drives;
  {
    std::lock_guard<std::mutex> lock(drive_mtx_);
    for (const auto& value : drives_) {
      drives.push_back(value);
    }
  }

  std::shared_ptr<std::deque<Archive_FileInfo>> nearestfile2 =
      GetNearestFile(drives, channel_uuid, time_stamp_msec / 1000, read_object.get()->GetProfile(), archive_read_type, true);
  if (nearestfile2 == nullptr)
    return false;

  read_object.get()->SetInfo(channel_uuid);
  read_object.get()->SetFiles(nearestfile2);
  std::optional<Archive_FileInfo> file_info;

  while (true) {
    if (archive_read_type == kArchiveReadNext)
      file_info = read_object.get()->GetNextFile();
    else if (archive_read_type == kArchiveReadPrev)
      file_info = read_object.get()->GetPrevFile();

    if (file_info == std::nullopt) {
      return false;
    }

    if (!ParseHeader(file_info->file_path_ + "/" + file_info->file_name_, read_object))
      continue;

    bool ret = read_object.get()->LoadDataByIndex(time_stamp_msec, archive_read_type);
    if (!ret)
      continue;
    else
      break;
  }

  return true;
}

bool ReaderWorker::IsInGOP(const PnxMediaTime& time, std::shared_ptr<ReaderObject> reader_object) {
  if (reader_object == nullptr)
    return false;
  return reader_object->IsInGOP(time);
}

std::shared_ptr<ArchiveChunkBuffer> ReaderWorker::GetStreamChunk(ArchiveChunkReadType gov_read_type, std::shared_ptr<ReaderObject> read_object) {
  return read_object.get()->GetStreamChunk(gov_read_type);
}

std::shared_ptr<ArchiveChunkBuffer> ReaderWorker::GetStreamGop(ArchiveChunkReadType gov_read_type, std::shared_ptr<ReaderObject> read_object) {
  return read_object.get()->GetStreamGop(gov_read_type);
}

std::shared_ptr<BaseBuffer> ReaderWorker::GetPrevData(ArchiveType get_video_type, std::shared_ptr<ReaderObject> read_object) {
   if (read_object == nullptr)
     return nullptr;

   std::shared_ptr<BaseBuffer> buffer = read_object.get()->GetPrevData(get_video_type);
  if (buffer != nullptr) {
    return buffer;
  }

  unsigned long long opened_file_time = read_object->GetOpenedFileTime();
  std::optional<Archive_FileInfo> file_info = read_object.get()->GetPrevFile();
  if (file_info == std::nullopt) {
    std::vector<std::filesystem::path> drives;
    {
      std::lock_guard<std::mutex> lock(drive_mtx_);
      for (const auto& value : drives_) {
        drives.push_back(value);
      }
    }

    std::shared_ptr<std::deque<Archive_FileInfo>> nearestfile2 =
        GetNearestFile(drives, read_object->GetSessionid(), opened_file_time - 1, read_object.get()->GetProfile(), kArchiveReadPrev);

    if (nearestfile2 == nullptr || nearestfile2->size() <= 0)
      return nullptr;

    read_object.get()->SetFiles(nearestfile2);
    file_info = read_object.get()->GetPrevFile();
  }
  if (file_info == std::nullopt)
    return nullptr;

  if (!ParseHeader(file_info->file_path_ + "/" + file_info->file_name_, read_object))
    return nullptr;

  bool ret = read_object.get()->LoadDataByIndex((opened_file_time - 1) * 1000, kArchiveReadPrev);
  if (!ret)
    return nullptr;

  buffer = read_object.get()->GetStreamChunk();
  if (buffer != nullptr) {
    return buffer;
  }

  return nullptr;
 }

std::optional<std::vector<std::shared_ptr<StreamBuffer>>> ReaderWorker::GetNextData(ArchiveType get_video_type, std::shared_ptr<ReaderObject> read_object) {
   if (read_object == nullptr)
     return std::nullopt;

  unsigned long long play_time = read_object->GetCurrentPositionTime();
  std::optional<std::vector<std::shared_ptr<StreamBuffer>>> buffers = read_object.get()->GetNextData(get_video_type);
  if (buffers != std::nullopt) {
    return buffers;
  }

  try {
    if (read_object != nullptr && std::filesystem::file_size(read_object.get()->FileName()) > read_object.get()->FileSize()) {
      return GetNextFrameIfFileUpdated(get_video_type, read_object);
    }
  } catch (const fs::filesystem_error& e) {
    std::cerr << "file system error: " << e.what() << std::endl;
  }

  std::optional<Archive_FileInfo> file_info = read_object.get()->GetNextFile();
  if (file_info == std::nullopt) {
    std::vector<std::filesystem::path> drives;
    {
      std::lock_guard<std::mutex> lock(drive_mtx_);
      for (const auto& value : drives_) {
        drives.push_back(value);
      }
    }

    std::shared_ptr<std::deque<Archive_FileInfo>> nearestfile2 =
        GetNearestFile(drives, read_object->GetSessionid(), play_time / 1000, read_object.get()->GetProfile(), kArchiveReadNext);
    if (nearestfile2 == nullptr || nearestfile2->size() <= 0)
      return std::nullopt;

    read_object.get()->SetFiles(nearestfile2);
    file_info = read_object.get()->GetNextFile();
  }
  if (file_info == std::nullopt)
    return std::nullopt;

  if (!ParseHeader(file_info->file_path_ + "/" + file_info->file_name_, read_object))
    return std::nullopt;

  bool ret = read_object.get()->LoadDataByIndex((unsigned long long)(file_info->time_stamp_) * 1000, kArchiveReadNext);
  if (!ret)
    return std::nullopt;

  buffers = read_object.get()->GetNextData(get_video_type);
  if (buffers != std::nullopt) {
    return buffers;
  }

  return std::nullopt;
 }


 std::optional<std::vector<std::shared_ptr<StreamBuffer>>> ReaderWorker::GetNextFrameIfFileUpdated(ArchiveType get_video_type,
                                                                                                   std::shared_ptr<ReaderObject> read_object) {
   try {
     std::filesystem::path filePath = read_object.get()->FileName();
     std::string file_name = filePath.filename().string();

     if (!ParseHeader(filePath.string(), read_object))
       return std::nullopt;
     
     if (read_object.get()->LoadDataByIndex(read_object->GetCurrentPositionTime(), kArchiveReadNext)) {
       std::optional<std::vector<std::shared_ptr<StreamBuffer>>> buffers = read_object.get()->GetNextData(get_video_type);
       if (buffers != std::nullopt && buffers->size() > 0 ) {
         return buffers;
       } else {
         return std::nullopt;
       }
       return std::nullopt;
     }
   } catch (const fs::filesystem_error& e) {
     std::cerr << "file system error: " << e.what() << std::endl;
   }
   return std::nullopt;
 }

time_t ReaderWorker::getNextHourLastSecond(time_t timestamp, int add_hour, ArchiveReadType archive_read_type) {
   struct tm* timeinfo = gmtime(&timestamp);
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

   if (archive_read_type == kArchiveReadNext) {
     timeinfo->tm_hour += add_hour;
     timeinfo->tm_min = 59;
     timeinfo->tm_sec = 59;
   } else {
     timeinfo->tm_hour -= add_hour;
     timeinfo->tm_min = 0;
     timeinfo->tm_sec = 0;
   }

#ifdef _WIN32
   std::time_t gm_timestamp = _mkgmtime(timeinfo);
#else
   std::time_t gm_timestamp = timegm(timeinfo);
#endif
   return gm_timestamp;
 }
