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
 * @file    archive_chunk_buffer.cc
 * @brief
 * @author    cd.kim@hanwha.com
 *
 * @date    2024.06.19
 * @version    1.0
 */

#include "archive_chunk_buffer.h"
#include <spdlog/spdlog.h>

ArchiveChunkBuffer::ArchiveChunkBuffer() {
  buffer_headers_size_ = 1024 * 100;
  buffer_headers_ = std::shared_ptr<char>(new char[buffer_headers_size_], [](char* ptr) { delete[] ptr; });
}

ArchiveChunkBuffer::~ArchiveChunkBuffer() {

}

void ArchiveChunkBuffer::PushQueue(std::shared_ptr<StreamBuffer> buffer) {
  std::lock_guard<std::mutex> lock(buffer_mtx_);
  chunk_block_buffers_.push_back(buffer);  
  if (buffer->archive_type == kArchiveTypeFrameVideo)
    end_time_msec_ = (((buffer->timestamp_msec) > (end_time_msec_)) ? (buffer->timestamp_msec) : (end_time_msec_));
  total_data_size_ += buffer->buffer->dataSize();
  if (buffer->archive_type == kArchiveTypeFrameVideo) {
    begin_time_msec_ = (((buffer->timestamp_msec) < (begin_time_msec_)) ? (buffer->timestamp_msec) : (begin_time_msec_));
    total_header_size_ += sizeof(VideoHeader);
  }
  else if (buffer->archive_type == kArchiveTypeAudio)
    total_header_size_ += sizeof(AudioHeader);
  else if (buffer->archive_type == kArchiveTypeMeta)
    total_header_size_ += sizeof(MetaHeader);
}

std::shared_ptr<StreamBuffer> ArchiveChunkBuffer::PopQueue() {
  std::lock_guard<std::mutex> lock(buffer_mtx_);
  if (chunk_block_buffers_.size() <= 0)
    return nullptr;

  std::shared_ptr<StreamBuffer> block_buffer = chunk_block_buffers_.front();
  chunk_block_buffers_.pop_front();

  return block_buffer;
}

int ArchiveChunkBuffer::GetInternalHeaderSize() {
  // Header : TotalHeaderFileSize + HeaderCount + [type + header]+ [type + header] + ...
  return total_header_size_ + (sizeof(int) * chunk_block_buffers_.size()) + (sizeof(int) * 2);
}

int ArchiveChunkBuffer::GetHeadersSize() {
  std::lock_guard<std::mutex> lock(buffer_mtx_);

  return GetInternalHeaderSize();
}

std::shared_ptr<char> ArchiveChunkBuffer::GetHeadersMemory() {
  std::lock_guard<std::mutex> lock(buffer_mtx_);
  total_data_size_ = 0;
  total_header_size_ = 0;
  for (auto buffer : chunk_block_buffers_) {
    total_data_size_ += buffer->buffer->dataSize();
    if (buffer->archive_type == kArchiveTypeFrameVideo)
      total_header_size_ += sizeof(VideoHeader);
    else if (buffer->archive_type == kArchiveTypeAudio)
      total_header_size_ += sizeof(AudioHeader);
    else if (buffer->archive_type == kArchiveTypeMeta)
      total_header_size_ += sizeof(MetaHeader);
  }

  // Header : TotalHeaderFileSize + HeaderCount + [type + header]+ [type + header] + ...
  int size = GetInternalHeaderSize(); 
  int headersize = 0;;
  int pos = 0;
  int item_count = chunk_block_buffers_.size();
  if (buffer_headers_size_ < size) {
    buffer_headers_size_ = size;
    buffer_headers_ = std::shared_ptr<char>(new char[buffer_headers_size_], [](char* ptr) { delete[] ptr; });
  }
  memcpy(buffer_headers_.get(), &size, sizeof(int));
  pos += sizeof(int);
  memcpy(buffer_headers_.get() + pos, &item_count, sizeof(int));
  pos += sizeof(int);
  for (auto buffer : chunk_block_buffers_) {
    if (auto video = std::dynamic_pointer_cast<VideoData>(buffer)) {
      headersize = sizeof(VideoHeader);
      memcpy(buffer_headers_.get() + pos, &(buffer->archive_type), sizeof(int));
      pos += sizeof(int);
      memcpy(buffer_headers_.get() + pos, &(video.get()->archive_header), headersize);
      pos += headersize;
    } else if (auto audio = std::dynamic_pointer_cast<AudioData>(buffer)) {
      headersize = sizeof(AudioHeader);
      memcpy(buffer_headers_.get() + pos, &(buffer->archive_type), sizeof(int));
      pos += sizeof(int);
      memcpy(buffer_headers_.get() + pos, &(audio.get()->archive_header), headersize);
      pos += headersize;
    } else if (auto meta = std::dynamic_pointer_cast<MetaData>(buffer)) {
      headersize = sizeof(MetaHeader);
      memcpy(buffer_headers_.get() + pos, &(buffer->archive_type), sizeof(int));
      pos += sizeof(int);
      memcpy(buffer_headers_.get() + pos, &(meta.get()->archive_header), headersize);
      pos += headersize;
    } else {
      SPDLOG_ERROR("Unknown Type Error[{}]", (int)buffer->archive_type);
    }
  }
  return buffer_headers_;
}

void ArchiveChunkBuffer::Clear() {
  std::lock_guard<std::mutex> lock(buffer_mtx_);
  begin_time_msec_ = ULLONG_MAX;
  end_time_msec_ = 0;
  chunk_block_buffers_.clear();
}

int ArchiveChunkBuffer::GetCount() {
  std::lock_guard<std::mutex> lock(buffer_mtx_);
  return chunk_block_buffers_.size();
}

unsigned long long ArchiveChunkBuffer::GetChunkBeginTime() {
  return begin_time_msec_;
}

unsigned long long ArchiveChunkBuffer::GetChunkEndTime() {
  return end_time_msec_ ;
}

unsigned int ArchiveChunkBuffer::GetChunkTotalSize() {
  std::lock_guard<std::mutex> lock(buffer_mtx_);
  // headers count + data size + count(int)
  return total_data_size_ + GetInternalHeaderSize() + sizeof(int);
}

bool ArchiveChunkBuffer::IsIncludedTime(unsigned long long timestamp_msec) {
  if (end_time_msec_ <= timestamp_msec && begin_time_msec_ <= timestamp_msec) {
    return true;
  } else {
    return false;
  }
}


std::shared_ptr<std::vector<FrameWriteIndexData>> ArchiveChunkBuffer::GetFrameInfos(std::string driver) {
  std::lock_guard<std::mutex> lock(buffer_mtx_);
  std::shared_ptr<std::vector<FrameWriteIndexData>> frameinfos = std::make_shared<std::vector<FrameWriteIndexData>>();
  for (auto frameinfo : chunk_block_buffers_) {
    FrameWriteIndexData frame_write_data;
    frame_write_data.archive_type = frameinfo.get()->archive_type;
    frame_write_data.packet_timestamp_msec = frameinfo.get()->timestamp_msec;
    frame_write_data.save_drive_ = driver;
    frameinfos->push_back(frame_write_data);
  }
  return frameinfos;
}

void ArchiveChunkBuffer::TrimToLastGOP() {
  std::lock_guard<std::mutex> lock(buffer_mtx_);
  if(chunk_block_buffers_.size() <= 1)
	return;

  std::list<std::shared_ptr<StreamBuffer>>::iterator last_iframe;
  auto it = chunk_block_buffers_.begin();
  for (; it != chunk_block_buffers_.end(); it++) {
    auto buffer = *it;
    if (buffer->archive_type == kArchiveTypeFrameVideo) {
      auto video = std::dynamic_pointer_cast<VideoData>(buffer);
      if (video != nullptr && video->archive_header.frameType == Pnx::Media::VideoFrameType::I_FRAME) {
        last_iframe = it;
      }
    }
  }

  chunk_block_buffers_.erase(chunk_block_buffers_.begin(), last_iframe);

  begin_time_msec_ = ULLONG_MAX;
  end_time_msec_ = 0;
  total_data_size_ = 0;
  total_header_size_ = 0;
  for (auto buffer : chunk_block_buffers_) {
    if (buffer->archive_type == kArchiveTypeFrameVideo)
      end_time_msec_ = (((buffer->timestamp_msec) > (end_time_msec_)) ? (buffer->timestamp_msec) : (end_time_msec_));
    total_data_size_ += buffer->buffer->dataSize();
    if (buffer->archive_type == kArchiveTypeFrameVideo) {
      begin_time_msec_ = (((buffer->timestamp_msec) < (begin_time_msec_)) ? (buffer->timestamp_msec) : (begin_time_msec_));
      total_header_size_ += sizeof(VideoHeader);
    }
    else if (buffer->archive_type == kArchiveTypeAudio)
      total_header_size_ += sizeof(AudioHeader);
    else if (buffer->archive_type == kArchiveTypeMeta)
      total_header_size_ += sizeof(MetaHeader);
  }

}