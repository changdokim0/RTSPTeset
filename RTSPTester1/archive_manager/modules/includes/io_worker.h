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
 * @file    io_worker.h
 * @brief
 * @author    cd.kim@hanwha.com
 *
 * @date    2024.06.19
 * @version    1.0
 */

#pragma once
#define _CRT_SECURE_NO_WARNINGS

#include <iostream>
#include <map>
#include <fstream>
#include <string>
#include <functional>
#include <optional>
#include <queue>
#include <mutex>
#include <random>
#include <shared_mutex>

#include "archive_object.h"
#include "data_file_handle.h"
#include "archive_define.h"
#include "file_indexer.h"
#include "data_loader.h"
#include "reader_object.h"
#include "archive_chunk_buffer.h"
#include "file_write_indexer.h"
#include "file_write_index_info.h"
#include "archive_object_buffer.h"
#include "archive_util.h"
#include "file_searcher.h"
#include "platform_file.h"
#include "stream_cryptor.h"
#include "bitrate_monitor.h"

#include <spdlog/spdlog.h>

class IOWorker {
 public:
  IOWorker(std::string session_id, MediaProfile profile);
  ~IOWorker();

  void SetPath(std::filesystem::path save_path);
  void SetStreamID(std::string stream_id) { stream_id_ = stream_id; }
  std::string GetStreamID() { return stream_id_; }
  std::string GetSessionID() { return session_id_; }
  void SetDataEncryption(EncryptionType encrytion_type) { encrytion_type_ = encrytion_type; }

  bool PushVideoChunkBuffer(std::filesystem::path save_driver, std::shared_ptr<ArchiveChunkBuffer> media_datas);
  bool ProcessBuffer(unsigned int timestamp);
  std::shared_ptr<ArchiveChunkBuffer> GetMemoryFrames(int64_t timestamp);

  std::tuple<bool, int> GetSaveStatus(); // record status, bitrate(5min)
  std::optional<std::pair<ArchiveIndexHeader, ArchiveObject>> GetFrame(int time_stamp);
  std::optional<std::pair<ArchiveIndexHeader, ArchiveObject>> GetPrevFrame();
  std::optional<std::pair<ArchiveIndexHeader, ArchiveObject>> GetNextFrame();
  void Flush() { reserve_flush_ = true; }

  std::function<void(SessionID session_id, MediaProfile profile, bool success, std::shared_ptr<std::vector<FrameWriteIndexData>>)> callback_write_status_iwtaw_;
  void CallbackWirteStatusIWTAW(SessionID session_id, MediaProfile profile, bool success, std::shared_ptr<std::vector<FrameWriteIndexData>> infos) {
    if (callback_write_status_iwtaw_) {
      callback_write_status_iwtaw_(session_id, profile_, success, infos);
    }
  }

 private:
  SessionID session_id_;
  std::string stream_id_ = "";
  MediaProfile profile_;
  std::filesystem::path save_driver_;
  bool change_file_ = false;
  std::shared_ptr<DataFileHandle> write_file_data_handle_;
  FileWriteIndexer write_file_indexer_handle;
  EncryptionType encrytion_type_ = kEncryptionNone;
  StreamCryptor stream_crytor_;

  int dio_buffer_nomal_size = g_dio_buffer_size_;
  int dio_buffer_busy_size = g_dio_buffer_size_;
  ArchiveObjectBuffer index_block_info_;             // Structure information of block info
  FileWriteIndexInfo index_buffer_info_;             // Pass failure time stamp information
  std::shared_ptr<char*> cur_index_data_ = nullptr;  // When all index data is used, then write
  std::filesystem::path save_path_;
  std::queue<std::shared_ptr<BaseBuffer>> media_buffers;

  FileHandle file_handle = InvalidHandle;
  std::string write_file_name_;

  // The time stamp below is intended to be used when changing the file. If it is not needed, it will be deleted.
  unsigned long long file_begin_timestamp_ = 0;
  unsigned int file_write_timestamp_ = 0;
  std::string cur_filetime;

  int file_index_last_pos_ = 0;  // pos for changing indexer location when write fails
  std::shared_mutex media_buffer_mtx_;
  //std::mutex memory_mtx_;
  int file_random_number_ = 0;
  int buffer_size_ = IOWORKER_QUEUE_MAX_COUNT;

  std::mutex mmy_buf_mtx_;
  std::vector<std::shared_ptr<BaseBuffer>> memory_buffer_frames;
  bool reserve_flush_ = false;
  // Calculation
  BitrateMonitor bitrate_monitor_;
  std::chrono::time_point<std::chrono::high_resolution_clock> write_time_;

  PlatformFile platform_file_;
  bool InitBuffer();
  bool WriteRemain(FileHandle hFile, int remain_size);
  void FireBuffer();
  bool WriteDisk(FileHandle file_handle, char* buffer, int write_size);
  bool FileClose();
  bool UpdateFileNameByCondition(FileHandle file_handle, unsigned long long timestamp);
  bool WriteFileChange(std::string filetime, unsigned long long timestamp);
  bool WriteBufferToDisk(Pnx::Media::MediaSourceFramePtr buffer_data);
  bool WriteIndexInfo(ArchiveType archive_type, int packet_size, unsigned long long begin_timestramp_msec, unsigned long long end_timestramp_msec,
                      EncryptionType encryption_type);
  bool WriteBlockInfo(std::shared_ptr<BaseBuffer> stream_buffer);
  void WriteDiskSuccess();
  void WriteDiskFail();
  void CheckBufferSize(int remain_size);
  bool CloseSavedFile();
  bool DoEncryption(std::shared_ptr<ArchiveChunkBuffer> media_datas);

  bool DeleteEmptyFolders(const std::string& folderPath);
  bool Delete_File(const std::string& filePath);
  void QueueCheck();
  void MemoryCheck();
  int GetRandomFileNumber();
  bool ProcessChunkBuffers(std::shared_ptr<ArchiveChunkBuffer> gop_buffers);
  void ProcessFlush();
};