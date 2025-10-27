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
 * @file    reader_object.h
 * @brief
 * @author    cd.kim@hanwha.com
 *
 * @date    2024.06.19
 * @version    1.0
 */

#pragma once
#include "file_indexer.h"
#include "data_loader.h"
#include "archive_define.h"
#include "reader_stream_chunk.h"
#include "archive_chunk_buffer.h"
#include "archive_fileinfo.h"
#include <spdlog/spdlog.h>

class ARCHIVE_MANAGER_API ReaderObject {
 public:
  ReaderObject(MediaProfile profile_type);
  ~ReaderObject();

  unsigned int allowable_time_ = 0;
  ReadFrameType read_frame_type = kReadTypeAllFrame;

  std::shared_ptr<FileIndexer> GetIndexer();
  std::shared_ptr<DataLoader> GetDataLoader();

  MediaProfile GetProfile() { return profile_type_; }
  std::string FileName() { return cur_file_name_; }
  uintmax_t FileSize() { return cur_file_size_; }
  std::ifstream& GetFileHandle() { return file_handle_; }
  unsigned long long GetCurrentPositionTime() { return read_frame_position_msec_; }
  unsigned long long GetChunkEndTime();
  unsigned int GetOpenedFileTime() { return opened_file_timestamp_; }
  std::string GetSessionid() { return session_id_; }

  bool isOpen() const;
  bool FileOpen(std::string file_name);
  bool LoadMedia(ArchiveIndexHeader& index_header);
  bool LoadDataByIndex(unsigned long long time_stamp_msec, ArchiveReadType archive_read_type);
  bool ParseMainHeader();
  void SetInfo(std::string session_id);
  void SetFiles(std::shared_ptr<std::deque<Archive_FileInfo>> file_list);
  std::optional<Archive_FileInfo> GetPrevFile();
  std::optional<Archive_FileInfo> GetNextFile();

  std::shared_ptr<ArchiveChunkBuffer> GetStreamChunk(ArchiveChunkReadType GovReadType = kArchiveChunkReadFull);
  std::shared_ptr<ArchiveChunkBuffer> GetStreamGop(ArchiveChunkReadType GovReadType = kArchiveChunkReadFull);

  std::shared_ptr<BaseBuffer> GetPrevData(ArchiveType get_video_type);
  //std::shared_ptr<BaseBuffer> GetNextData__(ArchiveType get_video_type);
  std::optional<std::vector<std::shared_ptr<StreamBuffer>>> GetNextData(ArchiveType get_video_type);
  bool IsInGOP(const PnxMediaTime& time);
  void ClearFileList();

 private:
  std::ifstream file_handle_;
  std::string cur_file_name_;
  uintmax_t cur_file_size_ = 0;
  std::string session_id_;
  unsigned int opened_file_timestamp_ = 0;
  unsigned long long read_request_time_ = 0;
  unsigned long long read_frame_position_msec_ = 0;

  ArchiveType cur_archive_type = kArchiveTypeNone;
  unsigned int file_position_ = 0;
  ReaderStreamChunk reader_stream_chunk;
  MediaProfile profile_type_;
  std::shared_ptr<std::deque<Archive_FileInfo>> file_list_;

  std::shared_ptr<FileIndexer> file_indexer_;
  std::shared_ptr<DataLoader> data_loader_;

  bool ResetToCurrentTime(ArchiveReadType archive_read_type);
};
