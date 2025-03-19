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
 * @file    archive_worker.cc
 * @brief
 * @author    cd.kim@hanwha.com
 *
 * @date    2024.06.19
 * @version    1.0
 */

#include "archive_worker.h"

ArchiveWorker::ArchiveWorker(std::string driver) : driver_(driver) {
  //reader_worker_.SetData(driver, drive_mtx_);
  RunWorkThread();
}

ArchiveWorker::~ArchiveWorker() {
  std::cout << "ArchiveWorker - delete worker begin : " << driver_ << std::endl;
  run_thread_ = false;
  condition_release_ = true;
  cv_.notify_one();
  if (work_thread_) {
    work_thread_->join();
    delete work_thread_;
    work_thread_ = nullptr;
  }
  std::cout << "ArchiveWorker - delete worker begin : " << driver_ << std::endl;
}

std::shared_ptr<IOWorker> ArchiveWorker::GetIOWorker(std::string stream_id) {
  std::shared_lock<std::shared_mutex> lock(mtx_ioworkers_);
  std::shared_ptr<IOWorker> io_worker = nullptr; 
  if (io_workers_.find(stream_id) == io_workers_.end()) {
    return nullptr;
  }

  return io_workers_[stream_id];
}

bool ArchiveWorker::RemoveIOWorker(std::string stream_id) {
  std::unique_lock<std::shared_mutex> lock(mtx_ioworkers_);
  if (io_workers_.find(stream_id) == io_workers_.end()) {
    return false;
  }
  SPDLOG_INFO("RemoveIOWorker[{}]", stream_id.c_str());
  io_workers_.erase(stream_id);
  return true;
}

void ArchiveWorker::PushStream(SessionID session_id, std::shared_ptr<IOWorker> io_worker) {
  std::shared_lock<std::shared_mutex> lock(mtx_ioworkers_);
  std::cout << "PushStream :" << session_id << std::endl;
  io_workers_[session_id] = io_worker;
}

bool ArchiveWorker::AddStream(SessionID session_id, MediaProfile profile) {
  std::string stream_id = GetStreamID(session_id, profile);
  std::shared_ptr<IOWorker> ioworkerptr = nullptr;
  {
    std::unique_lock<std::shared_mutex> lock(mtx_ioworkers_);
    if (io_workers_.find(stream_id) != io_workers_.end()) {
      return false;
    }

    std::cout << "AddStream :" << stream_id << std::endl;

    std::shared_ptr<IOWorker> io_w = std::make_shared<IOWorker>(session_id, profile);
    io_w->SetPath(save_path_);
    io_w->SetStreamID(stream_id);
    io_w->callback_write_status_iwtaw_ =
        std::bind(&ArchiveWorker::CallbackWirteStatusIWTAW, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4);
    io_workers_.emplace(stream_id, io_w);
  }

  return true;
}

bool ArchiveWorker::DeleteStream(SessionID session_id, MediaProfile profile) {
  std::string stream_id = GetStreamID(session_id, profile);
  { 
    std::unique_lock<std::shared_mutex> lock(mtx_io_delete_);
    std::unique_lock<std::shared_mutex> lock2(mtx_ioworkers_);
    auto it = io_workers_.find(stream_id);
    if (it != io_workers_.end()) {
      SPDLOG_INFO("DeleteStream stream_id[{}], profile[{}]", stream_id.c_str(), profile.ToString());
      io_workers_.erase(it);
    } else {
      SPDLOG_ERROR("(DeleteStream) couldn't find  stream_id[{}], profile[{}]", stream_id.c_str(), profile.ToString());
      return false;
    }
  }
  return true;
}

bool ArchiveWorker::Flush(SessionID session_id, MediaProfile profile) {
  std::string stream_id = GetStreamID(session_id, profile);
  {
    std::shared_lock<std::shared_mutex> lock(mtx_ioworkers_);
    auto it = io_workers_.find(stream_id);
    if (it != io_workers_.end()) {
      SPDLOG_INFO("Flush stream_id[{}], profile[{}]", stream_id.c_str(), profile.ToString());
      it->second->Flush();
    } else {
      SPDLOG_ERROR("(Flush) couldn't find  stream_id[{}], profile[{}]", stream_id.c_str(), profile.ToString());
      return false;
    }
  }
  return true;
}

bool ArchiveWorker::SetDataEncryption(SessionID session_id, MediaProfile profile, EncryptionType encrytion_type) {
  bool ret = false;
  std::string stream_id = GetStreamID(session_id, profile);
  {
    std::shared_lock<std::shared_mutex> lock(mtx_ioworkers_);
    if (io_workers_.find(stream_id) == io_workers_.end()) {
      return false;
    }

    io_workers_[stream_id]->SetDataEncryption(encrytion_type);
  }

  return ret;
}

void ArchiveWorker::RunWorkThread() {
  run_thread_ = true;
  work_thread_ = new std::thread([this]() {
    while (run_thread_) {
      bool worked = false;
      std::vector<std::shared_ptr<IOWorker>> io_workers_v;
      mtx_io_delete_.lock_shared();
      mtx_ioworkers_.lock_shared();

      //std::cout << "std::transform" << std::endl;
      std::transform(io_workers_.begin(), io_workers_.end(), std::back_inserter(io_workers_v), [](const auto& pair) { return pair.second; });
      mtx_ioworkers_.unlock_shared();

      time_t current_time = time(nullptr);
      unsigned int timestamp = static_cast<unsigned int>(current_time);
      for (auto io_worker : io_workers_v) {
        worked = io_worker->ProcessBuffer(timestamp);
      }
      mtx_io_delete_.unlock_shared();

      // If there's nothing to push, wait
      if (!worked) {
        std::unique_lock<std::mutex> lk(cv_mtx_);
        cv_.wait(lk, [&] { return condition_release_; });
        condition_release_ = false;
      }
    }
    return 0;
  });
}

bool ArchiveWorker::PushVideoChunkBuffer(SessionID session_id, MediaProfile profile, std::string save_driver, std::shared_ptr<ArchiveChunkBuffer> media_datas) {
  bool ret = false;
  std::string stream_id = GetStreamID(session_id, profile);
  {
    std::shared_lock<std::shared_mutex> lock(mtx_ioworkers_);
    if (io_workers_.find(stream_id) == io_workers_.end()) {
      return false;
    }

    ret = io_workers_[stream_id]->PushVideoChunkBuffer(save_driver, media_datas);
  }
  condition_release_ = true;
  cv_.notify_one();

  return ret;
}

std::optional<std::pair<ArchiveIndexHeader, ArchiveObject>> ArchiveWorker::GetFrame(SessionID session_id, int time_stamp,
                                                                                    std::shared_ptr<ReaderObject> read_object) {
  std::filesystem::path path = driver_/save_path_;
  std::optional<std::pair<ArchiveIndexHeader, ArchiveObject>> media_object = std::nullopt;
  if (!media_object.has_value())
    return std::nullopt;

  return media_object;
}

std::optional<std::pair<ArchiveIndexHeader, ArchiveObject>> ArchiveWorker::GetFrame(SessionID session_id, int time_stamp,
                                                                                    std::string frame_full_path,
                                                                                    std::shared_ptr<ReaderObject> read_object) {
  std::filesystem::path path = driver_ / save_path_;
  std::optional<std::pair<ArchiveIndexHeader, ArchiveObject>> media_object = std::nullopt;
  if (!media_object.has_value())
    return std::nullopt;

  return media_object;
}

std::optional<std::pair<ArchiveIndexHeader, ArchiveObject>> ArchiveWorker::GetPrevFrame(SessionID session_id, std::shared_ptr<ReaderObject> read_object) {
  std::optional<std::pair<ArchiveIndexHeader, ArchiveObject>> media_object = std::nullopt;
  if (!media_object.has_value())
    return std::nullopt;

  return media_object;
}

std::optional<std::pair<ArchiveIndexHeader, ArchiveObject>> ArchiveWorker::GetNextFrame(SessionID session_id, std::shared_ptr<ReaderObject> read_object) {
  std::optional<std::pair<ArchiveIndexHeader, ArchiveObject>> media_object = std::nullopt;
  if (!media_object.has_value())
    return std::nullopt;

  return media_object;
}

void ArchiveWorker::CallbackWirteStatusIWTAW(SessionID session_id, MediaProfile profile, bool success,
                                             std::shared_ptr<std::vector<FrameWriteIndexData>> infos) {
  CallbackWirteStatusAWTAM(session_id, profile, success, infos);
}
