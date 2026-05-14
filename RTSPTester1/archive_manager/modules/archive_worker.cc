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
#include "blaze_logger.h"

ArchiveWorker::ArchiveWorker(std::string driver) : driver_(driver) {
  //reader_worker_.SetData(driver, drive_mtx_);
  BLAZE_LOG_INFO("[ArchiveL] ArchiveWorker - create worker : {}", driver_.string());
  RunWorkThread();
}

ArchiveWorker::~ArchiveWorker() {
 try {
  BLAZE_LOG_INFO("[ArchiveL] ArchiveWorker - delete worker begin : {}", driver_.string());
   run_thread_ = false;
   condition_release_ = true;
   cv_.notify_all();

   std::unique_lock<std::mutex> lock(mtx_threads_);
   for (auto thread : work_threads_) {
     if (thread) {
       thread->join();
       delete thread;
     }
   }
   work_threads_.clear();
   BLAZE_LOG_INFO("[ArchiveL] ArchiveWorker - delete worker end : {}", driver_.string());
 } catch (const std::ios_base::failure& e) {
   BLAZE_LOG_ERROR("[ArchiveL] std::ios_base::failure Exception caught in ArchiveWorker destructor: {}", e.what());
 } catch (const std::system_error& e) {
   BLAZE_LOG_ERROR("[ArchiveL] system_error Exception caught in ArchiveWorker destructor: {}", e.what());
 } catch (const std::exception& e) {
   BLAZE_LOG_ERROR("[ArchiveL] Exception caught in ArchiveWorker destructor: {}", e.what());
 } catch (...) {
   BLAZE_LOG_ERROR("[ArchiveL] Unknown exception caught in ArchiveWorker destructor.");
 }
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
  BLAZE_LOG_INFO("[ArchiveL] RemoveIOWorker[{}]", stream_id.c_str());
  io_workers_.erase(stream_id);
  return true;
}

void ArchiveWorker::PushStream(std::string stream_id, std::shared_ptr<IOWorker> io_worker) {
  std::unique_lock<std::shared_mutex> lock(mtx_ioworkers_);
  BLAZE_LOG_INFO("[ArchiveL] PushStream : {}", stream_id);
  io_workers_[stream_id] = io_worker;
}

bool ArchiveWorker::AddStream(SessionID session_id) {
  std::string stream_id = GetStreamID(session_id);
  std::shared_ptr<IOWorker> ioworkerptr = nullptr;
  {
    std::unique_lock<std::shared_mutex> lock(mtx_ioworkers_);
    if (io_workers_.find(stream_id) != io_workers_.end()) {
      return false;
    }

    BLAZE_LOG_INFO("[ArchiveL] AddStream : {}", stream_id);

    std::shared_ptr<IOWorker> io_w = std::make_shared<IOWorker>(session_id);
    io_w->SetPath(save_path_);
    io_w->SetStreamID(stream_id);
    io_w->callback_write_status_iwtaw_ =
        std::bind(&ArchiveWorker::CallbackWirteStatusIWTAW, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);
    io_workers_.emplace(stream_id, io_w);
  }

  return true;
}

bool ArchiveWorker::DeleteStream(SessionID session_id) {
  std::string stream_id = GetStreamID(session_id);
  { 
    // First, acquire both locks to ensure worker threads don't copy the shared_ptr
    std::unique_lock<std::shared_mutex> lock_delete(mtx_io_delete_);
    std::unique_lock<std::shared_mutex> lock_workers(mtx_ioworkers_);
    
    auto it = io_workers_.find(stream_id);
    if (it != io_workers_.end()) {
      BLAZE_LOG_INFO("[ArchiveL] DeleteStream stream_id[{}], profile[{}], use_count[{}]", 
                     stream_id.c_str(), session_id.profile.ToString(), it->second.use_count());
      io_workers_.erase(it);
    } else {
      BLAZE_LOG_ERROR("[ArchiveL] (DeleteStream) couldn't find  stream_id[{}], profile[{}]", 
                      stream_id.c_str(), session_id.profile.ToString());
      return false;
    }
  }
  
  // Notify worker threads after releasing locks
  condition_release_ = true;
  cv_.notify_all();
  
  return true;
}

bool ArchiveWorker::Flush(SessionID session_id, FlushType flush_type) {
  std::string stream_id = GetStreamID(session_id);
  {
    std::shared_lock<std::shared_mutex> lock(mtx_ioworkers_);
    auto it = io_workers_.find(stream_id);
    if (it != io_workers_.end()) {
      BLAZE_LOG_INFO("[ArchiveL] Flush stream_id[{}], profile[{}]", stream_id.c_str(), session_id.profile.ToString());
      it->second->Flush(flush_type);
    } else {
      BLAZE_LOG_ERROR("[ArchiveL] (Flush) couldn't find  stream_id[{}], profile[{}]", stream_id.c_str(), session_id.profile.ToString());
      return false;
    }
  }
  condition_release_ = true;
  cv_.notify_all();
  return true;
}

bool ArchiveWorker::SetDataEncryption(SessionID session_id,  EncryptionType encrytion_type) {
  bool ret = false;
  std::string stream_id = GetStreamID(session_id);
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

  auto worker_func = std::make_shared<std::function<int(int)>>();
  *worker_func = [this, worker_func](int thread_index) {

    auto max_thread_count = IOWORKER_THREAD_MAX_COUNT;
    if (std::filesystem::exists("/tmp/ProductModelInformation.json")) { //appliance model
      max_thread_count = IOWORKER_THREAD_MAX_COUNT_APPLIANCE;
    }

    BLAZE_LOG_INFO("[ArchiveL] Worker thread {}, max_thread_count {}, started.", thread_index, max_thread_count);  

    while (run_thread_) {
      bool worked = false;
      std::vector<std::shared_ptr<IOWorker>> io_workers_v;
      mtx_io_delete_.lock_shared();
      mtx_ioworkers_.lock_shared();

      int total_threads = 0;
      {
        std::unique_lock<std::mutex> lock(mtx_threads_);
        total_threads = work_threads_.size();
      }

      try {
        int idx = 0;
        for (const auto& pair : io_workers_) {
          if (pair.second != nullptr && (idx % total_threads) == thread_index) {
            io_workers_v.push_back(pair.second);
          }
          idx++;
        }
      } catch (const std::exception& e) {
        BLAZE_LOG_ERROR("[ArchiveL] Exception occurred while iterating io_workers_: {}", e.what());
      } catch (...) {
        BLAZE_LOG_ERROR("[ArchiveL] Unknown exception occurred while iterating io_workers_");
      }
      mtx_ioworkers_.unlock_shared();

      time_t current_time = time(nullptr);
      unsigned int timestamp = static_cast<unsigned int>(current_time % UINT_MAX);
      for (auto io_worker : io_workers_v) {
        worked = io_worker->ProcessBuffer(timestamp);
      }
      mtx_io_delete_.unlock_shared();

      {
        std::unique_lock<std::mutex> lock(mtx_threads_);
        size_t current_threads = work_threads_.size();
        if (thread_index >= current_threads) {
          return 0;
        }
      }

      if (!worked) {
        std::unique_lock<std::mutex> lk(cv_mtx_);
        cv_.wait(lk, [&] { return condition_release_; });
        condition_release_ = false;
      }

      if (thread_index == 0) {  // only manager thread
        mtx_ioworkers_.lock_shared();
        size_t worker_count = io_workers_.size();
        mtx_ioworkers_.unlock_shared();

        size_t required_threads = (worker_count / max_thread_count) + 1;

        std::unique_lock<std::mutex> lock(mtx_threads_);
        size_t current_threads = work_threads_.size();

        if (required_threads > current_threads) {
          for (size_t i = current_threads; i < required_threads; i++) {
            std::thread* new_thread = new std::thread(*worker_func, i);
            work_threads_.push_back(new_thread);
            BLAZE_LOG_INFO("[ArchiveL] Started thread {} (total workers: {}, total threads: {})", i, worker_count, work_threads_.size());
          }
        }

        if (required_threads < current_threads && required_threads >= 1 && current_threads > 1) {
          std::thread* thread_to_remove = work_threads_.back();
          work_threads_.pop_back();
          size_t reduced_threads = work_threads_.size();
          lock.unlock();
          cv_.notify_all();
          thread_to_remove->join();
          delete thread_to_remove;
          BLAZE_LOG_INFO("[ArchiveL] Stopped thread {} (total workers: {}, total threads: {})", current_threads, worker_count, reduced_threads);
        }
      }
    }
    return 0;
  };

  std::unique_lock<std::mutex> lock(mtx_threads_);
  std::thread* first_thread = new std::thread(*worker_func, 0);
  work_threads_.push_back(first_thread);
}

bool ArchiveWorker::PushVideoChunkBuffer(SessionID session_id, std::string save_driver,
                                         std::shared_ptr<ArchiveChunkBuffer> media_datas) {
  bool ret = false;
  std::string stream_id = GetStreamID(session_id);
  {
    std::shared_lock<std::shared_mutex> lock(mtx_ioworkers_);
    if (io_workers_.find(stream_id) == io_workers_.end()) {
      return false;
    }

    ret = io_workers_[stream_id]->PushVideoChunkBuffer(save_driver, media_datas);
  }
  condition_release_ = true;
  cv_.notify_all();

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

void ArchiveWorker::CallbackWirteStatusIWTAW(SessionID session_id, bool success,
                                             std::shared_ptr<std::vector<FrameWriteIndexData>> infos) {
  CallbackWirteStatusAWTAM(session_id, success, infos);
}
