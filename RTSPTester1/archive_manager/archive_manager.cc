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
 * @file    archive_manager.cpp
 * @brief
 * @author    cd.kim@hanwha.com
 *
 * @date    2024.06.19
 * @version    1.0
 */

#include "archive_manager.h"
#include "archive_util.h"
#include "blaze_logger.h"
#include "raid_write_optimizer.h"

ArchiveManager* ArchiveManager::archive_manager_ = nullptr;

ArchiveManager* ArchiveManager::getInstance() {
  static std::once_flag init_flag;
  std::call_once(init_flag, []() { archive_manager_ = new ArchiveManager(); });
  return archive_manager_;
}

ArchiveManager::ArchiveManager() {
  BLAZE_LOG_INFO("[ArchiveL] ArchiveManager - constructor");
  InitailData();
}

void ArchiveManager::Destroy() {
  BLAZE_LOG_INFO("[ArchiveL] ArchiveManager - Destroy +++");
  destroyed_ = true;
  std::unique_lock<std::shared_mutex> lock(mtx_worker);
  for (auto& pair : archive_workers) {
    if (pair.second) pair.second->Shutdown();
  }
  archive_workers.clear();
  device_session_index.clear();
  BLAZE_LOG_INFO("[ArchiveL] ArchiveManager - Destroy ---");
}

void ArchiveManager::Restart() {
  BLAZE_LOG_INFO("[ArchiveL] ArchiveManager - Restart");
  destroyed_ = false;
}

bool ArchiveManager::AddDevice(std::string device_driver) {
  std::unique_lock<std::shared_mutex> lock(mtx_worker);
  if (destroyed_)
    return false;

  BLAZE_LOG_INFO("[ArchiveL] ArchiveManager - AddDevice  [{}]", device_driver);
  if (archive_workers.find(device_driver) != archive_workers.end()) {
    BLAZE_LOG_ERROR("[ArchiveL] ArchiveManager - Fail : already disk  [{}]", device_driver);
    return false;
  }

  std::shared_ptr<ArchiveWorker> aw = std::make_shared<ArchiveWorker>(device_driver);
  aw->SetSavePath(save_path_);
  reader_worker_.AddDrive(device_driver);
  aw->callback_write_status_awtam_ =
      std::bind(&ArchiveManager::CallbackWirteStatusAWTAM, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);
  archive_workers.emplace(device_driver, aw);

  {
    std::unique_lock<std::shared_mutex> drive_lock(mtx_registered_drives_);
    registered_drives_.insert(device_driver);
  }
  return true;
}

bool ArchiveManager::HasDevice(std::string device_driver) {
  std::shared_lock<std::shared_mutex> lock(mtx_worker);
  return archive_workers.find(device_driver) != archive_workers.end();
}

std::string ArchiveManager::GetRaidInfo(const std::string& path) {
#if defined(__linux__)
  io_worker_internal::RaidGeometry geom;
  if (path.empty() || !io_worker_internal::TryGetRaidGeometryForPath(path, &geom)) {
    return "none";
  }

  std::string info = "raid" + std::to_string(geom.level) + " disks=" + std::to_string(geom.raid_disks);
  if (geom.level == 5 || geom.level == 6) {
    const int data_disks = geom.raid_disks - (geom.level == 5 ? 1 : 2);
    info += " chunk=" + std::to_string(geom.chunk_size / 1024) + "K";
    info += " full_stripe=" + std::to_string(geom.chunk_size * static_cast<std::size_t>(data_disks > 0 ? data_disks : 0) / 1024) + "K";
  }
  return info;
#else
  return "none";
#endif
}

bool ArchiveManager::AddSession(std::string device_driver, SessionID session_id) {
  if (destroyed_)
    return false;

  // Retrieve channel metadata
  std::optional<int> channel_index;
  std::optional<std::string> mac_address;
  std::optional<std::string> ip_address;
  {
    std::shared_lock<std::shared_mutex> lock(channel_metadata_mutex_);
    auto it = channel_metadata_map_.find(session_id.channel_uuid);
    if (it != channel_metadata_map_.end()) {
      channel_index = std::get<0>(it->second);
      mac_address = std::get<1>(it->second);
      ip_address = std::get<2>(it->second);
    }
  }
  
  BLAZE_LOG_INFO("[ArchiveL] ArchiveManager - AddSession  [{}][{}][{}] channel_index:[{}] mac_address:[{}] ip_address:[{}]", 
                 device_driver, session_id.channel_uuid, session_id.profile.ToString(),
                 channel_index.has_value() ? std::to_string(channel_index.value()) : "none",
                 mac_address.has_value() ? mac_address.value() : "none",
                 ip_address.has_value() ? ip_address.value() : "none");
  std::unique_lock<std::shared_mutex> lock(mtx_worker);
  if (archive_workers.find(device_driver) == archive_workers.end()) {
    BLAZE_LOG_ERROR("[ArchiveL] ArchiveManager - Fail : already session  [{}][{}][{}]", device_driver, session_id.channel_uuid, session_id.profile.ToString());
    return false;
  }

  std::string stream_id = GetStreamID(session_id);
  if (device_session_index.find(stream_id) != device_session_index.end()) {
    BLAZE_LOG_ERROR("[ArchiveL] ArchiveManager - Fail : session already exists in driver[{}] [{}][{}][{}]",
                    device_session_index[stream_id], device_driver, session_id.channel_uuid, session_id.profile.ToString());
    return false;
  }
  archive_workers[device_driver]->AddStream(session_id);
  std::shared_ptr<IOWorker> io_worker = archive_workers[device_driver]->GetIOWorker(stream_id);
  if (io_worker) {
    io_worker->SetChannelMetadata(channel_index, mac_address, ip_address);
  }
  
  device_session_index.emplace(stream_id, device_driver);

  return true;
}

bool ArchiveManager::DeleteDevice(std::string device_driver) {
  BLAZE_LOG_INFO("[ArchiveL] ArchiveManager - DeleteDevice  [{}]", device_driver);
  std::unique_lock<std::shared_mutex> lock(mtx_worker);
  archive_workers.erase(device_driver);
  {
    std::unique_lock<std::shared_mutex> drive_lock(mtx_registered_drives_);
    registered_drives_.erase(device_driver);
  }
  return true;
}

void ArchiveManager::RegisterChannelMetadata(const std::string& channel_uuid, const std::optional<int>& channel_index, const std::optional<std::string>& mac_address, const std::optional<std::string>& ip_address) {
  std::unique_lock<std::shared_mutex> lock(channel_metadata_mutex_);
  channel_metadata_map_[channel_uuid] = std::make_tuple(channel_index, mac_address, ip_address);
  BLAZE_LOG_INFO("[ArchiveL] RegisterChannelMetadata - channel:[{}] index:[{}] mac:[{}] ip:[{}]",
                 channel_uuid,
                 channel_index.has_value() ? std::to_string(channel_index.value()) : "none",
                 mac_address.has_value() ? mac_address.value() : "none",
                 ip_address.has_value() ? ip_address.value() : "none");
}

void ArchiveManager::UnregisterChannelMetadata(const std::string& channel_uuid) {
  std::unique_lock<std::shared_mutex> lock(channel_metadata_mutex_);
  channel_metadata_map_.erase(channel_uuid);
  BLAZE_LOG_INFO("[ArchiveL] UnregisterChannelMetadata - channel:[{}]", channel_uuid);
}

// Running IOWorker re-creates .deviceinfo with its own MAC/IP on every new file, so update it as well
void ArchiveManager::UpdateChannelAddress(const std::string& channel_uuid, const std::string& mac_address, const std::string& ip_address,
                                          int channel_index) {
  // mac address is empty for ip camera
  auto mac = mac_address.empty() ? std::nullopt : std::optional<std::string>(mac_address);
  {
    std::unique_lock<std::shared_mutex> lock(channel_metadata_mutex_);
    auto it = channel_metadata_map_.find(channel_uuid);
    if (it != channel_metadata_map_.end()) {
      std::get<0>(it->second) = channel_index;
      std::get<1>(it->second) = mac;
      if (!ip_address.empty())
        std::get<2>(it->second) = ip_address;
    }
  }
  std::shared_lock<std::shared_mutex> lock(mtx_worker);
  for (const auto& worker : archive_workers) {
    for (const auto& io_worker : worker.second->GetIOWorkers()) {
      if (io_worker.second->GetChannelUUID() == channel_uuid) {
        auto ip = ip_address.empty() ? io_worker.second->GetIpAddress() : std::optional<std::string>(ip_address);
        io_worker.second->SetChannelMetadata(channel_index, mac, ip);
      }
    }
  }
  BLAZE_LOG_INFO("[ArchiveL] UpdateChannelAddress - channel:[{}] mac:[{}] ip:[{}] index:[{}]", channel_uuid, mac_address, ip_address, channel_index);
}

bool ArchiveManager::DeleteStream(SessionID session_id) {
  BLAZE_LOG_INFO("[ArchiveL] ArchiveManager - DeleteStream  [{}][{}]", session_id.channel_uuid, session_id.profile.ToString());
  std::string stream_id = GetStreamID(session_id);

  bool any_deleted = false;

  {
    std::unique_lock<std::shared_mutex> lock(mtx_worker);
    device_session_index.erase(stream_id);
    for (const auto& worker_pair : archive_workers) {
      if (worker_pair.second != nullptr) {
        if (worker_pair.second->DeleteStream(session_id)) {
          BLAZE_LOG_INFO("[ArchiveL] ArchiveManager - DeleteStream succeeded in worker [{}] for [{}][{}]", worker_pair.first, session_id.channel_uuid,
                         session_id.profile.ToString());
          any_deleted = true;
        }
      }
    }
  }

  if (!any_deleted) {
    BLAZE_LOG_WARN("[ArchiveL] ArchiveManager - DeleteStream: stream not found in any worker [{}][{}]", session_id.channel_uuid, session_id.profile.ToString());
  }

  return any_deleted;
}
bool ArchiveManager::Flush(SessionID session_id, FlushType flush_type) {
  std::string stream_id = GetStreamID(session_id);
  BLAZE_LOG_INFO("[ArchiveL] Flush : session_id [{}], profile : {}\n", session_id.channel_uuid, session_id.profile.ToString());
  std::shared_ptr<ArchiveWorker> workerptr = nullptr;
  {
    std::shared_lock<std::shared_mutex> lock(mtx_worker);
    if (device_session_index.find(stream_id) == device_session_index.end()) {
      return false;
    }
    workerptr = archive_workers[device_session_index[stream_id]];
  }
  if (workerptr == nullptr)
    return false;

  return workerptr->Flush(session_id, flush_type);
}

void ArchiveManager::ChangeArchiveSystemUUID(std::optional<std::string> prev_system_uuid, std::string target_system_uuid,
                                             std::optional<std::string> prev_server_uuid, std::string target_server_uuid, std::string db_separator_name,
                                             std::function<void(bool)> callback) {

  BLAZE_LOG_INFO("[ArchiveL] Begin. prev_system_uuid:{}, target_system_uuid:{}, prev_server_uuid:{}, target_server_uuid:{}, db_separator_name:{}",
                 prev_system_uuid.has_value() ? prev_system_uuid.value() : "(none)",
                 target_system_uuid,
                 prev_server_uuid.has_value() ? prev_server_uuid.value() : "(none)",
                 target_server_uuid, db_separator_name);

  if (prev_system_uuid == target_system_uuid && prev_server_uuid == target_server_uuid) {
    BLAZE_LOG_INFO("[ArchiveL] No changes in system or server UUIDs. Skipping move operation.");
    callback(true);
    return;
  }

  std::thread([=, this]() {
    std::filesystem::path save_root_path = save_path_.parent_path().parent_path();
    if (save_root_path.empty() || save_root_path == save_path_) {
      BLAZE_LOG_ERROR("[ArchiveL] save_path_ is too shallow or empty: '{}'. Cannot determine archive root. Aborting.",
                      save_path_.string());
      callback(false);
      return;
    }

    std::filesystem::path target_base_path = save_root_path / target_system_uuid / target_server_uuid;
    BLAZE_LOG_INFO("[ArchiveL] save_root_path: '{}', target_base_path: '{}'",
                   save_root_path.string(), target_base_path.string());

    std::vector<std::filesystem::path> drives;
    {
      std::shared_lock<std::shared_mutex> lock(mtx_registered_drives_);
      for (const auto& drive : registered_drives_) {
        drives.push_back(drive);
      }
    }
    if (drives.empty()) {
      BLAZE_LOG_WARN("[ArchiveL] No drives found for moving archive data.");
      callback(false);
      return;
    }

    for (const auto& driver : drives) {
      BLAZE_LOG_INFO("[ArchiveL] Processing drive: {}", driver.string());

      std::error_code ec_driver;
      if (!std::filesystem::exists(driver, ec_driver) || !std::filesystem::is_directory(driver, ec_driver)) {
        BLAZE_LOG_WARN("[ArchiveL] Skipping invalid or unmounted drive path: {} (ec: {})",
                       driver.string(), ec_driver.message());
        continue;
      }

      try {
        std::filesystem::path target_path = driver / target_base_path;
        std::vector<std::filesystem::path> existing_channels;

        if (!std::filesystem::exists(target_path)) {
          std::error_code ec;
          if (!std::filesystem::create_directories(target_path, ec)) {
            BLAZE_LOG_ERROR("[ArchiveL]Failed to create directory: {} ({}). Skipping this drive.",
                            target_path.string(), ec.message());
            continue;
          } else {
            BLAZE_LOG_INFO("[ArchiveL] Created directory: {}", target_path.string());
          }
        } else {
          std::error_code ec_target_it;
          std::filesystem::directory_iterator it(target_path, ec_target_it);
          if (ec_target_it) {
            BLAZE_LOG_ERROR("[ArchiveL] Error opening target_path {}: {}", target_path.string(), ec_target_it.message());
          }
          for (; !ec_target_it && it != std::filesystem::directory_iterator(); it.increment(ec_target_it)) {
            if (ec_target_it) {
              BLAZE_LOG_ERROR("[ArchiveL] Error iterating target_path {}: {}", target_path.string(), ec_target_it.message());
              break;
            }
            if (it->is_directory()) {
              existing_channels.push_back(it->path());
            }
          }
        }

        std::filesystem::path save_path = driver / save_root_path;

        std::error_code ec_save_check;
        if (!std::filesystem::exists(save_path, ec_save_check) || !std::filesystem::is_directory(save_path, ec_save_check)) {
          BLAZE_LOG_WARN("[ArchiveL] Skipping invalid save_path: {} (ec: {})", save_path.string(), ec_save_check.message());
          continue;
        }

        std::error_code ec_sys_it;
        std::filesystem::directory_iterator sys_it(save_path, ec_sys_it);
        if (ec_sys_it) {
          BLAZE_LOG_ERROR("[ArchiveL] Error opening save_path {}: {}", save_path.string(), ec_sys_it.message());
        }
        for (; !ec_sys_it && sys_it != std::filesystem::directory_iterator(); sys_it.increment(ec_sys_it)) {
          if (ec_sys_it) {
            BLAZE_LOG_ERROR("[ArchiveL] Error iterating save_path {}: {}", save_path.string(), ec_sys_it.message());
            break;
          }
          const auto& system_entry = *sys_it;
          if (!system_entry.is_directory()) {
            BLAZE_LOG_INFO("[ArchiveL] Skipping non-directory system entry: {}", system_entry.path().string());
            continue;
          }

          std::string system_name = system_entry.path().filename().string();
          if (prev_system_uuid.has_value() && system_name != prev_system_uuid.value()) {
            BLAZE_LOG_INFO("[ArchiveL] Skipping system dir (UUID mismatch): {} (expected: {})", system_name, prev_system_uuid.value());
            continue;
          }

          BLAZE_LOG_INFO("[ArchiveL] Directory: {}", system_entry.path().string());

          std::error_code ec_srv_it;
          std::filesystem::directory_iterator srv_it(system_entry.path(), ec_srv_it);
          if (ec_srv_it) {
            BLAZE_LOG_ERROR("[ArchiveL] Error opening system_entry {}: {}", system_entry.path().string(), ec_srv_it.message());
          }
          for (; !ec_srv_it && srv_it != std::filesystem::directory_iterator(); srv_it.increment(ec_srv_it)) {
            if (ec_srv_it) {
              BLAZE_LOG_ERROR("[ArchiveL] Error iterating system_entry {}: {}", system_entry.path().string(), ec_srv_it.message());
              break;
            }
            const auto& server_entry = *srv_it;
            if (!server_entry.is_directory()) {
              BLAZE_LOG_INFO("[ArchiveL] Skipping non-directory server entry: {}", server_entry.path().string());
              continue;
            }

            std::string server_name = server_entry.path().filename().string();
            if (system_name == target_system_uuid && server_name == target_server_uuid) {
              BLAZE_LOG_INFO("[ArchiveL] Skipping target server directory itself: {}", server_entry.path().string());
              continue;
            }
            if (prev_server_uuid.has_value() && server_name != prev_server_uuid.value()) {
              BLAZE_LOG_INFO("[ArchiveL] Skipping server dir (UUID mismatch): {} (expected: {})", server_name, prev_server_uuid.value());
              continue;
            }

            BLAZE_LOG_INFO("[ArchiveL] SubDirectory: {}", server_entry.path().string());

            std::error_code ec_ch_it;
            std::filesystem::directory_iterator ch_it(server_entry.path(), ec_ch_it);
            if (ec_ch_it) {
              BLAZE_LOG_ERROR("[ArchiveL] Error opening server_entry {}: {}", server_entry.path().string(), ec_ch_it.message());
            }
            for (; !ec_ch_it && ch_it != std::filesystem::directory_iterator(); ch_it.increment(ec_ch_it)) {
              if (ec_ch_it) {
                BLAZE_LOG_ERROR("[ArchiveL] Error iterating server_entry {}: {}", server_entry.path().string(), ec_ch_it.message());
                break;
              }
              const auto& channel_entry = *ch_it;
              if (!channel_entry.is_directory()) {
                BLAZE_LOG_INFO("[ArchiveL] Skipping non-directory channel entry: {}", channel_entry.path().string());
                continue;
              }

              std::string channel_name = channel_entry.path().filename().string();
              if (channel_name == db_separator_name) {
                BLAZE_LOG_INFO("[ArchiveL] Skipping db_separator channel: {}", channel_name);
                continue;
              }
              if (std::any_of(existing_channels.begin(), existing_channels.end(),
                              [&](const std::filesystem::path& p) { return p.filename() == channel_name; })) {
                BLAZE_LOG_INFO("[ArchiveL] Skipping channel already exists in target: {}", channel_name);
                continue;
              }

              std::filesystem::path dest_path = target_path / channel_name;
              std::error_code ec_rename;
              std::filesystem::rename(channel_entry.path(), dest_path, ec_rename);
              if (ec_rename) {
                BLAZE_LOG_ERROR("[ArchiveL] Failed to move channel folder: {} -> {} ({})",
                                channel_entry.path().string(), dest_path.string(), ec_rename.message());
              } else {
                BLAZE_LOG_INFO("[ArchiveL] Moved channel folder: {} -> {}", channel_entry.path().string(), dest_path.string());
              }
            }
            ArchiveUtil::RemoveIfEmpty(server_entry.path(), "server");
          }
          ArchiveUtil::RemoveIfEmpty(system_entry.path(), "system");
        }
      } catch (const std::filesystem::filesystem_error& fe) {
        BLAZE_LOG_ERROR("[ArchiveL] filesystem_error on drive {}: {}", driver.string(), fe.what());
      } catch (const std::exception& e) {
        BLAZE_LOG_ERROR("[ArchiveL] exception on drive {}: {}", driver.string(), e.what());
      } catch (...) {
        BLAZE_LOG_ERROR("[ArchiveL] unknown exception on drive {}", driver.string());
      }
    }
    callback(true);
  }).detach();
}

void ArchiveManager::SetSavePath(std::filesystem::path save_path) {
  BLAZE_LOG_INFO("[ArchiveL] ArchiveManager - SetSavePath [{}]", save_path.string());
  save_path_ = save_path;
  std::shared_lock<std::shared_mutex> lock(mtx_worker);
  for (const auto& worker : archive_workers) {
    worker.second->SetSavePath(save_path);
    std::map<std::string, std::shared_ptr<IOWorker>> io_workers = worker.second->GetIOWorkers();
    for (const auto& io_worker : io_workers) {
      io_worker.second->SetPath(save_path);
    }
  }

  reader_worker_.SetData(save_path_);
}


bool ArchiveManager::PushDataGroup(std::shared_ptr<PnxMediaArchiveDataGroup> data_group) {
  if (destroyed_)
    return false;

  if (data_group == nullptr || data_group->GetNumber() <= 0)
    return false;

  auto first_media_data = data_group->GetFirstMediaData();
  if (nullptr == first_media_data) {
    BLAZE_LOG_ERROR("[ArchiveL] ArchiveManager - Fail : first media data is null");
    return false;
  }

  data_group->channel_uuid = first_media_data->channel_uuid;
  MediaProfile profile = first_media_data->profile;
  SessionID session_id = SessionID(data_group->channel_uuid, data_group->purpose_, (MediaProfile)profile);
  std::string stream_id = GetStreamID(session_id);
  bool add_session = false;
  { 
    //add session if not exist
    std::unique_lock<std::shared_mutex> lock(mtx_worker);
    std::string stream_id = GetStreamID(session_id);
    if (device_session_index.find(stream_id) == device_session_index.end()) {
      add_session = true;
    }
  }
  if (add_session)
    AddSession(data_group->GetArchivingDrive().string(), session_id);

  std::shared_ptr<ArchiveChunkBuffer> media_datas = std::make_shared<ArchiveChunkBuffer>();
  size_t num_datas = data_group->GetNumber();
  for (size_t i = 0; i < num_datas; i ++) {
    std::shared_ptr<PnxMediaData> pnx_media_data = data_group->At(i);
    if (pnx_media_data == nullptr)
      break;

    std::shared_ptr<StreamBuffer> new_stream_buffer = nullptr;
    switch (pnx_media_data->type) {
      case Pnx::Media::MediaType::Video:
        new_stream_buffer = std::make_shared<VideoData>(pnx_media_data.get());
        break;
      case Pnx::Media::MediaType::Audio:
        new_stream_buffer = std::make_shared<AudioData>(pnx_media_data.get());
        break;
      case Pnx::Media::MediaType::MetaData:
        new_stream_buffer = std::make_shared<MetaData>(pnx_media_data.get());
        break;
      default:
        //BLAZE_LOG_INFO("[ArchiveL]Not Supported media data type\n");
        continue;
    }
    media_datas->PushQueue(new_stream_buffer);
  }
  if (media_datas->GetCount() < 0)
    return false;

  std::shared_ptr<ArchiveWorker> workerptr = nullptr;
  {
    std::unique_lock<std::shared_mutex> lock(mtx_worker);
    if (device_session_index.find(stream_id) == device_session_index.end()) {
      return false;
    }
    workerptr = archive_workers[device_session_index[stream_id]];
    if (CheckWorkerDrive(stream_id, data_group->GetArchivingDrive().string(), workerptr))
      workerptr = archive_workers[device_session_index[stream_id]];
  }
  if (workerptr == nullptr)
    return false;

  return workerptr->PushVideoChunkBuffer(session_id, data_group->GetArchivingDrive().string(), media_datas);
}

bool ArchiveManager::CheckWorkerDrive(std::string stream_id, std::string save_driver, std::shared_ptr<ArchiveWorker> workerptr) {
  if (device_session_index[stream_id].compare(save_driver) != 0) {
    std::shared_ptr<IOWorker> io_worker = workerptr->GetIOWorker(stream_id);
    if (io_worker == nullptr)
      return false;
    
    workerptr->RemoveIOWorker(io_worker->GetStreamID());
    archive_workers[save_driver]->PushStream(io_worker->GetStreamID(), io_worker);
    device_session_index[stream_id] = save_driver;
    return true;
  } else
    return false;
}

bool ArchiveManager::SetDataEncryption(SessionID session_id, EncryptionType encrytion_type) {
  std::string stream_id = GetStreamID(session_id);
  std::shared_ptr<ArchiveWorker> workerptr = nullptr;
  {
    std::shared_lock<std::shared_mutex> lock(mtx_worker);
    if (device_session_index.find(stream_id) == device_session_index.end()) {
      return false;
    }
    workerptr = archive_workers[device_session_index[stream_id]];
    if (nullptr != workerptr) {
      workerptr->SetDataEncryption(session_id, encrytion_type);
    } else {
      BLAZE_LOG_ERROR("[ArchiveL] ArchiveManager - Fail : workerptr is null");
    }
  }
  if (workerptr == nullptr)
    return false;

  return true;
}

bool ArchiveManager::Seek(ChannelUUID channel, const PnxMediaTime& time, ArchiveReadType archive_read_type, std::shared_ptr<ReaderObject> read_object) {
  if (read_object == nullptr) {
    BLAZE_LOG_ERROR("[ArchiveL] Seek failed - read_object is null. ch:[{}] pos(ms):[{}] read_type:[{}]", channel, time.ToMilliSeconds(),
                    static_cast<int>(archive_read_type));
    return false;
  }

  bool is_seek = true;
  if (kArchiveReadNext == archive_read_type) {
    bool initial_seek_ok = reader_worker_.Seek(channel, time.ToMilliSeconds(), kArchiveReadPrev, read_object);
    if (!initial_seek_ok) {
      BLAZE_LOG_WARN("[ArchiveL] Seek(kNext) initial seek failed. ch:[{}] pos(ms):[{}] profile:[{}]", channel, time.ToMilliSeconds(),
                     read_object->GetProfile().ToString());
    }

    if (read_object->IsInGOP(time)) {
      read_object->ClearFileList();
      while (true) {
        if (read_object->GetChunkEndTime() < time.ToMilliSeconds()) {
          auto frame_data = reader_worker_.GetNextData(kArchiveTypeFrameVideo, read_object);
          if (frame_data == std::nullopt) {
            BLAZE_LOG_WARN("[ArchiveL] Seek(kNext) failed while scanning GOP. ch:[{}] pos(ms):[{}] profile:[{}]", channel, time.ToMilliSeconds(),
                           read_object->GetProfile().ToString());
            is_seek = false;
            break;
          }
        } else
          break;
      }

      if (is_seek) {
        BLAZE_LOG_DEBUG("[ArchiveL] Seek(kNext) resolved in GOP. ch:[{}] pos(ms):[{}]", channel, time.ToMilliSeconds());
      }
      return is_seek;
    }
  }

  bool seek_result = reader_worker_.Seek(channel, time.ToMilliSeconds(), archive_read_type, read_object);
  if (!seek_result) {
    BLAZE_LOG_WARN("[ArchiveL] Seek failed. ch:[{}] pos(ms):[{}] read_type:[{}] profile:[{}]", channel, time.ToMilliSeconds(),
                   static_cast<int>(archive_read_type), read_object->GetProfile().ToString());
  }
  return seek_result;
}

std::shared_ptr<ArchiveChunkBuffer> ArchiveManager::GetStreamChunk(ArchiveChunkReadType gov_read_type, std::shared_ptr<ReaderObject> read_object) {
  auto chunk_buffer = reader_worker_.GetStreamChunk(gov_read_type, read_object);
  if (chunk_buffer && read_object) {
    BLAZE_LOG_INFO("[ArchiveL] GetStreamChunk ch_uuid: {}, ArchiveChunkReadType: {}, pos(ms): {}, chunk_begin(ms): {}, chunk_end(ms): {}",
                read_object->GetSessionid(), static_cast<int>(gov_read_type), read_object->GetCurrentPositionTime(), chunk_buffer->GetChunkBeginTime(),
                chunk_buffer->GetChunkEndTime());
  }
  return chunk_buffer;
}

std::shared_ptr<ArchiveChunkBuffer> ArchiveManager::GetStreamGop(ArchiveChunkReadType gov_read_type, std::shared_ptr<ReaderObject> read_object) {
  auto chunk_buffer = reader_worker_.GetStreamGop(gov_read_type, read_object);
  if (chunk_buffer && read_object) {
    BLAZE_LOG_INFO("[ArchiveL] GetStreamGop ch_uuid: {}, ArchiveChunkReadType: {}, pos(ms): {}, chunk_begin(ms): {}, chunk_end(ms): {}",
                read_object->GetSessionid(), static_cast<int>(gov_read_type), read_object->GetCurrentPositionTime(), chunk_buffer->GetChunkBeginTime(),
                chunk_buffer->GetChunkEndTime());
  }
  return chunk_buffer;
}

// GetNextData,GetPrevData :  Data is provided according to ArchiveType. If none, it is provided sequentially.
std::shared_ptr<PnxMediaDataGroup> ArchiveManager::GetNextData(std::shared_ptr<ReaderObject> read_object,
                                                                const unsigned char cseq, ArchiveType get_video_type) {
  std::optional<std::vector<std::shared_ptr<StreamBuffer>>> datas = reader_worker_.GetNextData(get_video_type, read_object);
  if (datas == std::nullopt)
    return nullptr;

  //BLAZE_LOG_INFO("[ArchiveL] GetNextData : frame time : {}\n", datas->begin()->get()->timestamp_msec);
  std::shared_ptr<PnxMediaDataGroup> media_datas = std::make_shared<PnxMediaDataGroup>();

  for (auto data : datas.value()) {
    if (auto video = std::dynamic_pointer_cast<VideoData>(data)) {
      auto video_data = std::dynamic_pointer_cast<Pnx::Media::VideoSourceFrame>(video.get()->buffer);
      if (video_data == nullptr)
        continue;

      video_data->codecType = video.get()->archive_header.codecType;
      video_data->frameType = video.get()->archive_header.frameType;
      video_data->resolution.height = video.get()->archive_header.height;
      video_data->resolution.width = video.get()->archive_header.width;
      video_data->pts = video.get()->timestamp_msec;

      auto pnx_data = std::make_shared<PnxMediaData>();
      pnx_data->type = Pnx::Media::MediaType::Video;
      pnx_data->profile = read_object->GetProfile();
      pnx_data->phoenix_play_info.cseq = cseq;
      pnx_data->data = video_data;
      pnx_data->time_info.FromMilliSeconds(video_data->pts);
      media_datas->PushBack(pnx_data);
    } else if (auto audio = std::dynamic_pointer_cast<AudioData>(data)) {
      auto audio_data = std::dynamic_pointer_cast<Pnx::Media::AudioSourceFrame>(audio.get()->buffer);
      if (audio_data == nullptr)
        continue;

      audio_data->codecType = audio.get()->archive_header.codecType;
      audio_data->audioChannels = audio.get()->archive_header.audioChannels;
      audio_data->audioSampleRate = audio.get()->archive_header.audioSampleRate;
      audio_data->audioBitPerSample = audio.get()->archive_header.audioBitPerSample;
      audio_data->audioBitrate = audio.get()->archive_header.audioBitrate;
      audio_data->pts = audio.get()->timestamp_msec;

      auto pnx_data = std::make_shared<PnxMediaData>();
      pnx_data->type = Pnx::Media::MediaType::Audio;
      pnx_data->profile = read_object->GetProfile();
      pnx_data->phoenix_play_info.cseq = cseq;
      pnx_data->data = audio_data;
      pnx_data->time_info.FromMilliSeconds(audio_data->pts);
      media_datas->PushBack(pnx_data);
    } else if (auto meta = std::dynamic_pointer_cast<MetaData>(data)) {
      auto meta_data = std::dynamic_pointer_cast<Pnx::Media::MetaDataSourceFrame>(meta.get()->buffer);
      if (meta_data == nullptr)
        continue;

      meta_data->pts = meta.get()->timestamp_msec;

      auto pnx_data = std::make_shared<PnxMediaData>();
      pnx_data->type = Pnx::Media::MediaType::MetaData;
      pnx_data->profile = read_object->GetProfile();
      pnx_data->phoenix_play_info.cseq = cseq;
      pnx_data->data = meta_data;
      pnx_data->time_info.FromMilliSeconds(meta_data->pts);
      media_datas->PushBack(pnx_data);
    }
  }

  return media_datas;
}

std::shared_ptr<BaseBuffer> ArchiveManager::GetPrevData(std::shared_ptr<ReaderObject> read_object, ArchiveType get_video_type) {
  return reader_worker_.GetPrevData(get_video_type, read_object);
}

void ArchiveManager::InitailData(){
  g_dio_page_size_ = ArchiveUtil::GetPageSize();
}

void ArchiveManager::CallbackWirteStatusAWTAM(SessionID session_id, bool success,
                                              std::shared_ptr<std::vector<FrameWriteIndexData>> list) {
  if (list == nullptr || list->size() <= 0)
    return;

  std::string stream_id = GetStreamID(session_id);

  PnxMediaTime start_time;
  PnxMediaTime end_time;
  start_time.FromMilliSeconds(list->at(0).packet_timestamp_msec);
  end_time.FromMilliSeconds(list->at(list->size() - 1).packet_timestamp_msec);
  BLAZE_LOG_INFO("[ArchiveL] ArchiveManager - Write Result channel[{}], profile[{}], success[{}], begintime[{}], endtime[{}]", session_id.channel_uuid,
              session_id.profile.ToString(),
              success,
              start_time.ToLocalTimeString(), end_time.ToLocalTimeString());

  std::shared_lock<std::shared_mutex> lock(callback_mtx); //shared lock
  if (stream_id.empty())
    return;

  auto it = callback_writeresults_.find(stream_id);
  if (it != callback_writeresults_.end())
    it->second(session_id, success, list);
}

void ArchiveManager::CallbackWriteResult(
    SessionID session_id, const std::function<void(SessionID session_id, bool is_success, std::shared_ptr<std::vector<FrameWriteIndexData>> infos)>& cb_func) {

  std::string stream_id = GetStreamID(session_id);
  std::lock_guard<std::shared_mutex> lock(callback_mtx); //exclusive  lock
  callback_writeresults_.insert_or_assign(stream_id, cb_func);
}

void ArchiveManager::DeleteCallbackFunction(SessionID session_id) {
  std::string stream_id = GetStreamID(session_id);

  std::lock_guard<std::shared_mutex> lock(callback_mtx);  //exclusive  lock
  auto it = callback_writeresults_.find(stream_id);
  if (it != callback_writeresults_.end())
    callback_writeresults_.erase(stream_id);
}

std::map<std::string, std::tuple<bool, int>> ArchiveManager::GetSaveStatus() {
  std::map<std::string, std::tuple<bool, int>> datas;
  std::shared_lock<std::shared_mutex> lock(mtx_worker);
  for (const auto& worker : archive_workers) {
    std::map<std::string, std::shared_ptr<IOWorker>> io_workers = worker.second->GetIOWorkers();
    for (const auto& io_worker : io_workers) {
      if (io_worker.second->GetSessionID().purpose == PnxMediaArchiveInfo::EPurpose::kBackup)
        continue;

      auto save_status = io_worker.second->GetSaveStatus();

      if (std::get<0>(datas[io_worker.second->GetChannelUUID()]) == false)
        std::get<0>(datas[io_worker.second->GetChannelUUID()]) = std::get<0>(save_status);

      std::get<1>(datas[io_worker.second->GetChannelUUID()]) += std::get<1>(save_status);

    }
  }

  return datas;
}

std::shared_ptr<ArchiveChunkBuffer> ArchiveManager::GetMemoryFrames(std::string session_id, MediaProfile profile, int64_t timestamp) {
  std::string stream_id = GetStreamID(session_id);
  std::shared_ptr<ArchiveWorker> workerptr = nullptr;
  {
    std::shared_lock<std::shared_mutex> lock(mtx_worker);
    if (device_session_index.find(stream_id) == device_session_index.end()) {
      return nullptr;
    }
    workerptr = archive_workers[device_session_index[stream_id]];
  }
  if (workerptr == nullptr)
    return nullptr;

  std::shared_ptr<IOWorker> io_worker = workerptr->GetIOWorker(stream_id);
  if (io_worker == nullptr)
    return nullptr;

  return io_worker->GetMemoryFrames(timestamp);
}

int ArchiveManager::GetOpenFileCount() {
  int total_open_files = 0;
  std::shared_lock<std::shared_mutex> lock(mtx_worker);
  
  for (const auto& worker : archive_workers) {
    worker.second->WakeWorker();
    std::map<std::string, std::shared_ptr<IOWorker>> io_workers = worker.second->GetIOWorkers();
    for (const auto& io_worker : io_workers) {
      if (io_worker.second->IsFileOpen()) {
        total_open_files++;
      }
    }
  }
  
  return total_open_files;
}

std::map<MediaProfile, std::vector<std::shared_ptr<ArchiveTime>>> ArchiveManager::GetRecordTime(const std::string session_id, const PnxMediaTime& begin_time,
                                                                                 const PnxMediaTime& end_time) {

  PnxMediaTime file_start_time = begin_time - PnxMediaTime(60 * 60);  // 60 minutes before
  std::map<MediaProfile, std::vector<SearchFileEntry>> files = reader_worker_.GetSearchFile(session_id, file_start_time, end_time);
  std::map<MediaProfile, std::vector<std::shared_ptr<ArchiveTime>>> archiving_times;
  for (auto& [profile, entries] : files) {
    std::vector<std::shared_ptr<ArchiveTime>> archiving_time = reader_worker_.GetTimeInfo(entries, begin_time, end_time, profile);
    archiving_times.emplace(profile, archiving_time);
  }
  return archiving_times;
}

uint64_t ArchiveManager::GetArchiveSize(const std::string& channel_uuid, MediaProfile profile, const PnxMediaTime& begin_time, const PnxMediaTime& end_time,
                                        uint64_t& recording_msec) {
  recording_msec = 0;

  // Start one hour folder before the one holding begin_time: a chunk crossing the hour stays in the previous folder.
  // The start must be on the hour: GetSearchFile steps one hour at a time from it and would otherwise skip the folder of end_time.
  const uint64_t hour_msec = 60 * 60 * 1000;
  uint64_t begin_hour_msec = begin_time.ToMilliSeconds() - begin_time.ToMilliSeconds() % hour_msec;
  PnxMediaTime file_start_time;
  file_start_time.FromMilliSeconds(begin_hour_msec >= hour_msec ? begin_hour_msec - hour_msec : 0);

  std::map<MediaProfile, std::vector<SearchFileEntry>> files = reader_worker_.GetSearchFile(channel_uuid, file_start_time, end_time);
  auto it = files.find(profile);
  if (files.end() == it)
    return 0;

  return reader_worker_.GetArchiveSize(it->second, begin_time, end_time, profile, recording_msec);
}

std::optional<unsigned long long> ArchiveManager::GetCurrentFileStartTime(std::string channel_uuid, MediaProfile profile_type) {
  std::optional<unsigned long long> recording_time = std::nullopt; 
  SessionID session_id = SessionID(channel_uuid, PnxMediaArchiveInfo::EPurpose::kRecord, profile_type);
  std::string stream_id = GetStreamID(session_id);

  std::shared_ptr<ArchiveWorker> workerptr = nullptr;
  {
    std::shared_lock<std::shared_mutex> lock(mtx_worker);
    if (device_session_index.find(stream_id) == device_session_index.end()) {
      return std::nullopt;
    }
    workerptr = archive_workers[device_session_index[stream_id]];
    if (workerptr == nullptr)
      return std::nullopt;

    recording_time = workerptr->GetCurrentFileStartTime(channel_uuid, profile_type);
  }
  return recording_time;
}