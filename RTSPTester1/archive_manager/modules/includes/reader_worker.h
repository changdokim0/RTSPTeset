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
 * @file    reader_worker.h
 * @brief
 * @author    cd.kim@hanwha.com
 *
 * @date    2024.06.19
 * @version    1.0
 */

#pragma once
#define _CRT_SECURE_NO_WARNINGS

#include <optional>
#include <mutex>
#include <ctime>
#include <spdlog/spdlog.h>

#include "reader_object.h"
#include "file_indexer.h"
#include "archive_object.h"
#include "file_searcher.h"
#include "archive_chunk_buffer.h"

class ReaderWorker 
{
 public:
  ReaderWorker();
  ~ReaderWorker();
  void AddDrive(std::filesystem::path drive);
  void DelDrive(std::filesystem::path drive);
  void SetData(std::filesystem::path save_path);

  std::shared_ptr<std::deque<Archive_FileInfo>> GetNearestFile(std::vector<std::filesystem::path> drives, ChannelUUID channel_uuid, unsigned int time_stamp,
                                                                MediaProfile profile_type, ArchiveReadType archive_read_type, bool is_include_curfile = false);

  bool Seek(ChannelUUID channel_uuid, unsigned long long time_stamp_msec, ArchiveReadType archive_read_type, std::shared_ptr<ReaderObject> read_object);
  std::shared_ptr<ArchiveChunkBuffer> GetStreamChunk(ArchiveChunkReadType gov_read_type, std::shared_ptr<ReaderObject> read_object);
  std::shared_ptr<ArchiveChunkBuffer> GetStreamGop(ArchiveChunkReadType gov_read_type, std::shared_ptr<ReaderObject> read_object);

  std::shared_ptr<BaseBuffer> GetPrevData(ArchiveType get_video_type, std::shared_ptr<ReaderObject> read_object);
  std::optional<std::vector<std::shared_ptr<StreamBuffer>>> GetNextData(ArchiveType get_video_type, std::shared_ptr<ReaderObject> read_object);
  bool IsInGOP(const PnxMediaTime& time, std::shared_ptr<ReaderObject> reader_object);

 private:
  std::vector<std::filesystem::path> drives_;
  std::filesystem::path save_path_;
  std::mutex drive_mtx_;
  std::filesystem::path drive_;

  bool ParseHeader(std::string file_name, std::shared_ptr<ReaderObject> read_object);
  time_t getNextHourLastSecond(time_t timestamp, int add_hour, ArchiveReadType archive_read_type);
  std::optional<std::vector<std::shared_ptr<StreamBuffer>>> GetNextFrameIfFileUpdated(ArchiveType get_video_type, std::shared_ptr<ReaderObject> read_object);
};
