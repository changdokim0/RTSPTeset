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
 * @file    archive_worker.h
 * @brief
 * @author    cd.kim@hanwha.com
 *
 * @date    2024.06.19
 * @version    1.0
 */

#pragma once
#include <iostream>
#include <map>
#include <vector>
#include <thread>
#include <condition_variable>
#include <functional>

#include "io_worker.h"
#include "reader_object.h"

// Class to be associated with storage input/output at the drive level
class ArchiveWorker {
 public:
  ArchiveWorker(std::string driver);
  ~ArchiveWorker();

  // device
  void SetSavePath(std::filesystem::path save_path) { save_path_ = save_path; }

  std::shared_ptr<IOWorker> GetIOWorker(SessionID session_id);
  std::map<std::string, std::shared_ptr<IOWorker>> GetIOWorkers() { return io_workers_; };
  bool AddStream(SessionID session_id, MediaProfile profile);
  bool DeleteStream(SessionID session_id, MediaProfile profile);
  bool Flush(SessionID session_id, MediaProfile profile);
  bool RemoveIOWorker(std::string stream_id);
  void PushStream(SessionID session_id, std::shared_ptr<IOWorker> io_worker);
  bool SetDataEncryption(SessionID session_id, MediaProfile profile, EncryptionType encrytion_type);
  bool PushVideoChunkBuffer(SessionID session_id, MediaProfile profile, std::string save_driver, std::shared_ptr<ArchiveChunkBuffer> media_datas);

  // information
  std::optional<std::pair<ArchiveIndexHeader, ArchiveObject>> GetFrame(SessionID session_id, int time_stamp, std::shared_ptr<ReaderObject> read_object);
  std::optional<std::pair<ArchiveIndexHeader, ArchiveObject>> GetFrame(SessionID session_id, int time_stamp, std::string frame_full_path,
                                                                       std::shared_ptr<ReaderObject> read_object);
  std::optional<std::pair<ArchiveIndexHeader, ArchiveObject>> GetPrevFrame(SessionID session_id, std::shared_ptr<ReaderObject> read_object);
  std::optional<std::pair<ArchiveIndexHeader, ArchiveObject>> GetNextFrame(SessionID session_id, std::shared_ptr<ReaderObject> read_object);

  //callback aw -> am
  std::function<void(SessionID session_id, MediaProfile profile, bool success, std::shared_ptr<std::vector<FrameWriteIndexData>>)> callback_write_status_awtam_;
  void CallbackWirteStatusAWTAM(SessionID session_id, MediaProfile profile, bool success, std::shared_ptr<std::vector<FrameWriteIndexData>> infos) {
    if (callback_write_status_awtam_) {
      callback_write_status_awtam_(session_id, profile, success, infos);
    }
  }

  //callback iow -> aw
  void CallbackWirteStatusIWTAW(SessionID session_id, MediaProfile profile, bool success, std::shared_ptr<std::vector<FrameWriteIndexData>> value);

 private:
  std::filesystem::path driver_ = "";
  std::filesystem::path save_path_ = "";
  void RunWorkThread();
  std::map<std::string, std::shared_ptr<IOWorker>> io_workers_; 
  std::shared_mutex mtx_ioworkers_, mtx_io_delete_;

  //thread
  bool run_thread_ = false;
  std::thread* work_thread_ = nullptr;
  std::condition_variable cv_;
  std::mutex cv_mtx_;
  bool condition_release_ = false;
};
