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
#include <set>
#include <vector>
#include <shared_mutex>
#include <optional>
#include <unordered_map>
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

  static ArchiveManager* getInstance();

  void InitailData();
  void SetSavePath(std::filesystem::path save_path);
  std::filesystem::path GetSavePath() { return save_path_; }
  std::map<std::string, std::tuple<bool, int>> GetSaveStatus();
  bool AddDevice(std::string device_driver);
  bool HasDevice(std::string device_driver);
  // Returns a short RAID description of the mount backing `path` (e.g. "raid5 disks=4 chunk=512K full_stripe=1536K"), or "none" if not on md RAID1/5/6.
  std::string GetRaidInfo(const std::string& path);
  bool AddSession(std::string device_driver, SessionID session_id);
  bool DeleteDevice(std::string device_driver);
  bool DeleteAllDevices(void);
  bool DeleteStream(SessionID session_id);
  bool SetDataEncryption(SessionID session_id, EncryptionType encrytion_type);
  bool Flush(SessionID session_id, FlushType flush_type = kFlushTypeNone);
  void ChangeArchiveSystemUUID(std::optional<std::string> prev_system_uuid, std::string target_system_uuid, std::optional<std::string> prev_server_uuid,
                               std::string target_server_uuid, std::string db_separator_name, std::function<void(bool)> callback);
  int GetOpenFileCount();
  void Destroy();
  void Restart();
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
  
  // Reindexing API
  std::optional<unsigned long long> GetCurrentFileStartTime(std::string channel_uuid, MediaProfile profile_type);
  std::map<MediaProfile, std::vector<std::shared_ptr<ArchiveTime>>> GetRecordTime(const std::string session_id, const PnxMediaTime& begin_time, const PnxMediaTime& end_time);

  // Backup API : size (byte) of the recorded chunks overlapping [begin_time, end_time], read from the file index headers only.
  // recording_msec : recorded time inside [begin_time, end_time]
  uint64_t GetArchiveSize(const std::string& channel_uuid, MediaProfile profile, const PnxMediaTime& begin_time, const PnxMediaTime& end_time,
                          uint64_t& recording_msec);

  // ********** Return Msg **********
  //callback aw -> am
  void CallbackWirteStatusAWTAM(SessionID session_id, bool success, std::shared_ptr<std::vector<FrameWriteIndexData>> list);
  void CallbackWriteResult(SessionID session_id,
                           const std::function<void(SessionID session_id, bool is_success, std::shared_ptr<std::vector<FrameWriteIndexData>> infos)>& cb_func);
  void DeleteCallbackFunction(SessionID session_id);

  // Channel metadata mapping
  void RegisterChannelMetadata(const std::string& channel_uuid, const std::optional<int>& channel_index, const std::optional<std::string>& mac_address, const std::optional<std::string>& ip_address);
  void UnregisterChannelMetadata(const std::string& channel_uuid);
  // mac_address is empty for ip camera, ip_address is kept as is when empty
  void UpdateChannelAddress(const std::string& channel_uuid, const std::string& mac_address, const std::string& ip_address, int channel_index);

 private:
  std::filesystem::path save_path_;
  std::map<std::string, std::shared_ptr<ArchiveWorker>> archive_workers;  // key:  Drive
  std::map<std::string, std::string> device_session_index;
  std::set<std::string> registered_drives_;  // Drives added by AddDevice. Removed only by DeleteDevice (not cleared by Destroy)
  std::shared_mutex mtx_registered_drives_;
  std::shared_mutex mtx_worker;
  std::shared_mutex callback_mtx;
  ReaderWorker reader_worker_;
  std::unordered_map<std::string, std::function<void(SessionID session_id, bool success, std::shared_ptr<std::vector<FrameWriteIndexData>>)>>
      callback_writeresults_;
  
  std::unordered_map<std::string, std::tuple<std::optional<int>, std::optional<std::string>, std::optional<std::string>>> channel_metadata_map_;
  std::shared_mutex channel_metadata_mutex_;
  
  static ArchiveManager* archive_manager_;
  std::atomic<bool> destroyed_{false};
  bool CheckWorkerDrive(std::string session_id, std::string save_driver, std::shared_ptr<ArchiveWorker> workerptr);
};
