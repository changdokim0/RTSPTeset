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
 * @file    io_worker.cc
 * @brief
 * @author    cd.kim@hanwha.com
 *
 * @date    2024.06.19
 * @version    1.0
 */

#include "io_worker.h"
#include "blaze_logger.h"
#include <chrono>
#include <cmath>
#include <algorithm>

static bool debug_frame_drop_ = false;

IOWorker::IOWorker(SessionID session_id) : session_id_(session_id){
  profile_ = session_id_.profile;
  channel_uuid_ = session_id_.channel_uuid;
  std::string profileaa = session_id_.profile.ToString();
 
  if (std::filesystem::exists("/tmp/ProductModelInformation.json")) {  //appliance model
    if (session_id_.profile == MediaProfile::kSecondary) {
      dio_buffer_nomal_size = g_dio_page_size_ * 128;    // 512 KB
      dio_buffer_busy_size = dio_buffer_nomal_size * 2;  // 1MB 
    } else {
      dio_buffer_nomal_size = g_dio_page_size_ * 256 * 2; // 2MB
      dio_buffer_busy_size = dio_buffer_nomal_size * 2;   // 4MB
    }

    //// TODO : GA v1.3 검증 이후 삭제  
    if (std::filesystem::exists("/root/debug_frame_drop")) {
      debug_frame_drop_ = true;
    }   
    
  } else {  // Blaze model
    if (session_id_.profile == MediaProfile::kSecondary) {
      dio_buffer_nomal_size = g_dio_page_size_ * 128;
      dio_buffer_busy_size = dio_buffer_nomal_size * 5;
    } else {
      dio_buffer_nomal_size = g_dio_page_size_ * 256;
      dio_buffer_busy_size = dio_buffer_nomal_size * 5;
    }
  }

  write_file_data_handle_ = std::make_shared<DataFileHandle>(dio_buffer_nomal_size, dio_buffer_busy_size);
  BLAZE_LOG_INFO("[ArchiveL] ioworker - create memory buffer   [{}]", dio_buffer_busy_size);
  BLAZE_LOG_INFO("[ArchiveL] ioworker - create stream  [{}]", channel_uuid_);
}

IOWorker::~IOWorker() {
  try {
    BLAZE_LOG_INFO("[ArchiveL] ioworker - delete stream  [{}]", stream_id_.c_str());

    CheckMemoryBufferSize(0);
    time_t current_time = time(nullptr);
    unsigned int timestamp = static_cast<unsigned int>(current_time % UINT_MAX);
    ProcessBuffer(timestamp);
    
  } catch (const std::bad_array_new_length& e) {
    BLAZE_LOG_ERROR("[ArchiveL] bad_array_new_length Exception caught in IOWorker destructor: {}", e.what());
  } catch (const std::exception& e) {
    BLAZE_LOG_ERROR("[ArchiveL] Exception caught in IOWorker destructor: {}", e.what());
  } catch (...) {
    BLAZE_LOG_ERROR("[ArchiveL] Unknown exception caught in IOWorker destructor. ");
  }

  try {
    FileClose();
  } catch (...) {
    BLAZE_LOG_ERROR("[ArchiveL] Exception while closing file in IOWorker destructor.");
  }
}

void IOWorker::SetPath(std::filesystem::path save_path) {
  BLAZE_LOG_INFO("[ArchiveL] IOWorker - SetPath [{}]", save_path.string());
  save_path_ = save_path;
};

bool IOWorker::PushVideoChunkBuffer(std::filesystem::path save_driver, std::shared_ptr<ArchiveChunkBuffer> media_datas) {
  if (media_datas == nullptr)    return false;

  DoEncryption(media_datas);
  {
    if (save_driver_.string().find(save_driver.string()) == std::string::npos) {
      if (!save_driver_.empty()) {
        change_file_ = true;
      }
      save_driver_ = save_driver;
    }
    {
      std::unique_lock<std::shared_mutex> lock(media_buffer_mtx_);
      media_buffers.push(media_datas);
    }
    QueueCheck();
  }
  //printf("[%s]session[%s]stream[%s]profile[%s] save_driver_[%s]\n", __FUNCTION__, channel_uuid_.c_str(), stream_id_.c_str(), profile_.ToString().c_str(), save_driver_.string().c_str());
  return true;
}

bool IOWorker::ProcessBuffer(unsigned int current_time) {
  std::unique_lock<std::mutex> lock(write_process_mtx_);

  while (true) {
    if (file_handle_ != InvalidHandle && file_write_timestamp_ != 0 && std::abs((int)current_time - (int)file_write_timestamp_) > SAVED_TIME) {
      BLAZE_LOG_INFO("[ArchiveL] File write time exceeded. close file. [{}]", write_file_name_.c_str());
      CloseSavedFile();
      file_write_timestamp_ = 0;
    }

    std::shared_ptr<BaseBuffer> base_data = nullptr;
    {
      std::unique_lock<std::shared_mutex> lock(media_buffer_mtx_);
      if (media_buffers.empty())
        return false;

      base_data = media_buffers.front();
      media_buffers.pop();
    }

    if (base_data == nullptr)
      return false;

    if (auto t_buffer = std::dynamic_pointer_cast<StreamBuffer>(base_data)) {
      if (t_buffer->archive_type == kArchiveTypeFlushFileClose || t_buffer->archive_type == kArchiveTypeFlush) {
        ProcessFlush(t_buffer->archive_type);
        continue;
      }
    }

    if (auto gop_buffers = std::dynamic_pointer_cast<ArchiveChunkBuffer>(base_data)) {
      if (!ProcessChunkBuffers(gop_buffers)) {
        return false;
      }
    }
  }
  return true;
}

bool IOWorker::ProcessChunkBuffers(std::shared_ptr<ArchiveChunkBuffer> gop_buffers) {
  if (!UpdateFileNameByCondition(file_handle_, gop_buffers->GetChunkBeginTime())) {
    BLAZE_LOG_ERROR("[ArchiveL] [{}][{}]{}] UpdateFileNameByCondition error", channel_uuid_, stream_id_, profile_.ToString());
    CallbackWirteStatusIWTAW(session_id_, false, gop_buffers->GetFrameInfos(save_driver_.string()));
    index_buffer_info_.Clear();
    return false;
  }
  if (!WriteIndexInfo(kArchiveTypeGopVideo, gop_buffers->GetChunkTotalSize(), gop_buffers->GetChunkBeginTime(), gop_buffers->GetChunkEndTime(),
                      gop_buffers->GetEncryptionType())) {
    BLAZE_LOG_ERROR("[ArchiveL] [{}][{}][{}] WriteIndexInfo error", channel_uuid_, stream_id_, profile_.ToString());
    CallbackWirteStatusIWTAW(session_id_, false, gop_buffers->GetFrameInfos(save_driver_.string()));
    index_buffer_info_.Clear();
    return false;
  }

  WriteBlockInfo(gop_buffers);
  bool fail = false;
  auto buffers = std::make_shared<ArchiveChunkBuffer>();
  while (true) {
    std::shared_ptr<StreamBuffer> buffer = gop_buffers->PopQueue();
    if (buffer == nullptr)
      break;
    
    buffers->PushQueue(buffer);
    FrameWriteIndexData frameinfo = {buffer.get()->archive_type, buffer.get()->timestamp_msec, save_driver_};
    index_buffer_info_.AddFrameInfo(frameinfo);

    if (fail)
      continue;

    if (!WriteBufferToDisk(buffer->buffer)) {
      BLAZE_LOG_ERROR("[ArchiveL] [{}][{}][{}] WriteBufferToDisk failed", channel_uuid_, stream_id_, profile_.ToString());
      fail = true;
      continue;
    }

    file_write_timestamp_ = static_cast<unsigned int>(time(nullptr) % UINT_MAX);
  }
  CheckMemoryBufferSize(IOWORKER_MEMORY_BUFFER_QUEUE_MAX_COUNT);
  if (session_id_.purpose == PnxMediaArchiveInfo::EPurpose::kRecord) {
    std::unique_lock<std::mutex> lock(mmy_buf_mtx_);
    memory_buffer_frames.push_back(buffers);
  }

  return true;
}

bool IOWorker::WriteIndexInfo(ArchiveType archive_type, int packet_size, unsigned long long begin_timestramp_msec, unsigned long long end_timestramp_msec, EncryptionType encryption_type) {
  long long current_file_position = platform_file_.GetDatFilePosition(file_handle_);
  if (current_file_position < 0) {
    BLAZE_LOG_ERROR("[ArchiveL] PlatformFile::GetDatFilePosition Fail  [{}], profile[{}]", channel_uuid_.c_str(), profile_.ToString().c_str());
    return false;
  }
  
  if (write_file_indexer_handle.buffer_pos + sizeof(ArchiveIndexHeader) > g_dio_indexer_size_) {
    BLAZE_LOG_ERROR("[ArchiveL] Header index buffer over flow!!!!! need size up!!  [{}], profile[{}]", channel_uuid_.c_str(), profile_.ToString().c_str());
    return false;
  }
  
  long long write_start_offset = (write_file_indexer_handle.buffer_pos / g_dio_page_size_) * g_dio_page_size_;
  
  if (!platform_file_.SetDatFilePosition(file_handle_, write_start_offset)) {
    BLAZE_LOG_ERROR("[ArchiveL] PlatformFile::SetDatFilePosition to offset[{}] Fail [{}], profile[{}]", write_start_offset, channel_uuid_.c_str(),
    profile_.ToString().c_str());
    return false;
  }
  
  ArchiveIndexHeader archive_index_header;
  archive_index_header.type = archive_type;
  archive_index_header.file_offset = current_file_position + write_file_data_handle_->buffer_pos;
  archive_index_header.packet_size = packet_size;
  archive_index_header.begin_timestamp_msec = begin_timestramp_msec;
  archive_index_header.end_timestamp_msec = end_timestramp_msec;
  archive_index_header.encryption_type = encryption_type;
  
  memcpy(write_file_indexer_handle.writebuffer + write_file_indexer_handle.buffer_pos, &archive_index_header, sizeof(ArchiveIndexHeader));
  write_file_indexer_handle.buffer_pos += sizeof(ArchiveIndexHeader);
  write_file_indexer_handle.archive_indexers_.push_back(archive_index_header);
  
  long long data_to_write_size = write_file_indexer_handle.buffer_pos - write_start_offset;
  long long aligned_write_size = (data_to_write_size / g_dio_page_size_ + 1) * g_dio_page_size_;
  
  if (write_start_offset + aligned_write_size > g_dio_indexer_size_) {
    aligned_write_size = g_dio_indexer_size_ - write_start_offset;
  }
  
  if (!platform_file_.FileWrite(file_handle_, write_file_indexer_handle.writebuffer + write_start_offset, aligned_write_size)) {
    BLAZE_LOG_ERROR("[ArchiveL] PlatformFile::FileWrite Fail  [{}], profile[{}]", channel_uuid_.c_str(), profile_.ToString().c_str());
    write_file_indexer_handle.buffer_pos -= sizeof(ArchiveIndexHeader);
    write_file_indexer_handle.archive_indexers_.pop_back();
    if (!platform_file_.SetDatFilePosition(file_handle_, current_file_position)) {
      BLAZE_LOG_ERROR("[ArchiveL] PlatformFile::SetDatFilePosition Fail  [{}], profile[{}]", channel_uuid_.c_str(), profile_.ToString().c_str());
    }
    return false;
  }
  
  if (!platform_file_.SetDatFilePosition(file_handle_, current_file_position)) {
    BLAZE_LOG_ERROR("[ArchiveL] PlatformFile::SetDatFilePosition Fail  [{}], profile[{}]", channel_uuid_.c_str(), profile_.ToString().c_str());
    return false;
  }
  return true;
}

bool IOWorker::WriteBlockInfo(std::shared_ptr<BaseBuffer> base_buffer) {
  if (auto gop_buffers = std::dynamic_pointer_cast<ArchiveChunkBuffer>(base_buffer)) {
    int buffer_size = gop_buffers.get()->GetHeadersSize();
    if (index_block_info_.GetBufferSize() < buffer_size) {
      index_block_info_.SetBufferSize(buffer_size);
    }
    index_block_info_.GetBuffer().get()->size = buffer_size;
    memcpy(index_block_info_.GetBuffer().get()->data.get(), gop_buffers.get()->GetHeadersMemory().get(), gop_buffers.get()->GetHeadersSize());
  } else {
    return false;
  }

  auto index_buffer =
      std::make_shared<Pnx::Media::MediaSourceFrame>((unsigned char*)index_block_info_.GetBuffer()->data.get(), index_block_info_.GetBufferSize());
  return WriteBufferToDisk(index_buffer);
}

bool IOWorker::WriteBufferToDisk(Pnx::Media::MediaSourceFramePtr buffer_data) {
  int remain__space = write_file_data_handle_->dio_buffer_size_ - write_file_data_handle_->buffer_pos;
  if (remain__space <= 0) {
    BLAZE_LOG_ERROR("[ArchiveL] size of remain__space is negative. remain__space[{}]", remain__space);
    return false;
  }

  if (remain__space > buffer_data->dataSize()) {
    memcpy(write_file_data_handle_->writebuffer + write_file_data_handle_->buffer_pos, buffer_data->data(), buffer_data->dataSize());

    write_file_data_handle_->buffer_pos += buffer_data->dataSize();

  } else {
    memcpy(write_file_data_handle_->writebuffer + write_file_data_handle_->buffer_pos, buffer_data->data(), remain__space);

    if (!WriteDisk(file_handle_, write_file_data_handle_->writebuffer, write_file_data_handle_->dio_buffer_size_)) {
      return false;
    }
    CheckMemoryBufferSize(1);
    int remain_size = buffer_data->dataSize() - remain__space;
    CheckBufferSize(remain_size);

    memcpy(write_file_data_handle_->writebuffer, buffer_data->data() + remain__space, remain_size);
    write_file_data_handle_->buffer_pos = remain_size;
    if (remain_size > write_file_data_handle_->dio_buffer_size_)
      FireBuffer();

    AdjustWriteBlockSize();
  }
  return true;
}

bool IOWorker::WriteDisk(FileHandle handle, char* buffer, int write_size) {
  if (!platform_file_.FileWrite(handle, write_file_data_handle_->writebuffer, write_size)) {
    BLAZE_LOG_ERROR("[ArchiveL] PlatformFile::FileWrite Fail");
    WriteDiskFail();
    return false;
  }

  bitrate_monitor_.AddData(write_size);
  // success
  WriteDiskSuccess();

  if (write_file_indexer_handle.buffer_pos >= sizeof(ArchiveIndexHeader))
    file_index_last_pos_ = write_file_indexer_handle.buffer_pos - sizeof(ArchiveIndexHeader);

  write_file_data_handle_->buffer_pos = 0;
  return true;
}

bool IOWorker::WriteFlushMemory(FileHandle file_handle) {
  if (write_file_data_handle_->buffer_pos <= 0) {
    BLAZE_LOG_INFO("[ArchiveL] no data to flush");
    return false;
  }

  BLAZE_LOG_INFO("[ArchiveL] WriteFlushMemory buffer pos [{}]", write_file_data_handle_->buffer_pos);
  WriteRemain(file_handle, write_file_data_handle_->buffer_pos);
  index_buffer_info_.Clear();
  return true;
}

bool IOWorker::WriteFileChange(std::string filetime, unsigned long long timestamp) {

  FileClose();
  write_file_name_ = FileSearcher::GetDatFileName(save_driver_ / save_path_, channel_uuid_, filetime, profile_, timestamp);

  try {
    if (std::filesystem::exists(write_file_name_)) {
      write_file_name_ = FileSearcher::GetDatFileName(save_driver_ / save_path_, channel_uuid_, filetime, profile_, timestamp + 1);
    }
  } catch (const std::filesystem::filesystem_error& e) {
    BLAZE_LOG_ERROR("[ArchiveL] filesystem error during exists check: {} - path: {}", e.what(), write_file_name_.c_str());
    return false;
  }

  file_handle_ = platform_file_.FileDirectOpen(write_file_name_);
  if (file_handle_ == InvalidHandle) {
    BLAZE_LOG_ERROR("[ArchiveL] create file fail[{}]", write_file_name_.c_str());
    return false;
  }
      
  BLAZE_LOG_INFO("[ArchiveL] create file : {}", write_file_name_.c_str());
  
  CreateDeviceInfoFile();
  
  return InitBuffer();
}

bool IOWorker::FileClose() {
  if (file_handle_ != InvalidHandle) {

    if (!WriteRemain(file_handle_, write_file_data_handle_->buffer_pos)) {
      file_handle_ = InvalidHandle;
      return false;
    }

    BLAZE_LOG_INFO("[ArchiveL] close file [{}]", write_file_name_.c_str());
    if (!platform_file_.CloseFile(file_handle_)) {
      printf("[ERROR] %s Failed to close a file. (path: %s)\n", __FUNCTION__, write_file_name_.c_str());
      file_handle_ = InvalidHandle;
      return false;
    }
    file_handle_ = InvalidHandle;
  }
  return true;
}

bool IOWorker::InitBuffer() {
  write_file_indexer_handle.buffer_pos = 0;
  write_file_indexer_handle.archive_indexers_.clear();

  int write_size = 4;
  memcpy(write_file_indexer_handle.writebuffer, "PFFT", write_size);
  write_file_indexer_handle.buffer_pos += write_size;

  short file_version = ARCHIVER_VERSOIN;
  write_size = 2;
  memcpy(write_file_indexer_handle.writebuffer + write_file_indexer_handle.buffer_pos, &file_version, write_size);
  write_file_indexer_handle.buffer_pos += write_size;

  if (!platform_file_.FileWrite(file_handle_, write_file_indexer_handle.writebuffer, g_dio_indexer_size_)) {
    BLAZE_LOG_ERROR("[ArchiveL] PlatformFile::FileWrite Fail [{}]", write_file_name_.c_str());
    return false;
  }

  return true;
}

void IOWorker::FireBuffer() {
  ////////////////////////////////////
  // If the remaining BUFFER SIZE is larger than the WRITE BUFFER SIZE, it will be used up.

  int roof_count = write_file_data_handle_->buffer_pos / write_file_data_handle_->dio_buffer_size_;
  int remain_memory = write_file_data_handle_->buffer_pos % write_file_data_handle_->dio_buffer_size_;

  int memory_position = 0;
  for (int i = 0; i < roof_count; i++) {
    memory_position = write_file_data_handle_->dio_buffer_size_ * i;
    if (!platform_file_.FileWrite(file_handle_, write_file_data_handle_->writebuffer + memory_position, write_file_data_handle_->dio_buffer_size_)) {
      BLAZE_LOG_ERROR("[ArchiveL] PlatformFile::FileWrite Fail");
    }
  }

  memcpy(write_file_data_handle_->writebuffer, write_file_data_handle_->writebuffer + (roof_count * write_file_data_handle_->dio_buffer_size_), remain_memory);
  write_file_data_handle_->buffer_pos = remain_memory;
}

bool IOWorker::WriteRemain(FileHandle hFile, int remain_size) {
  int sector_size = g_dio_page_size_;
  int writes_size = 0;
  if (remain_size < sector_size)
    writes_size = sector_size;
  else
    writes_size = (remain_size / sector_size) * sector_size + sector_size;

  if (!WriteDisk(hFile, write_file_data_handle_->writebuffer, writes_size)) {
    // on failure, reset buffer pos and propagate failure
    write_file_data_handle_->buffer_pos = 0;
    BLAZE_LOG_ERROR("[ArchiveL] PlatformFile::FileWrite Fail ");
    return false;
  }
  return true;
}

void IOWorker::CheckBufferSize(int remain_size) {
  if (write_file_data_handle_->real_buffer_size < remain_size) {
    if (write_file_data_handle_->writebuffer) {

#ifdef _WIN32
      char* new_buffer = (char*)VirtualAlloc(nullptr, remain_size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
      if (new_buffer == nullptr) {
        std::cerr << "[ArchiveL] Failed to allocate memory : " << GetLastError() << std::endl;
        write_file_data_handle_->writebuffer = nullptr;

        write_file_data_handle_->real_buffer_size = 0;
        return;
      }
#else
      char* new_buffer = nullptr;
      if (posix_memalign((void**)&new_buffer, g_dio_page_size_, remain_size) != 0) {
        BLAZE_LOG_ERROR("[ArchiveL] Failed to allocate memory");
        write_file_data_handle_->writebuffer = nullptr;
        return;
      }
#endif

      memcpy(new_buffer, write_file_data_handle_->writebuffer, write_file_data_handle_->real_buffer_size);

#ifdef _WIN32
      if (write_file_data_handle_->writebuffer != nullptr)
        VirtualFree(write_file_data_handle_->writebuffer, 0, MEM_RELEASE);
#else
      if (write_file_data_handle_->writebuffer != nullptr)
        free(write_file_data_handle_->writebuffer);
#endif

      write_file_data_handle_->writebuffer = new_buffer;
      write_file_data_handle_->real_buffer_size = remain_size;
    }
  }
}

bool IOWorker::UpdateFileNameByCondition(FileHandle file_handle, unsigned long long timestamp) {
  std::string filetime = FileSearcher::GetFolderFrTime(timestamp);
  if (file_random_number_ == 0) {
    file_random_number_ = GetRandomFileNumber();
  }
  if (file_handle == InvalidHandle || file_handle == 0 || change_file_ == true ||
      (timestamp - file_begin_timestamp_) / 1000 > (SAVED_TIME + file_random_number_) || filetime.compare(cur_filetime) != 0) {
    
    if (timestamp < file_begin_timestamp_) {
      HandleTimestampRollback(timestamp);
    }

    cur_filetime = filetime;
    file_begin_timestamp_ = timestamp;
    change_file_ = false;
    file_random_number_ = GetRandomFileNumber();

    if (!WriteFileChange(cur_filetime, timestamp))
      return false;
  }
  return true;
}

bool IOWorker::HandleTimestampRollback(unsigned long long timestamp) {
  if (write_file_indexer_handle.archive_indexers_.size() > 3) {
    BLAZE_LOG_WARN("[ArchiveL] Timestamp rollback detected but index buffer has more than 3 entries. No backup will be made. timestamp[{}] < file_begin[{}]", 
                   timestamp, file_begin_timestamp_);
    return false;
  }
  if (file_handle_ == InvalidHandle || write_file_name_.empty()) {
    return false;
  }
  
  BLAZE_LOG_WARN("[ArchiveL] Timestamp rollback detected! Backing up current file. timestamp[{}] < file_begin[{}]", 
                 timestamp, file_begin_timestamp_);
  
  std::string original_file = write_file_name_;
  
  if (!FileClose()) {
    return false;
  }
  
  try {
    if (std::filesystem::exists(original_file)) {
      std::filesystem::path original_path(original_file);
      std::string backup_name = "bk_" + original_path.filename().string();
      std::filesystem::path backup_path = original_path.parent_path() / backup_name;
      
      if (std::filesystem::exists(backup_path)) {
        backup_name = "bk_" + std::to_string(std::time(nullptr)) + "_" + original_path.filename().string();
        backup_path = original_path.parent_path() / backup_name;
      }
      
      std::filesystem::rename(original_file, backup_path);
      BLAZE_LOG_INFO("[ArchiveL] File backed up: {} -> {}", original_file, backup_path.string());
      return true;
    }
  } catch (const std::exception& e) {
    BLAZE_LOG_ERROR("[ArchiveL] Failed to backup file: {}, error: {}", original_file, e.what());
  }
  
  return false;
}

int IOWorker::GetRandomFileNumber() {
  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_int_distribution<> distrib(-10, 10);
  return distrib(gen);
}

void IOWorker::WriteDiskSuccess() {
  // Success
  write_file_data_handle_->buffer_pos = 0;
  std::shared_ptr<std::vector<FrameWriteIndexData>> frameinfos = index_buffer_info_.GetFrameInfos(save_driver_.string());
  CallbackWirteStatusIWTAW(session_id_, true, frameinfos);
  index_buffer_info_.ResetInfo();
  write_time_ = std::chrono::high_resolution_clock::now();
}

void IOWorker::WriteDiskFail() {
  write_file_indexer_handle.buffer_pos = file_index_last_pos_;

  // Move the file index position to the previous memory block position.
  ArchiveUtil::SetZeroAfter(write_file_indexer_handle.writebuffer, g_dio_indexer_size_, write_file_indexer_handle.buffer_pos);

  long long current_file_position = platform_file_.GetDatFilePosition(file_handle_);
  if (current_file_position > 0) {
    if (!platform_file_.SetDatFilePosition(file_handle_, 0)) {
      BLAZE_LOG_ERROR("[ArchiveL] PlatformFile::SetDatFilePosition Fail ");
    }

    if (!platform_file_.FileWrite(file_handle_, write_file_indexer_handle.writebuffer, g_dio_indexer_size_)) {
      BLAZE_LOG_ERROR("[ArchiveL] PlatformFile::FileWrite Fail ");
      write_file_indexer_handle.buffer_pos = current_file_position;
    }

    if (!platform_file_.SetDatFilePosition(file_handle_, current_file_position)) {
      BLAZE_LOG_ERROR("[ArchiveL] PlatformFile::SetDatFilePosition Fail");
    }
  } else {
    BLAZE_LOG_ERROR("[ArchiveL] PlatformFile::GetDatFilePosition Fail");
  }

  // fail
  write_file_data_handle_->buffer_pos = 0;
  std::shared_ptr<std::vector<FrameWriteIndexData>> frameinfos = index_buffer_info_.GetFrameInfos(save_driver_.string());
  CallbackWirteStatusIWTAW(session_id_, false, frameinfos);
  index_buffer_info_.Clear();
}

// read coding
std::optional<std::pair<ArchiveIndexHeader, ArchiveObject>> IOWorker::GetFrame(int time_stamp) {
  std::pair<ArchiveIndexHeader, ArchiveObject> ret_object;
  return ret_object;
}

std::optional<std::pair<ArchiveIndexHeader, ArchiveObject>> IOWorker::GetPrevFrame() {
  std::pair<ArchiveIndexHeader, ArchiveObject> ret_object;
  return ret_object;
}

std::optional<std::pair<ArchiveIndexHeader, ArchiveObject>> IOWorker::GetNextFrame() {

  std::pair<ArchiveIndexHeader, ArchiveObject> ret_object;
  return ret_object;
}

bool IOWorker::DeleteEmptyFolders(const std::string& folderPath) {
    try {
      if (std::filesystem::is_empty(folderPath)) {
        if (std::filesystem::remove(folderPath)) {
          BLAZE_LOG_INFO("[ArchiveL] Deleted empty folder[{}]", folderPath.c_str());
          DeleteEmptyFolders(std::filesystem::path(folderPath).parent_path().string());
            } else {
                std::cerr << "[ArchiveL] Failed to delete folder: " << folderPath << std::endl;
                return false;
            }
        } else {
            return true;
        }
    } catch (const std::exception& e) {
        std::cerr << "[ArchiveL] Error deleting folder: " << folderPath << std::endl;
        std::cerr << e.what() << std::endl;
        return false;
    }
    return true;
}

bool IOWorker::Delete_File(const std::string& filePath) {
  try {
    if (std::filesystem::remove(filePath)) {
      BLAZE_LOG_INFO("[ArchiveL] File deleted [{}]", filePath.c_str());
      return true;
    } else {
      BLAZE_LOG_INFO("[ArchiveL] Failed to delete file [{}]", filePath.c_str());
      return false;
    }
  } catch (const std::exception& e) {
    std::cerr << "Error deleting file: " << filePath << std::endl;
    std::cerr << e.what() << std::endl;
    return false;
  }
}

void IOWorker::QueueCheck() {
  std::shared_ptr<std::vector<FrameWriteIndexData>> frameinfos = std::make_shared<std::vector<FrameWriteIndexData>>();
  std::shared_ptr<BaseBuffer> base_data = nullptr;

  bool is_flush_buffer = false;
  ArchiveType flush_type = kArchiveTypeFlush;
  std::unique_lock<std::shared_mutex> lock(media_buffer_mtx_);

 

  // TODO : GA v1.3 검증 이후 삭제  
  ////////////////////////////////////////////
  auto buffer_size = media_buffers.size();
  if (debug_frame_drop_ && buffer_size >= 4) {
    try {           
      auto now = std::chrono::system_clock::now();
      auto time_t_now = std::chrono::system_clock::to_time_t(now);
      auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
      
      std::ofstream debug_log("/root/frame_buffer.log", std::ios::app);
      if (debug_log.is_open()) {
        std::tm* timeinfo = std::localtime(&time_t_now);
        debug_log << std::put_time(timeinfo, "%Y-%m-%d %H:%M:%S") << "." << std::setfill('0') << std::setw(3) << ms.count() 
              << " [ArchiveL] warnning media_buffers.size >= 8 - session_id[" << channel_uuid_ << "], profile[" << profile_.ToString() 
              << "], drive[" << save_driver_.string() << "], media_buffers.size()[" << buffer_size << "]" << std::endl;
        debug_log.close();
      }
    } catch (const std::exception& e) {
      BLAZE_LOG_ERROR("[ArchiveL] Failed to write debug log: {}", e.what());
    }
  }
  //////////////////////////////////////////////

  while (media_buffers.size() > buffer_size_) {
    if (media_buffers.empty())
      break;

    base_data = media_buffers.front();
    if (auto t_buffer = std::dynamic_pointer_cast<StreamBuffer>(base_data)) {
      if (t_buffer->archive_type == kArchiveTypeFlushFileClose || t_buffer->archive_type == kArchiveTypeFlush) {
        is_flush_buffer = true;
        if (flush_type != kArchiveTypeFlushFileClose)
          flush_type = t_buffer->archive_type;
      }
    }
    media_buffers.pop();

    if (auto gop_buffers = std::dynamic_pointer_cast<ArchiveChunkBuffer>(base_data)) {
      while (true) {
        std::shared_ptr<StreamBuffer> buffer = gop_buffers->PopQueue();
        if (buffer == nullptr)
          break;

        FrameWriteIndexData index_data = {buffer.get()->archive_type, buffer.get()->timestamp_msec, save_driver_.string()};
        frameinfos->push_back(index_data);
      }
    }
  }

  if (is_flush_buffer) {
    if (!media_buffers.empty()) {
      base_data = media_buffers.front();
      if (std::dynamic_pointer_cast<ArchiveChunkBuffer>(base_data)) {
        auto flush_stream = std::make_shared<StreamBuffer>();
        flush_stream->archive_type = flush_type;
        media_buffers.front() = flush_stream;
        BLAZE_LOG_INFO("[ArchiveL] QueueCheck: replaced ArchiveChunkBuffer with StreamBuffer(flush) session[{}], stream[{}], new_front_type[{}]", channel_uuid_, stream_id_.c_str(), flush_type);
      }
    }
  }
  lock.unlock();

  if (frameinfos->size() > 0) {
    if (cnt_buffer_over_ == 0) {
      BLAZE_LOG_FLUSH_BACKTRACE();
    }
    cnt_buffer_over_ = cnt_buffer_over_ >= 10 ? 0 : cnt_buffer_over_ + 1;
    BLAZE_LOG_ERROR("[ArchiveL] Buffer Over flow !!!!! session_id[{}], profile[{}], drive[{}], media_buffers.size()[{}]", channel_uuid_, profile_.ToString().c_str(), save_driver_.string(), buffer_size);
    
    // TODO : GA v1.3 검증 이후 삭제 
    ////////////////////////////
    if (debug_frame_drop_) {
      try {                      
        auto now = std::chrono::system_clock::now();
        auto time_t_now = std::chrono::system_clock::to_time_t(now);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
        
        std::ofstream debug_log("/root/frame_drop.log", std::ios::app);
        if (debug_log.is_open()) {
          std::tm* timeinfo = std::localtime(&time_t_now);
          debug_log << std::put_time(timeinfo, "%Y-%m-%d %H:%M:%S") << "." << std::setfill('0') << std::setw(3) << ms.count() 
                << " [ArchiveL] Buffer Overflow - session_id[" << channel_uuid_ << "], profile[" << profile_.ToString() 
                << "], drive[" << save_driver_.string() << "], media_buffers.size()[" << buffer_size << "]" << std::endl;
          debug_log.close();
        }       
      } catch (const std::exception& e) {
        BLAZE_LOG_ERROR("[ArchiveL] Failed to write debug log: {}", e.what());
      }
    }
    //////////////////////////////

    CallbackWirteStatusIWTAW(session_id_, false, frameinfos);
    index_buffer_info_.Clear();
  }
}

void IOWorker::AdjustWriteBlockSize() {
  std::unique_lock<std::shared_mutex> lock(media_buffer_mtx_);
  if (media_buffers.size() > IOWORKER_QUEUE_MAX_COUNT / 2 + 1 && write_file_data_handle_->dio_buffer_size_ != dio_buffer_busy_size) {
    BLAZE_LOG_INFO("[ArchiveL] ++++++++++++ IOWorker Buffer Busy Change dio buffer size up session_id[{}], profile[{}], buffer_size_[{}]", channel_uuid_,
                profile_.ToString().c_str(), media_buffers.size());

    write_file_data_handle_->dio_buffer_size_ = dio_buffer_busy_size;
  } else if (media_buffers.size() <= IOWORKER_QUEUE_MAX_COUNT / 2 - 1 && write_file_data_handle_->buffer_pos < dio_buffer_nomal_size &&
             write_file_data_handle_->dio_buffer_size_ != dio_buffer_nomal_size) {
    BLAZE_LOG_INFO("[ArchiveL] ------------ IOWorker Buffer Normal Change dio buffer size down session_id[{}], profile[{}], buffer_size_[{}]", channel_uuid_,
                profile_.ToString().c_str(), media_buffers.size());

    write_file_data_handle_->dio_buffer_size_ = dio_buffer_nomal_size;
  }
}

bool IOWorker::CloseSavedFile() {
  write_file_data_handle_->buffer_pos = 0;

  if (file_handle_ != InvalidHandle) {
    if (!platform_file_.CloseFile(file_handle_)) {
      printf("[ERROR] %s Failed to close a file. (path: %s)\n", __FUNCTION__, write_file_name_.c_str());
      file_handle_ = InvalidHandle;
      return false;
    }
    file_handle_ = InvalidHandle;
    return true;
  }
  return true;
}

bool IOWorker::DoEncryption(std::shared_ptr<ArchiveChunkBuffer> media_datas) {
  EncryptionType encrytion_type = encrytion_type_;
  if (encrytion_type != kEncryptionNone) {
    int total_size = 0;
    for (auto buffer : *media_datas) {
      Pnx::Media::MediaSourceFramePtr enc_buffer = stream_crytor_.Encrypt(buffer->buffer, encrytion_type);
      buffer->buffer = nullptr;
      buffer->buffer = enc_buffer;
      buffer->packet_size = buffer->buffer->dataSize();
      if (buffer->archive_type == kArchiveTypeFrameVideo) {
        static_cast<VideoData*>(buffer.get())->archive_header.packet_size = buffer->packet_size;
      } else if (buffer->archive_type == kArchiveTypeAudio) {
        static_cast<AudioData*>(buffer.get())->archive_header.packet_size = buffer->packet_size;
      } else if (buffer->archive_type == kArchiveTypeMeta) {
        static_cast<MetaData*>(buffer.get())->archive_header.packet_size = buffer->packet_size;
      }

      total_size += buffer->buffer->dataSize();
    }
    media_datas->SetEncryptionType(encrytion_type);
    media_datas->SetChunkTotalSize(total_size);
  }

  return true;
}

void IOWorker::Flush(FlushType flush_type) {
  BLAZE_LOG_INFO("[ArchiveL] Flush stream_id_[{}], flush_type[{}] ", stream_id_.c_str(), flush_type);
  std::unique_lock<std::shared_mutex> lock(media_buffer_mtx_);
  if (!media_buffers.empty()) {
    auto last_data = media_buffers.back();
    if (auto last_stream = std::dynamic_pointer_cast<StreamBuffer>(last_data)) {
      if (last_stream->archive_type == kArchiveTypeFlush || last_stream->archive_type == kArchiveTypeFlushFileClose) {
        if (flush_type == kFileClose) {
          BLAZE_LOG_INFO("[ArchiveL] --- (change last flush type to file close) stream_id_[{}], flush_type[{}] ", stream_id_.c_str(), flush_type);
          last_stream->archive_type = kArchiveTypeFlushFileClose;
        }

        BLAZE_LOG_INFO("[ArchiveL] --- (skip flush, already last is flush) stream_id_[{}], flush_type[{}] ", stream_id_.c_str(), flush_type);
        return;
      }
    }
  }

  std::shared_ptr<StreamBuffer> flush_datas = std::make_shared<StreamBuffer>();
  if (flush_type == kFileClose)
    flush_datas->archive_type = kArchiveTypeFlushFileClose;
  else
    flush_datas->archive_type = kArchiveTypeFlush;
  media_buffers.push(flush_datas);
}

void IOWorker::ProcessFlush(ArchiveType archive_type) {
  if (file_handle_ == InvalidHandle) {
    BLAZE_LOG_INFO("[ArchiveL] skip flush, file handle is invalid");
    return;
  }

  BLAZE_LOG_INFO("[ArchiveL] ProcessFlush +++");
  {
    CheckMemoryBufferSize(1);
    std::unique_lock<std::shared_mutex> lock(media_buffer_mtx_);
    BLAZE_LOG_INFO("[ArchiveL] PushVideoChunkBuffer stream_id_[{}] ProcessFlush() 2, buffersize[{}], flushtype[{}]", stream_id_.c_str(), media_buffers.size(),
                archive_type); 
  }
  if (archive_type == kArchiveTypeFlushFileClose) {
    FileClose();
    index_buffer_info_.Clear();
  } else {
    WriteFlushMemory(file_handle_);
  }
  BLAZE_LOG_INFO("[ArchiveL] ProcessFlush ---", __func__);
}

std::tuple<bool, int> IOWorker::GetSaveStatus() {
  std::chrono::time_point<std::chrono::high_resolution_clock> cur_time = std::chrono::high_resolution_clock::now();
  std::chrono::duration<double, std::milli> elapsed = cur_time - write_time_;
  bool record_status = false;
  if (elapsed.count() > 10000) {
    record_status = false;
  } else
    record_status = true;

  return std::make_tuple(record_status, bitrate_monitor_.GetBitrate());
}

std::shared_ptr<ArchiveChunkBuffer> IOWorker::GetMemoryFrames(int64_t timestamp) {
  std::unique_lock<std::mutex> lock(mmy_buf_mtx_);
  for (auto data : memory_buffer_frames) {
    if (auto gop_buffers = std::dynamic_pointer_cast<ArchiveChunkBuffer>(data)) {
      if (gop_buffers->GetChunkEndTime() > timestamp && gop_buffers->GetChunkBeginTime() < timestamp) {
        BLAZE_LOG_INFO("[ArchiveL] IOWorker::GetMemoryFrames 1 : timestamp[{}], BeginTime[{}], EndTime[{}]", timestamp, gop_buffers->GetChunkBeginTime(),
                    gop_buffers->GetChunkEndTime());
        return gop_buffers;
      }
    }
  }
  return nullptr;
}

void IOWorker::CheckMemoryBufferSize(int max_buffer_count) {
  std::unique_lock<std::mutex> lock(mmy_buf_mtx_);
  if (memory_buffer_frames.size() > max_buffer_count) {
    auto erase_begin = memory_buffer_frames.begin();
    auto erase_end = memory_buffer_frames.end() - max_buffer_count;
    for (auto it = erase_begin; it != erase_end; ++it) {
      if (auto gop_buffers = std::dynamic_pointer_cast<ArchiveChunkBuffer>(*it)) {
        gop_buffers->Clear();
      }
    }
    memory_buffer_frames.erase(erase_begin, erase_end);
  }
}

void IOWorker::CreateDeviceInfoFile() {
  if (!channel_index_.has_value()) {
    return;
  }
  
  // Build filename: _M_[MAC]_I_[IP]_C_[INDEX].deviceinfo
  std::string safe_mac_address;
  if (mac_address_.has_value()) {
    safe_mac_address = mac_address_.value();
    std::replace(safe_mac_address.begin(), safe_mac_address.end(), ':', '-');
  }
  
  std::string safe_ip_address;
  if (ip_address_.has_value()) {
    safe_ip_address = ip_address_.value();
    std::replace(safe_ip_address.begin(), safe_ip_address.end(), '.', '-');
  }
  
  std::filesystem::path channel_dir = save_driver_ / save_path_ / channel_uuid_;
  std::string deviceinfo_filename = "_M_" + safe_mac_address + "_I_" + safe_ip_address + "_C_" + std::to_string(channel_index_.value()) + ".deviceinfo";
  std::filesystem::path deviceinfo_path = channel_dir / deviceinfo_filename;

  try {
    if (std::filesystem::exists(deviceinfo_path)) {
      return;
    }

    if (std::filesystem::exists(channel_dir) && std::filesystem::is_directory(channel_dir)) {
      for (const auto& entry : std::filesystem::directory_iterator(channel_dir)) {
        if (entry.is_regular_file() && entry.path().extension() == ".deviceinfo") {
          if (entry.path() != deviceinfo_path) {
            std::filesystem::remove(entry.path());
            BLAZE_LOG_INFO("[ArchiveL] deleted old device info file: {}", entry.path().string());
          }
        }
      }
    }
    
    if (!std::filesystem::exists(deviceinfo_path)) {
      std::ofstream deviceinfo_file(deviceinfo_path);
      if (deviceinfo_file.is_open()) {
        if (mac_address_.has_value()) {
          deviceinfo_file << "mac_address=" << mac_address_.value() << std::endl;
        }
        if (ip_address_.has_value()) {
          deviceinfo_file << "ip_address=" << ip_address_.value() << std::endl;
        }
        deviceinfo_file << "channel_index=" << channel_index_.value() << std::endl;
        deviceinfo_file.close();
        BLAZE_LOG_INFO("[ArchiveL] created device info file: {}", deviceinfo_path.string());
      } else {
        BLAZE_LOG_ERROR("[ArchiveL] failed to create device info file: {}", deviceinfo_path.string());
      }
    }
  } catch (const std::exception& e) {
    BLAZE_LOG_ERROR("[ArchiveL] exception while creating device info file: {}", e.what());
  }
}

// Check if this IOWorker has an open file (returns true if file is open)
bool IOWorker::IsFileOpen() const {
  return (file_handle_ != InvalidHandle);
}
