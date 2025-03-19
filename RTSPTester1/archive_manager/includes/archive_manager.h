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
 * @file    archive_manager.h
 * @brief
 * @author    cd.kim@hanwha.com
 *
 * @date    2024.06.19
 * @version    1.0
 */

#pragma once
#include <boost/asio.hpp>
#include <iostream>
#include <map>
#include <vector>
#include <shared_mutex>
#include "reader_object.h"

#include "archive_worker.h"
#include "reader_worker.h"

#include "phoenix/define.h"

class ARCHIVE_MANAGER_API ArchiveManager {
 public:
  ArchiveManager();
  ArchiveManager(const ArchiveManager&) = delete;
  ArchiveManager& operator=(const ArchiveManager&) = delete;
  ~ArchiveManager(){};

  static ArchiveManager* getInstance() {
    if (archive_manager_ == nullptr) {
      std::lock_guard<std::mutex> lock(mtx);
      if (archive_manager_ == nullptr) {
        archive_manager_ = new ArchiveManager();
      }
    }
    return archive_manager_;
  }

  void InitailData();
  void SetSavePath(std::filesystem::path save_path);
  std::filesystem::path GetSavePath() { return save_path_; }
  std::map<std::string, std::tuple<bool, int>> GetSaveStatus();
  bool AddDevice(std::string device_driver);
  bool AddSession(std::string device_driver, SessionID session_id, MediaProfile profile);
  bool DeleteDevice(std::string device_driver);
  bool DeleteStream(SessionID session_id, MediaProfile profile);
  bool CheckWorkerDrive(std::string session_id, std::string save_driver, std::shared_ptr<ArchiveWorker> workerptr);
  bool SetDataEncryption(SessionID session_id, MediaProfile profile, EncryptionType encrytion_type);
  bool Flush(SessionID session_id, MediaProfile profile);

  // ********** write API **********
  bool PushDataGroup(std::shared_ptr<PnxMediaArchiveDataGroup> data_group);

  // ********** Read API **********
  bool Seek(ChannelUUID channel, const PnxMediaTime& time, ArchiveReadType archive_read_type, std::shared_ptr<ReaderObject> read_object);

  std::shared_ptr<ArchiveChunkBuffer> GetStreamChunk(ArchiveChunkReadType gov_read_type, std::shared_ptr<ReaderObject> read_object);
  std::shared_ptr<ArchiveChunkBuffer> GetStreamGop(ArchiveChunkReadType gov_read_type, std::shared_ptr<ReaderObject> read_object);
  // GetNextData,GetPrevData :  Data is provided according to ArchiveType. If none, it is provided sequentially.
  std::shared_ptr<PnxMediaDataGroup> GetNextData(std::shared_ptr<ReaderObject> read_object, const unsigned char cseq,
                                                  ArchiveType get_video_type = kArchiveTypeNone);
  // GetPrevData : In the case of video, it is delivered in gop units.
  std::shared_ptr<BaseBuffer> GetPrevData(std::shared_ptr<ReaderObject> read_object, ArchiveType get_video_type = kArchiveTypeNone);

  // GetMomory Frame
  std::shared_ptr<ArchiveChunkBuffer> GetMemoryFrames(std::string session_id, MediaProfile profile, int64_t timestamp);
  
  // ********** Return Msg **********
  //callback aw -> am
  void CallbackWirteStatusAWTAM(SessionID session_id, MediaProfile profile, bool success, std::shared_ptr<std::vector<FrameWriteIndexData>> list);
  void CallbackWriteResult(SessionID session_id, MediaProfile profile, 
      const std::function<void(ChannelUUID channel, MediaProfile profile, bool is_success, std::shared_ptr<std::vector<FrameWriteIndexData>> infos)>& cb_func);

  void DeleteCallbackFunction(SessionID session_id, MediaProfile profile);
 private:
  std::filesystem::path save_path_;
  std::map<std::string, std::shared_ptr<ArchiveWorker>> archive_workers;  // key:  Drive
  std::map<std::string, std::string> device_session_index;
  std::shared_mutex mtx_worker;
  std::shared_mutex callback_mtx;
  ReaderWorker reader_worker_;
  std::unordered_map<std::string, std::function<void(SessionID session_id, MediaProfile profile, bool success, std::shared_ptr<std::vector<FrameWriteIndexData>>)>>
      callback_writeresults_;
  static ArchiveManager* archive_manager_;
  static std::mutex mtx;
};
