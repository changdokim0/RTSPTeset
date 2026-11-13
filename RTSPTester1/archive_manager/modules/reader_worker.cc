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
#include "blaze_logger.h"
#include <atomic>

ReaderWorker::ReaderWorker() {

}

ReaderWorker::~ReaderWorker() {

}

void ReaderWorker::SetData(std::filesystem::path save_path) {
  save_path_ = save_path;
}

void ReaderWorker::AddDrive(std::filesystem::path drive) {
  std::lock_guard<std::mutex> lock(drive_mtx_);
  auto it = std::find(drives_.begin(), drives_.end(), drive);
  if (it == drives_.end()) {
    drives_.push_back(drive);
  }
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
                                                                           unsigned long long time_stamp,
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
  time_t time = getNextHourLastSecond(time_stamp /1000, 1, archive_read_type);
  int in2hour_size = 0;
  if (archive_read_type == kArchiveReadNext) {
    in2hour_size = std::count_if(files->begin(), files->end(), [time](const Archive_FileInfo& obj) { return obj.time_stamp_ /1000 <= time; });
    if (in2hour_size != 0) {
      files->erase(std::remove_if(files->begin(), files->end(), [time](Archive_FileInfo x) { return x.time_stamp_ / 1000 > time; }), files->end());
    } else {
      time = getNextHourLastSecond(files->begin()->time_stamp_ / 1000, 0, archive_read_type);
      files->erase(std::remove_if(files->begin(), files->end(), [time](Archive_FileInfo x) { return x.time_stamp_ / 1000 > time; }), files->end());
    }
  } else if (archive_read_type == kArchiveReadPrev) {
    in2hour_size = std::count_if(files->begin(), files->end(), [time](const Archive_FileInfo& obj) { return obj.time_stamp_ / 1000 >= time; });
    if (in2hour_size != 0) {
      files->erase(std::remove_if(files->begin(), files->end(), [time](Archive_FileInfo x) { return x.time_stamp_ / 1000 < time; }), files->end());
    } else {
      time = getNextHourLastSecond(files->begin()->time_stamp_ / 1000, 0, archive_read_type);
      files->erase(std::remove_if(files->begin(), files->end(), [time](Archive_FileInfo x) { return x.time_stamp_ / 1000 < time; }), files->end());
    }
  }

  return files;
}

bool ReaderWorker::Seek(ChannelUUID channel_uuid, unsigned long long time_stamp_msec, ArchiveReadType archive_read_type,
                        std::shared_ptr<ReaderObject> read_object) {
  {
    std::lock_guard<std::mutex> lock(drive_mtx_);
    if (drives_.size() == 0) {
      BLAZE_LOG_ERROR("[ArchiveL] ReaderWorker::Seek failed - no drive registered. ch:[{}]", channel_uuid);
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
      GetNearestFile(drives, channel_uuid, time_stamp_msec, read_object.get()->GetProfile(), archive_read_type, true);
  if (nearestfile2 == nullptr) {
    BLAZE_LOG_WARN("[ArchiveL] ReaderWorker::Seek no candidate file. ch:[{}] pos(ms):[{}] profile:[{}] read_type:[{}]", channel_uuid,
                   time_stamp_msec, read_object.get()->GetProfile().ToString(), static_cast<int>(archive_read_type));
    return false;
  }

  read_object.get()->SetInfo(channel_uuid);
  read_object.get()->SetFiles(nearestfile2);
  std::optional<Archive_FileInfo> file_info;

  while (true) {
    if (archive_read_type == kArchiveReadNext)
      file_info = read_object.get()->GetNextFile();
    else if (archive_read_type == kArchiveReadPrev)
      file_info = read_object.get()->GetPrevFile();

    if (file_info == std::nullopt) {
      BLAZE_LOG_WARN("[ArchiveL] ReaderWorker::Seek no file selected. ch:[{}] pos(ms):[{}] read_type:[{}]", channel_uuid, time_stamp_msec,
                     static_cast<int>(archive_read_type));
      return false;
    }

    if (!ParseHeader(file_info->file_path_ + "/" + file_info->file_name_, read_object)) {
      continue;
    }

    bool ret = read_object.get()->LoadDataByIndex(time_stamp_msec, archive_read_type);
    if (ret) 
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

  bool ret = read_object.get()->LoadDataByIndex((opened_file_time - 1), kArchiveReadPrev);
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
        GetNearestFile(drives, read_object->GetSessionid(), play_time , read_object.get()->GetProfile(), kArchiveReadNext);
    if (nearestfile2 == nullptr || nearestfile2->size() <= 0)
      return std::nullopt;

    read_object.get()->SetFiles(nearestfile2);
    file_info = read_object.get()->GetNextFile();
  }
  if (file_info == std::nullopt)
    return std::nullopt;

  if (!ParseHeader(file_info->file_path_ + "/" + file_info->file_name_, read_object))
    return std::nullopt;

  bool ret = read_object.get()->LoadDataByIndex((unsigned long long)(file_info->time_stamp_), kArchiveReadNext);
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
   // Use a local tm (gmtime_r/gmtime_s). std::gmtime() returns a process-wide static buffer,
   // and this function modifies the result, which would corrupt other threads' conversions.
   std::tm tm_utc{};
#ifdef _WIN32
   const bool converted = (gmtime_s(&tm_utc, &timestamp) == 0);
#else
   const bool converted = (gmtime_r(&timestamp, &tm_utc) != nullptr);
#endif
   if (!converted) {
     BLAZE_LOG_ERROR("[ArchiveL] getNextHourLastSecond: failed to convert timestamp [{}]", static_cast<long long>(timestamp));
     return timestamp;
   }

   if (archive_read_type == kArchiveReadNext) {
     tm_utc.tm_hour += add_hour;
     tm_utc.tm_min = 59;
     tm_utc.tm_sec = 59;
   } else {
     tm_utc.tm_hour -= add_hour;
     tm_utc.tm_min = 0;
     tm_utc.tm_sec = 0;
   }

#ifdef _WIN32
   std::time_t gm_timestamp = _mkgmtime(&tm_utc);
#else
   std::time_t gm_timestamp = timegm(&tm_utc);
#endif
   return gm_timestamp;
 }


std::map<MediaProfile, std::vector<SearchFileEntry>> ReaderWorker::GetSearchFile(const std::string session_id, const PnxMediaTime& begin_time,
                                                                                  const PnxMediaTime& end_time) {

   std::vector<std::filesystem::path> drives;
   {
     std::lock_guard<std::mutex> lock(drive_mtx_);
     for (const auto& value : drives_) {
       drives.push_back(value);
     }
   }
   std::map<MediaProfile, std::vector<SearchFileEntry>> file_list;
   std::vector<SearchFileEntry> forder_list;
   std::tm begin_time_t = GetTmFromTimestamp(begin_time.ToMilliSeconds());
   std::tm end_time_t = GetTmFromTimestamp(end_time.ToMilliSeconds());
#ifdef _WIN32
   std::time_t bt_t = _mkgmtime(&begin_time_t);
   std::time_t et_t = _mkgmtime(&end_time_t);
#else
   std::time_t bt_t = timegm(&begin_time_t);  // POSIX non-standard but commonly available
   std::time_t et_t = timegm(&end_time_t);
#endif
   // helper
   auto format_num = [](int val, int width) -> std::string {
     std::ostringstream oss;
     oss << std::setfill('0') << std::setw(width) << val;
     return oss.str();
   };

   if (bt_t == (std::time_t)-1 || et_t == (std::time_t)-1 || bt_t > et_t)
     return file_list;

   for (auto tp = std::chrono::system_clock::from_time_t(bt_t); tp <= std::chrono::system_clock::from_time_t(et_t); tp += std::chrono::hours(1)) {
     std::time_t t = std::chrono::system_clock::to_time_t(tp);
     std::cout << "";
     std::tm tm = GetTmFromTimestamp(t * 1000);

     for (auto& drive : drives) {
       std::filesystem::path p = drive;
       p /= save_path_;
       p /= session_id;
       p /= format_num(tm.tm_year + 1900, 4);
       p /= format_num(tm.tm_mon + 1, 2);
       p /= format_num(tm.tm_mday, 2);
       p /= format_num(tm.tm_hour, 2);
       if (std::filesystem::exists(p)) {
         forder_list.push_back(SearchFileEntry{p.string(), drive});
       }
     }
   }

   for (const auto& folder : forder_list) {
     for (auto& entry : std::filesystem::directory_iterator(folder.file_path)) {
       if (entry.is_regular_file()) {
         if (entry.path().filename().string().find(MediaProfile::ToString(MediaProfile::kPrimary)) != std::string::npos) {
           file_list[MediaProfile::kPrimary].push_back(SearchFileEntry{entry.path(), folder.drive, MediaProfile::kPrimary});
         } else {
           file_list[MediaProfile::kSecondary].push_back(SearchFileEntry{entry.path(), folder.drive, MediaProfile::kSecondary});
         }
       }
     }
   }

   for (auto& [profile, files] : file_list) {
     std::sort(files.begin(), files.end(),
               [](const SearchFileEntry& a, const SearchFileEntry& b) { return a.file_path.filename().string() < b.file_path.filename().string(); });
   }
   return file_list;
 }

 std::vector<std::shared_ptr<ArchiveTime>> ReaderWorker::GetTimeInfo(std::vector<SearchFileEntry> files, const PnxMediaTime& begin_time, const PnxMediaTime& end_time,
                                                      MediaProfile profile) {
   std::vector<std::shared_ptr<ArchiveTime>> archiving_times;
   std::shared_ptr<ReaderObject> read_object = std::make_shared<ReaderObject>(MediaProfile::kAuto);

   const unsigned long long elapsed_time = 2 * 1000;
   unsigned long long main_begin = 0, main_end = 0, base_begin_time = begin_time.ToMilliSeconds(), base_end_time = end_time.ToMilliSeconds();
   std::filesystem::path main_drive;
   bool has_main = false;

   // Filter: keep from the file just before begin_time up to end_time
   {
     auto it_start = files.begin();
     auto it_stop = files.begin();
     for (auto it = files.begin(); it != files.end(); ++it) {
       unsigned long long ts = FileSearcher::ParseFileNameToTimestamp(it->file_path.filename().string());
       if (ts <= base_begin_time)
         it_start = it;
       if (ts <= base_end_time) {
         it_stop = it;
       }
     }
     if (it_stop != files.end())
       ++it_stop;
     files = std::vector<SearchFileEntry>(it_start, it_stop);
   }

   for (auto& file : files) {
     if (!ParseHeader(file.file_path.string(), read_object))
       continue;

     std::shared_ptr<FileIndexer> indexer = read_object->GetIndexer();
     std::shared_ptr<ArchiveIndexHeader> index_header = indexer->GetFirstHeader();

     while (index_header != nullptr) {
       if (index_header->type != ArchiveType::kArchiveTypeGopVideo) {
         index_header = indexer->GetNextIndex();
         continue;
       }

       unsigned long long chunk_begin = index_header->begin_timestamp_msec;
       unsigned long long chunk_end = index_header->end_timestamp_msec;
       if ((base_begin_time > chunk_begin && base_begin_time > chunk_end) || (base_end_time < chunk_begin && base_end_time < chunk_end)) {
         index_header = indexer->GetNextIndex();
         continue;
       }

       if (!has_main) {
         main_begin = chunk_begin;
         main_end = chunk_end;
         main_drive = file.drive;
         has_main = true;
       } else {
         bool is_far = (chunk_begin > main_end + elapsed_time) || (chunk_end + elapsed_time < main_begin) || (file.drive != main_drive);
         if (is_far) {
           archiving_times.push_back(std::make_shared<ArchiveTime>(main_drive, profile, main_begin, main_end));
           main_begin = chunk_begin;
           main_end = chunk_end;
           main_drive = file.drive;
         } else {
           main_begin = std::min(main_begin, chunk_begin);
           main_end = std::max(main_end, chunk_end);
         }
       }

       index_header = indexer->GetNextIndex();
     }
   }

   if (has_main)
     archiving_times.push_back(std::make_shared<ArchiveTime>(main_drive, profile, main_begin, main_end));

   return archiving_times;
 }

uint64_t ReaderWorker::GetArchiveSize(const std::vector<SearchFileEntry>& files, const PnxMediaTime& begin_time, const PnxMediaTime& end_time,
                                      MediaProfile profile, uint64_t& recording_msec) {
  uint64_t archive_size = 0;
  recording_msec = 0;
  std::shared_ptr<ReaderObject> read_object = std::make_shared<ReaderObject>(MediaProfile::kAuto);
  unsigned long long base_begin_time = begin_time.ToMilliSeconds(), base_end_time = end_time.ToMilliSeconds();
  std::vector<std::filesystem::path> profile_files;
  for (const auto& file : files) {
    std::string file_name = file.file_path.filename().string();
    unsigned long long ts = FileSearcher::ParseFileNameToTimestamp(file_name);
    if (ts > 0 && ts <= base_end_time && std::string::npos != file_name.find(profile.ToString()))
      profile_files.push_back(file.file_path);
  }

  std::vector<std::pair<unsigned long long, unsigned long long>> recorded_spans;

  for (const auto& file_path : profile_files) {
    if (!ParseHeader(file_path.string(), read_object))
      continue;

    std::shared_ptr<FileIndexer> indexer = read_object->GetIndexer();
    std::shared_ptr<ArchiveIndexHeader> index_header = indexer->GetFirstHeader();
    while (index_header != nullptr) {
      bool is_in_range = index_header->end_timestamp_msec >= base_begin_time && index_header->begin_timestamp_msec <= base_end_time;
      if (index_header->type == ArchiveType::kArchiveTypeGopVideo && is_in_range && index_header->packet_size > 0) {
        archive_size += static_cast<uint64_t>(index_header->packet_size);

        unsigned long long span_begin = std::max<unsigned long long>(index_header->begin_timestamp_msec, base_begin_time);
        unsigned long long span_end = std::min<unsigned long long>(index_header->end_timestamp_msec, base_end_time);
        if (span_begin <= span_end)  // a broken entry (begin > end) is not counted as time
          recorded_spans.emplace_back(span_begin, span_end);
      }

      index_header = indexer->GetNextIndex();
    }
  }

  // Recording time = length of the union of the spans. A span ends at the last frame of its GOP and the next GOP starts one frame later
  std::sort(recorded_spans.begin(), recorded_spans.end());
  const unsigned long long elapsed_time = 2 * 1000;
  unsigned long long main_begin = 0, main_end = 0;
  bool has_main = false;
  for (const auto& [span_begin, span_end] : recorded_spans) {
    if (has_main && span_begin <= main_end + elapsed_time) {
      main_end = std::max(main_end, span_end);
      continue;
    }

    if (has_main)
      recording_msec += main_end - main_begin;
    main_begin = span_begin;
    main_end = span_end;
    has_main = true;
  }

  if (has_main)
    recording_msec += main_end - main_begin;

  return archive_size;
}

 std::tm ReaderWorker::GetTmFromTimestamp(unsigned long long data_time) {
   std::time_t secs = static_cast<std::time_t>(data_time / 1000);
   std::tm tm{};
#ifdef _WIN32
   gmtime_s(&tm, &secs);  // Windows thread-safe variant
#else
   gmtime_r(&secs, &tm);  // POSIX
#endif
   return tm;
 }