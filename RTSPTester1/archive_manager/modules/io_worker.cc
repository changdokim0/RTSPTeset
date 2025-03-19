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

#include <chrono>
#include <cmath>

IOWorker::IOWorker(std::string session_id, MediaProfile profile) : session_id_(session_id), profile_(profile) {

  std::string profileaa = profile.ToString();
  if (profile == MediaProfile::kSecondary) {
    dio_buffer_nomal_size = g_dio_page_size_ * 122;
    dio_buffer_busy_size = g_dio_page_size_ * 244;
  } else {
    dio_buffer_nomal_size = g_dio_page_size_ * 244;
    dio_buffer_busy_size = g_dio_page_size_ * (244 * 2);
  }

  write_file_data_handle_ = std::make_shared<DataFileHandle>(g_dio_page_size_ * 30, dio_buffer_busy_size);
  SPDLOG_INFO("[ArchiveL] ioworker - create memory buffer   [{}]", dio_buffer_busy_size);
  SPDLOG_INFO("[ArchiveL] ioworker - create stream  [{}]", session_id.c_str());
}

IOWorker::~IOWorker() {
  SPDLOG_INFO("[ArchiveL] ioworker - delete stream  [{}]", stream_id_.c_str());
}

void IOWorker::SetPath(std::filesystem::path save_path) {
  save_path_ = save_path;
};

bool IOWorker::PushVideoChunkBuffer(std::filesystem::path save_driver, std::shared_ptr<ArchiveChunkBuffer> media_datas) {
  if (media_datas == nullptr)    return false;

  DoEncryption(media_datas);
  {
    if (save_driver_.string().find(save_driver.string()) == std::string::npos) {
      if (!save_driver_.empty())
        change_file_ = true;

      save_driver_ = save_driver;
    }
    {
      std::unique_lock<std::shared_mutex> lock(media_buffer_mtx_);
      media_buffers.push(media_datas);
    }
    QueueCheck();
  }
  return true;
}

bool IOWorker::ProcessBuffer(unsigned int current_time) {

  MemoryCheck();

  while (true) {
    if (file_handle != InvalidHandle && file_write_timestamp_ != 0 && std::abs((int)current_time - (int)file_write_timestamp_) > SAVED_TIME ) {
      SPDLOG_INFO("[ArchiveL] File write time exceeded. close file. [{}]", write_file_name_.c_str());
      CloseSavedFile();
      file_write_timestamp_ = 0;
    }
    if (reserve_flush_) {
      reserve_flush_ = false;
      ProcessFlush();
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

    if (auto gop_buffers = std::dynamic_pointer_cast<ArchiveChunkBuffer>(base_data)) {
      if (!ProcessChunkBuffers(gop_buffers)) {
        return false;
      }
    }
  }
  return true;
}

bool IOWorker::ProcessChunkBuffers(std::shared_ptr<ArchiveChunkBuffer> gop_buffers) {
  if (!UpdateFileNameByCondition(file_handle, gop_buffers->GetChunkBeginTime())) {
    CallbackWirteStatusIWTAW(session_id_, profile_, false, gop_buffers->GetFrameInfos());
    index_buffer_info_.Clear();
    return false;
  }
  if (!WriteIndexInfo(kArchiveTypeGopVideo, gop_buffers->GetChunkTotalSize(), gop_buffers->GetChunkBeginTime(), gop_buffers->GetChunkEndTime(),
                      gop_buffers->GetEncryptionType())) {
    SPDLOG_ERROR("[ArchiveL] WriteIndexInfo error");
    CallbackWirteStatusIWTAW(session_id_, profile_, false, gop_buffers->GetFrameInfos());
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
      fail = true;
      continue;
    }

    file_write_timestamp_ = static_cast<unsigned int>(time(nullptr));
  }
  std::unique_lock<std::mutex> lock(mmy_buf_mtx_);
  memory_buffer_frames.push_back(buffers);
  return true;
}

bool IOWorker::WriteIndexInfo(ArchiveType archive_type, int packet_size, unsigned long long begin_timestramp_msec, unsigned long long end_timestramp_msec,
                              EncryptionType encryption_type) {
  long long current_file_position = platform_file_.GetDatFilePosition(file_handle);
  if (!platform_file_.SetDatFilePosition(file_handle, 0)) {
    SPDLOG_ERROR("[ArchiveL] PlatformFile::SetDatFilePosition Fail  [{}], profile[{}]", session_id_.c_str(), profile_.ToString().c_str());
    return false;
  }
  if (write_file_indexer_handle.buffer_pos + sizeof(ArchiveIndexHeader) > g_dio_indexer_size_){
    SPDLOG_ERROR("[ArchiveL] Header index buffer over flow!!!!! need size up!!  [{}], profile[{}]", session_id_.c_str(), profile_.ToString().c_str());
    return false;
  }
  ArchiveIndexHeader archive_index_header;
  archive_index_header.type = archive_type;
  archive_index_header.file_offset = current_file_position + write_file_data_handle_->buffer_pos;
  archive_index_header.packet_size = packet_size;
  archive_index_header.begin_timestamp_msec = begin_timestramp_msec;
  archive_index_header.end_timestamp_msec = end_timestramp_msec;
  archive_index_header.encryption_type = encryption_type;
  write_file_indexer_handle.archive_indexers_.push_back(archive_index_header);

  memcpy(write_file_indexer_handle.writebuffer + write_file_indexer_handle.buffer_pos, &archive_index_header, sizeof(ArchiveIndexHeader));
  write_file_indexer_handle.buffer_pos += sizeof(ArchiveIndexHeader);
  if (!platform_file_.FileWrite(file_handle, write_file_indexer_handle.writebuffer, g_dio_indexer_size_)) {
    SPDLOG_ERROR("[ArchiveL] PlatformFile::FileWrite Fail  [{}], profile[{}]", session_id_.c_str(), profile_.ToString().c_str());
    write_file_indexer_handle.buffer_pos = current_file_position;
    return false;
  }

  if (!platform_file_.SetDatFilePosition(file_handle, current_file_position)) {
    SPDLOG_ERROR("[ArchiveL] PlatformFile::SetDatFilePosition Fail  [{}], profile[{}]", session_id_.c_str(), profile_.ToString().c_str());
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
  if (remain__space > buffer_data->dataSize()) {
    memcpy(write_file_data_handle_->writebuffer + write_file_data_handle_->buffer_pos, buffer_data->data(), buffer_data->dataSize());

    write_file_data_handle_->buffer_pos += buffer_data->dataSize();

  } else {
    memcpy(write_file_data_handle_->writebuffer + write_file_data_handle_->buffer_pos, buffer_data->data(), remain__space);

    if (!WriteDisk(file_handle, write_file_data_handle_->writebuffer, write_file_data_handle_->dio_buffer_size_)) {
      return false;
    }
    {
      std::unique_lock<std::mutex> lock(mmy_buf_mtx_);
      if (memory_buffer_frames.size() > buffer_size_) {

        auto buffer = *memory_buffer_frames.begin();
        for (auto data : memory_buffer_frames) {
          if (auto gop_buffers = std::dynamic_pointer_cast<ArchiveChunkBuffer>(data)) {
            gop_buffers->Clear();
          }
        }
        memory_buffer_frames.clear();
        memory_buffer_frames.push_back(buffer);
      }
    }
    int remain_size = buffer_data->dataSize() - remain__space;
    CheckBufferSize(remain_size);

    memcpy(write_file_data_handle_->writebuffer, buffer_data->data() + remain__space, remain_size);
    write_file_data_handle_->buffer_pos = remain_size;
    if (remain_size > write_file_data_handle_->dio_buffer_size_)
      FireBuffer();
  }
  return true;
}

bool IOWorker::WriteDisk(FileHandle handle, char* buffer, int write_size) {
  if (!platform_file_.FileWrite(handle, write_file_data_handle_->writebuffer, write_size)) {
    SPDLOG_ERROR("[ArchiveL] PlatformFile::FileWrite Fail");
    WriteDiskFail();
    return false;
  }

  bitrate_monitor_.AddData(write_size);
  // success
  WriteDiskSuccess();
  file_index_last_pos_ = write_file_indexer_handle.buffer_pos - sizeof(ArchiveIndexHeader);
  write_file_data_handle_->buffer_pos = 0;
  return true;
}

bool IOWorker::WriteFileChange(std::string filetime, unsigned long long timestamp) {

  FileClose();
  write_file_name_ = FileSearcher::GetDatFileName(save_driver_ / save_path_, session_id_, filetime, profile_, timestamp);

  file_handle = platform_file_.FileDirectOpen(write_file_name_);
  if (file_handle == InvalidHandle) {
    SPDLOG_ERROR("[ArchiveL] create file fail[{}]", write_file_name_.c_str());
    return false;
  }
      
  SPDLOG_INFO("[ArchiveL] create file : {}", write_file_name_.c_str());
  return InitBuffer();
}

bool IOWorker::FileClose() {
  if (file_handle != InvalidHandle) {

    if (!WriteRemain(file_handle, write_file_data_handle_->buffer_pos)) {
      file_handle = InvalidHandle;
      return false;
    }

    SPDLOG_INFO("[ArchiveL] close file [{}]", write_file_name_.c_str());
    if (!platform_file_.CloseFile(file_handle)) {
      printf("[ERROR] %s Failed to close a file. (path: %s)\n", __FUNCTION__, write_file_name_.c_str());
      file_handle = InvalidHandle;
      return false;
    }
    file_handle = InvalidHandle;
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

  if (!platform_file_.FileWrite(file_handle, write_file_indexer_handle.writebuffer, g_dio_indexer_size_)) {
    SPDLOG_ERROR("[ArchiveL] PlatformFile::FileWrite Fail [{}]", write_file_name_.c_str());
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
    if (!platform_file_.FileWrite(file_handle, write_file_data_handle_->writebuffer + memory_position, write_file_data_handle_->dio_buffer_size_)) {
      SPDLOG_ERROR("[ArchiveL] PlatformFile::FileWrite Fail");
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
    write_file_data_handle_->buffer_pos = 0;
    SPDLOG_ERROR("PlatformFile::FileWrite Fail ");
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
        SPDLOG_ERROR("[ArchiveL] Failed to allocate memory");
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
  std::string filetime = FileSearcher::GetFolderFrTime(timestamp / 1000);
  if (file_random_number_ == 0) {
    file_random_number_ = GetRandomFileNumber();
  }
  if (file_handle == InvalidHandle || file_handle == 0 || change_file_ == true ||
      (timestamp - file_begin_timestamp_) / 1000 > (SAVED_TIME + file_random_number_) || filetime.compare(cur_filetime) != 0) {
    cur_filetime = filetime;
    file_begin_timestamp_ = timestamp;
    change_file_ = false;
    file_random_number_ = GetRandomFileNumber();

    if (!WriteFileChange(cur_filetime, timestamp))
      return false;
  }
  return true;
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
  std::shared_ptr<std::vector<FrameWriteIndexData>> frameinfos = index_buffer_info_.GetFrameInfos();
  CallbackWirteStatusIWTAW(session_id_, profile_, true, frameinfos);
  index_buffer_info_.ResetInfo();
  write_time_ = std::chrono::high_resolution_clock::now();
}

void IOWorker::WriteDiskFail() {
  write_file_indexer_handle.buffer_pos = file_index_last_pos_;

  // Move the file index position to the previous memory block position.
  ArchiveUtil::SetZeroAfter(write_file_indexer_handle.writebuffer, g_dio_indexer_size_, write_file_indexer_handle.buffer_pos);

  long long current_file_position = platform_file_.GetDatFilePosition(file_handle);
  if (!platform_file_.SetDatFilePosition(file_handle, 0)) {
    SPDLOG_ERROR("[ArchiveL] PlatformFile::SetDatFilePosition Fail ");
  }

  if (!platform_file_.FileWrite(file_handle, write_file_indexer_handle.writebuffer, g_dio_indexer_size_)) {
    SPDLOG_ERROR("[ArchiveL] PlatformFile::FileWrite Fail ");
    write_file_indexer_handle.buffer_pos = current_file_position;
  }

  if (!platform_file_.SetDatFilePosition(file_handle, current_file_position)) {
    SPDLOG_ERROR("[ArchiveL] PlatformFile::SetDatFilePosition Fail");
  }

  // fail
  write_file_data_handle_->buffer_pos = 0;
  std::shared_ptr<std::vector<FrameWriteIndexData>> frameinfos = index_buffer_info_.GetFrameInfos();
  CallbackWirteStatusIWTAW(session_id_, profile_, false, frameinfos);
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
          SPDLOG_INFO("[ArchiveL] Deleted empty folder[{}]", folderPath.c_str());
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
      SPDLOG_INFO("[ArchiveL] File deleted [{}]", filePath.c_str());
      return true;
    } else {
      SPDLOG_INFO("[ArchiveL] Failed to delete file [{}]", filePath.c_str());
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

  media_buffer_mtx_.lock_shared();
  int buff_size = media_buffers.size();
  media_buffer_mtx_.unlock_shared();

  while (buff_size > buffer_size_) {
    {
      std::unique_lock<std::shared_mutex> lock(media_buffer_mtx_);
      base_data = media_buffers.front();
      media_buffers.pop();
      buff_size = media_buffers.size();
    }

    if (auto gop_buffers = std::dynamic_pointer_cast<ArchiveChunkBuffer>(base_data)) {
      while (true) {
        std::shared_ptr<StreamBuffer> buffer = gop_buffers->PopQueue();
        if (buffer == nullptr)
          break;

        FrameWriteIndexData index_data = {buffer.get()->archive_type, buffer.get()->timestamp_msec};
        frameinfos->push_back(index_data);
      }
    }
  }

  if (frameinfos->size() > 0) {
    SPDLOG_ERROR("[ArchiveL] Buffer Over flow !!!!! session_id[{}], profile[{}]", session_id_.c_str(), profile_.ToString().c_str());
    CallbackWirteStatusIWTAW(session_id_, profile_, false, frameinfos);
    index_buffer_info_.Clear();
  }
}

void IOWorker::MemoryCheck() {
  media_buffer_mtx_.lock_shared();
  int inclease_size = 2;
  if (media_buffers.size() > buffer_size_ - 3) {
    int memory_percent = ArchiveUtil::PercentAvailableMemorySize();
    if (memory_percent < ARCHIVER_BUFFER_MEMORY_PERCENT) {
      buffer_size_ += inclease_size;
      SPDLOG_INFO("[ArchiveL] MemoryCheck Buffer buffer size up session_id[{}], profile[{}], buffer_size_[{}], memory_percent[{}]", session_id_.c_str(),
                   profile_.ToString().c_str(), buffer_size_, memory_percent);
    } else if (memory_percent > ARCHIVER_BUFFER_MEMORY_PERCENT + 10 && buffer_size_ > IOWORKER_QUEUE_MAX_COUNT + inclease_size) {
      buffer_size_ -= inclease_size;
      SPDLOG_INFO("[ArchiveL] MemoryCheck Buffer buffer size down session_id[{}], profile[{}], buffer_size_[{}], memory_percent[{}]", session_id_.c_str(),
                  profile_.ToString().c_str(), buffer_size_, memory_percent);
    }
  } else if (media_buffers.size() < inclease_size && buffer_size_ != IOWORKER_QUEUE_MAX_COUNT) {
    buffer_size_ = IOWORKER_QUEUE_MAX_COUNT;
    SPDLOG_INFO("[ArchiveL] MemoryCheck Buffer Reset session_id[{}], profile[{}], buffer_size_[{}]", session_id_.c_str(),
                profile_.ToString().c_str(), buffer_size_);
  }
  media_buffer_mtx_.unlock_shared();

  if (buffer_size_ > IOWORKER_QUEUE_MAX_COUNT && write_file_data_handle_->buffer_pos == 0 &&
      write_file_data_handle_->dio_buffer_size_ != dio_buffer_busy_size) {
    write_file_data_handle_->dio_buffer_size_ = dio_buffer_busy_size;
  } else if (buffer_size_ <= IOWORKER_QUEUE_MAX_COUNT && write_file_data_handle_->buffer_pos < dio_buffer_nomal_size &&
             write_file_data_handle_->dio_buffer_size_ != dio_buffer_nomal_size) {
    write_file_data_handle_->dio_buffer_size_ = dio_buffer_nomal_size;
  }
}

bool IOWorker::CloseSavedFile() {
  if (file_handle != InvalidHandle) {
    if (!platform_file_.CloseFile(file_handle)) {
      printf("[ERROR] %s Failed to close a file. (path: %s)\n", __FUNCTION__, write_file_name_.c_str());
      file_handle = InvalidHandle;
      return false;
    }
    file_handle = InvalidHandle;
    return false;
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
      if (auto v_buffer = std::dynamic_pointer_cast<VideoData>(buffer)) {
        v_buffer->archive_header.packet_size = buffer->packet_size;
      } else if (auto a_buffer = std::dynamic_pointer_cast<AudioData>(buffer)) {
        a_buffer->archive_header.packet_size = buffer->packet_size;
      } else if (auto m_buffer = std::dynamic_pointer_cast<MetaData>(buffer)) {
        m_buffer->archive_header.packet_size = buffer->packet_size;
      }

      total_size += buffer->buffer->dataSize();
    }
    media_datas->SetEncryptionType(encrytion_type);
    media_datas->SetChunkTotalSize(total_size);
  }

  return true;
}

void IOWorker::ProcessFlush() {
  std::shared_ptr<BaseBuffer> base_data = nullptr;

  media_buffer_mtx_.lock();
  while (true) {
    if (media_buffers.empty())
      break;

    media_buffers.pop();
  }
  media_buffer_mtx_.unlock();
  FileClose();
  index_buffer_info_.Clear();
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
      if (gop_buffers->GetChunkEndTime() + 200 > timestamp) {
        SPDLOG_INFO("[ArchiveL] IOWorker::GetMemoryFrames 1 : timestamp[{}], BeginTime[{}], EndTime[{}]", timestamp, gop_buffers->GetChunkBeginTime(),
                    gop_buffers->GetChunkEndTime());
        return gop_buffers;
      }
    }
  }
  return nullptr;
}
