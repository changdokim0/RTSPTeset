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

ArchiveManager* ArchiveManager::archive_manager_ = nullptr;
std::mutex ArchiveManager::mtx;
ArchiveManager::ArchiveManager() {
  InitailData();
}


bool ArchiveManager::AddDevice(std::string device_driver) {
  SPDLOG_INFO("[ArchiveL] ArchiveManager - AddDevice  [{}]", device_driver.c_str());
  std::unique_lock<std::shared_mutex> lock(mtx_worker);
  if (archive_workers.find(device_driver) != archive_workers.end()) {
    SPDLOG_ERROR("[ArchiveL] ArchiveManager - Fail : already disk  [{}]", device_driver.c_str());
    return false;
  }

  std::shared_ptr<ArchiveWorker> aw = std::make_shared<ArchiveWorker>(device_driver);
  aw->SetSavePath(save_path_);
  reader_worker_.AddDrive(device_driver);
  aw->callback_write_status_awtam_ =
      std::bind(&ArchiveManager::CallbackWirteStatusAWTAM, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4);
  archive_workers.emplace(device_driver, aw);
  return true;
}

bool ArchiveManager::AddSession(std::string device_driver, SessionID session_id, MediaProfile profile) {
  SPDLOG_INFO("[ArchiveL] ArchiveManager - AddSession  [{}][{}][{}]", device_driver.c_str(), session_id.c_str(), profile.ToString().c_str());
  std::unique_lock<std::shared_mutex> lock(mtx_worker);
  if (archive_workers.find(device_driver) == archive_workers.end()) {
    SPDLOG_ERROR("[ArchiveL] ArchiveManager - Fail : already session  [{}][{}][{}]", device_driver.c_str(), session_id.c_str(), profile.ToString().c_str());
    return false;
  }

  std::string stream_id = GetStreamID(session_id, profile);
  archive_workers[device_driver]->AddStream(session_id, profile);
  device_session_index.emplace(stream_id, device_driver);

  return true;
}

bool ArchiveManager::DeleteDevice(std::string device_driver) {
  SPDLOG_INFO("[ArchiveL] ArchiveManager - DeleteDevice  [{}]", device_driver.c_str());
  std::unique_lock<std::shared_mutex> lock(mtx_worker);
  archive_workers.erase(device_driver);
  return true;
}

bool ArchiveManager::DeleteStream(SessionID session_id, MediaProfile profile) {
  SPDLOG_INFO("[ArchiveL] ArchiveManager - DeleteStream  [{}][{}]", session_id.c_str(), profile.ToString().c_str());
  std::string stream_id = GetStreamID(session_id, profile);
  std::shared_ptr<ArchiveWorker> workerptr = nullptr;
  {
    std::shared_lock<std::shared_mutex> lock(mtx_worker);
    if (device_session_index.find(stream_id) == device_session_index.end()) {
      return false;
    }
    workerptr = archive_workers[device_session_index[stream_id]];
    device_session_index.erase(stream_id);
  }
  if (workerptr == nullptr)
    return false;

  return workerptr->DeleteStream(session_id, profile);
}

bool ArchiveManager::Flush(SessionID session_id, MediaProfile profile) {
  std::string stream_id = GetStreamID(session_id, profile);
  SPDLOG_INFO("[ArchiveL] Flush : session_id [{}], profile : {}\n", session_id, profile.ToString());
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

  return workerptr->Flush(session_id, profile);
}

void ArchiveManager::SetSavePath(std::filesystem::path save_path) {
  save_path_ = save_path;
  reader_worker_.SetData(save_path_);
}


bool ArchiveManager::PushDataGroup(std::shared_ptr<PnxMediaArchiveDataGroup> data_group) {
  if (data_group == nullptr || data_group->GetNumber() <= 0)
    return false;

  // TODO cd.kim temp code begin
  auto first_media_data = data_group->GetFirstMediaData();
  if (nullptr == first_media_data) {
    SPDLOG_ERROR("[ArchiveL][{0}] ArchiveManager - Fail : first media data is null", __func__);
    return false;
  }
  data_group->channel_uuid = first_media_data->channel_uuid;
  MediaProfile profile = first_media_data->profile;
  // TODO cd.kim temp code end

  std::string session_id = GetStreamID(data_group->channel_uuid, profile);
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
        //SPDLOG_INFO("[ArchiveL]Not Supported media data type\n");
        continue;
    }
    media_datas->PushQueue(new_stream_buffer);
  }

  if (media_datas->GetCount() < 0)
    return false;

  std::shared_ptr<ArchiveWorker> workerptr = nullptr;
  {
    std::shared_lock<std::shared_mutex> lock(mtx_worker);
    if (device_session_index.find(session_id) == device_session_index.end()) {
      return false;
    }
    workerptr = archive_workers[device_session_index[session_id]];
    if (CheckWorkerDrive(session_id, data_group->GetArchivingDrive().string(), workerptr))
      workerptr = archive_workers[device_session_index[session_id]];
  }
  if (workerptr == nullptr)
    return false;

  return workerptr->PushVideoChunkBuffer(data_group->channel_uuid, profile, data_group->GetArchivingDrive().string(), media_datas);
}

bool ArchiveManager::CheckWorkerDrive(std::string session_id, std::string save_driver, std::shared_ptr<ArchiveWorker> workerptr) {
  if (device_session_index[session_id].compare(save_driver) != 0) {
    std::shared_ptr<IOWorker> io_worker = workerptr->GetIOWorker(session_id);
    if (io_worker == nullptr)
      return false;
    
    workerptr->RemoveIOWorker(io_worker->GetStreamID());
    archive_workers[save_driver]->PushStream(io_worker->GetStreamID(), io_worker);
    device_session_index[session_id] = save_driver;
    return true;
  } else
    return false;
}

bool ArchiveManager::SetDataEncryption(SessionID session_id, MediaProfile profile, EncryptionType encrytion_type) {
  std::string stream_id = GetStreamID(session_id, profile);
  std::shared_ptr<ArchiveWorker> workerptr = nullptr;
  {
    std::shared_lock<std::shared_mutex> lock(mtx_worker);
    if (device_session_index.find(stream_id) == device_session_index.end()) {
      return false;
    }
    workerptr = archive_workers[device_session_index[stream_id]];
    if (nullptr != workerptr) {
      workerptr->SetDataEncryption(session_id, profile, encrytion_type);
    } else {
      SPDLOG_ERROR("[ArchiveL][{0}] ArchiveManager - Fail : workerptr is null", __func__);
    }
  }
  if (workerptr == nullptr)
    return false;

  return true;
}

bool ArchiveManager::Seek(ChannelUUID channel, const PnxMediaTime& time, ArchiveReadType archive_read_type, std::shared_ptr<ReaderObject> read_object) {
  if (read_object == nullptr)
    return false;
  if (kArchiveReadNext == archive_read_type) {
    reader_worker_.Seek(channel, time.ToMilliSeconds(), kArchiveReadPrev, read_object);
    if (read_object->IsInGOP(time)) {
      read_object->ClearFileList();
      return true;
    }
  }

  SPDLOG_INFO("[ArchiveL] Seek : channel : {}, time : {}, archive_read_type : {}\n", channel, time.ToMilliSeconds(), archive_read_type);
  return reader_worker_.Seek(channel, time.ToMilliSeconds(), archive_read_type, read_object);
}

std::shared_ptr<ArchiveChunkBuffer> ArchiveManager::GetStreamChunk(ArchiveChunkReadType gov_read_type,
                                                            std::shared_ptr<ReaderObject> read_object) {
  return reader_worker_.GetStreamChunk(gov_read_type, read_object);
}

std::shared_ptr<ArchiveChunkBuffer> ArchiveManager::GetStreamGop(ArchiveChunkReadType gov_read_type, std::shared_ptr<ReaderObject> read_object) {
  return reader_worker_.GetStreamGop(gov_read_type, read_object);
}

// GetNextData,GetPrevData :  Data is provided according to ArchiveType. If none, it is provided sequentially.
std::shared_ptr<PnxMediaDataGroup> ArchiveManager::GetNextData(std::shared_ptr<ReaderObject> read_object,
                                                                const unsigned char cseq, ArchiveType get_video_type) {
  std::optional<std::vector<std::shared_ptr<StreamBuffer>>> datas = reader_worker_.GetNextData(get_video_type, read_object);
  if (datas == std::nullopt)
    return nullptr;

  //SPDLOG_INFO("[ArchiveL] GetNextData : frame time : {}\n", datas->begin()->get()->timestamp_msec);
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

void ArchiveManager::CallbackWirteStatusAWTAM(SessionID session_id, MediaProfile profile, bool success,
                                              std::shared_ptr<std::vector<FrameWriteIndexData>> list) {
  if (list == nullptr || list->size() <= 0)
    return;

  std::string stream_id = GetStreamID(session_id, profile);

  PnxMediaTime start_time;
  PnxMediaTime end_time;
  start_time.FromMilliSeconds(list->at(0).packet_timestamp_msec);
  end_time.FromMilliSeconds(list->at(list->size() - 1).packet_timestamp_msec);
  SPDLOG_INFO("ArchiveManager - Write Result channel[{}], profile[{}], success[{}], begintime[{}], endtime[{}]", session_id.c_str(), profile.ToString(), success, start_time.ToLocalTimeString(), end_time.ToLocalTimeString());

  std::shared_lock<std::shared_mutex> lock(callback_mtx); //shared lock
  if (stream_id.empty())
    return;

  auto it = callback_writeresults_.find(stream_id);
  if (it != callback_writeresults_.end())
    it->second(session_id, profile, success, list);
}

void ArchiveManager::CallbackWriteResult(SessionID session_id, MediaProfile profile, 
  const std::function<void(ChannelUUID channel, MediaProfile profile, bool is_success, std::shared_ptr<std::vector<FrameWriteIndexData>> infos)>& cb_func) {

  std::string stream_id = GetStreamID(session_id, profile);

  std::lock_guard<std::shared_mutex> lock(callback_mtx); //exclusive  lock

  callback_writeresults_.emplace(stream_id, cb_func);
}

void ArchiveManager::DeleteCallbackFunction(SessionID session_id, MediaProfile profile) {
  std::string stream_id = GetStreamID(session_id, profile);

  std::lock_guard<std::shared_mutex> lock(callback_mtx);  //exclusive  lock
  auto it = callback_writeresults_.find(stream_id);
  if (it != callback_writeresults_.end())
    callback_writeresults_.erase(stream_id);
}

std::map<std::string, std::tuple<bool, int>> ArchiveManager::GetSaveStatus() {
  std::map<std::string, std::tuple<bool, int>> datas;

  for (const auto& worker : archive_workers) {
    std::map<std::string, std::shared_ptr<IOWorker>> io_workers = worker.second->GetIOWorkers();
    for (const auto& io_worker : io_workers) {
      auto save_status = io_worker.second->GetSaveStatus();

      if (std::get<0>(datas[io_worker.second->GetSessionID()]) == false)
        std::get<0>(datas[io_worker.second->GetSessionID()]) = std::get<0>(save_status);

      std::get<1>(datas[io_worker.second->GetSessionID()]) += std::get<1>(save_status);

    }
  }

  return datas;
}

std::shared_ptr<ArchiveChunkBuffer> ArchiveManager::GetMemoryFrames(std::string session_id, MediaProfile profile, int64_t timestamp) {
  std::string stream_id = GetStreamID(session_id, profile);
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
