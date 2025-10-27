
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
 * @file    reader_object.cc
 * @brief
 * @author    cd.kim@hanwha.com
 *
 * @date    2024.06.19
 * @version    1.0
 */

#include "reader_object.h"

ReaderObject::ReaderObject(MediaProfile pfofile_type) : profile_type_(pfofile_type) {
  file_indexer_ = std::make_shared<FileIndexer>();
  data_loader_ = std::make_shared<DataLoader>();
}

ReaderObject::~ReaderObject() {
  try {
    if (file_handle_.is_open())
      file_handle_.close();
  } catch (const std::ios_base::failure& e) {
    fprintf(stdout, "[%s:%d] IOStreams Exception caught in ReaderObject destructor: %s \n", __func__, __LINE__, e.what());
  } catch (const std::exception& e) {
    fprintf(stdout, "[%s:%d] Exception caught in ReaderObject destructor: %s \n", __func__, __LINE__, e.what());
  } catch (...) {
    fprintf(stdout, "[%s:%d] Unknown exception caught in ReaderObject destructor\n", __func__, __LINE__);
  }
}

bool ReaderObject::isOpen() const {
  return file_handle_.is_open();
}

bool ReaderObject::FileOpen(std::string file_name) {
  if (file_handle_.is_open())
    file_handle_.close();

  file_handle_.open(file_name, std::ios::binary);
  if (file_handle_.is_open()) {
    cur_file_name_ = file_name;
    cur_file_size_ = std::filesystem::file_size(file_name);
    SPDLOG_INFO("[ArchiveL]########### Read File Open [{}]", file_name.c_str());
    return true;
  } else {
    cur_file_name_ = "";
    cur_file_size_ = 0;
    SPDLOG_ERROR("[ArchiveL]########### Read File Open Fail [{}]", file_name.c_str());
    return false;
  }
}

void ReaderObject::SetInfo( std::string session_id) {
  session_id_ = session_id;
}

void ReaderObject::SetFiles(std::shared_ptr<std::deque<Archive_FileInfo>> file_list) {
  if (file_list_)
    file_list_->clear();
  file_list_ = file_list;
}

bool ReaderObject::ParseMainHeader() {
  if (!isOpen())
    return false;

  return file_indexer_->ParseMainHeader(file_handle_);
}

std::shared_ptr<FileIndexer> ReaderObject::GetIndexer() {
  return file_indexer_;
}

std::shared_ptr<DataLoader> ReaderObject::GetDataLoader(){
  return data_loader_;
}

bool ReaderObject::LoadMedia(ArchiveIndexHeader& index_header) {
  return data_loader_->LoadData(file_handle_, index_header);
}

bool ReaderObject::LoadDataByIndex(unsigned long long time_stamp_msec, ArchiveReadType archive_read_type) {
  
  file_indexer_->SetIndexPosition(time_stamp_msec, archive_read_type);
  read_request_time_ = time_stamp_msec;

  std::shared_ptr<ArchiveIndexHeader> index = file_indexer_->GetIndexHeader();
  if (index == nullptr) {
    SPDLOG_ERROR("[ArchiveL]GetIndexHeader timestamp error");
    return false;
  }
  bool ret = false;
  while (index != nullptr) {
    if (data_loader_->LoadData(file_handle_, *index)) {
      if (index->type == kArchiveTypeGopVideo) {
        ret = reader_stream_chunk.LoadFrames(data_loader_->GetData(), data_loader_->GetDataSize(), index->encryption_type);
        if (ret)
          ret = ResetToCurrentTime(archive_read_type);

        if (ret)
          break;
      }
    }

    if (archive_read_type == kArchiveReadNext)
      index = file_indexer_->GetNextIndex();
    else
      index = file_indexer_->GetPrevIndex();
  }

  if (!ret) {
    SPDLOG_ERROR("[ArchiveL] LoadData error");
    return false;
  }

  return true;
}

std::shared_ptr<ArchiveChunkBuffer> ReaderObject::GetStreamChunk(ArchiveChunkReadType GovReadType) {
  std::shared_ptr<ArchiveChunkBuffer> chunk_buffer = reader_stream_chunk.GetStreamChunk(read_frame_position_msec_, GovReadType);
  return chunk_buffer;
}

std::shared_ptr<ArchiveChunkBuffer> ReaderObject::GetStreamGop(ArchiveChunkReadType GovReadType) {
  std::shared_ptr<ArchiveChunkBuffer> chunk_buffer = reader_stream_chunk.GetStreamGop(read_frame_position_msec_, GovReadType);
  return chunk_buffer;
}

std::shared_ptr<BaseBuffer> ReaderObject::GetPrevData(ArchiveType get_video_type) {
  std::shared_ptr<BaseBuffer> buffer = nullptr;
  std::shared_ptr<ArchiveIndexHeader> header = file_indexer_->GetPrevIndex();
  if (header != nullptr) {
    if (!data_loader_->LoadData(file_handle_, *header)) {
      return nullptr;
    }

    if (header->type == kArchiveTypeGopVideo || header->type == kArchiveTypeNone) {
      reader_stream_chunk.LoadFrames(data_loader_->GetData(), data_loader_->GetDataSize(), header->encryption_type);
      std::shared_ptr<ArchiveChunkBuffer> chunk_buffer = reader_stream_chunk.GetStreamChunk(read_frame_position_msec_, kArchiveChunkReadFull);
      //read_frame_position_msec_ = read_request_time_;
      return chunk_buffer;
    }
  }

  return nullptr;
}

bool ReaderObject::ResetToCurrentTime(ArchiveReadType archive_read_type) {
  unsigned long long read_request_time = read_request_time_;
  bool ret = reader_stream_chunk.SeekToTime(read_request_time, archive_read_type);
  if (ret)
    read_frame_position_msec_ = read_request_time;
  return ret;
}

std::optional<std::vector<std::shared_ptr<StreamBuffer>>> ReaderObject::GetNextData(ArchiveType get_video_type) {
  std::vector<std::shared_ptr<StreamBuffer>> buffers;

  std::shared_ptr<StreamBuffer> buffer = nullptr;
  if (get_video_type == kArchiveTypeFrameVideo || get_video_type == kArchiveTypeNone) {
    while (true) {
      buffer = reader_stream_chunk.GetForwardData();
      if (buffer != nullptr) {
        if (buffer->archive_type == kArchiveTypeFrameVideo)
          read_frame_position_msec_ = buffer->timestamp_msec;
        buffers.push_back(buffer);
      }

      auto current_data = reader_stream_chunk.GetCurrentData();
      if (current_data == nullptr || current_data->archive_type == kArchiveTypeFrameVideo)
        break;
    }
  }
  if (buffers.size() > 0)
    return buffers;

  std::shared_ptr<ArchiveIndexHeader> header = file_indexer_->GetNextIndex();
  if (header != nullptr) {
    if (!data_loader_->LoadData(file_handle_, *header)) {
      return std::nullopt;
    }

    if (header->type == kArchiveTypeGopVideo) {
      reader_stream_chunk.LoadFrames(data_loader_->GetData(), data_loader_->GetDataSize(), header->encryption_type);
      while (true) {
        buffer = reader_stream_chunk.GetForwardData();
        if (buffer != nullptr) {
          if (buffer->archive_type == kArchiveTypeFrameVideo)
            read_frame_position_msec_ = buffer->timestamp_msec;
          buffers.push_back(buffer);
        }
        auto current_data = reader_stream_chunk.GetCurrentData();
        if (current_data == nullptr || current_data->archive_type == kArchiveTypeFrameVideo)
          break;
      }
    } 
  }

  if (buffers.size() > 0)
    return buffers;
  else
    return std::nullopt;
}



std::optional<Archive_FileInfo> ReaderObject::GetPrevFile() {
  if (file_list_ == nullptr || file_list_->size() <= 0)
    return std::nullopt;

  Archive_FileInfo file_info = file_list_->back();
  file_list_->pop_back();
  opened_file_timestamp_ = file_info.time_stamp_;
  return file_info;
}

std::optional<Archive_FileInfo> ReaderObject::GetNextFile() {
  if (file_list_ == nullptr || file_list_->size() <= 0)
    return std::nullopt;

  Archive_FileInfo file_info = file_list_->front();
  file_list_->pop_front();
  opened_file_timestamp_ = file_info.time_stamp_;
  return file_info;
}

unsigned long long ReaderObject::GetChunkEndTime() {
  return read_frame_position_msec_ + (int)reader_stream_chunk.GetFrameDuration();
}

bool ReaderObject::IsInGOP(const PnxMediaTime& time) {
  return reader_stream_chunk.IsInGOP(time);
}

void ReaderObject::ClearFileList() {
  file_list_->clear();
}