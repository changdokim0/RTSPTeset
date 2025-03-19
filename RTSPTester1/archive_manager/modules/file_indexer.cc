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
 * @file    file_indexer.cc
 * @brief
 * @author    cd.kim@hanwha.com
 *
 * @date    2024.06.19
 * @version    1.0
 */

#include "file_indexer.h"

FileIndexer::FileIndexer() {}

FileIndexer::~FileIndexer() {}


bool FileIndexer::ParseMainHeader(std::ifstream& file_handle_) {
  archive_index_headers_.clear();
  data_index_ = -1;

  if (!file_handle_.is_open()) {
    std::cerr << "Failed to open index file: " << std::endl;
    return false;
  }

  std::shared_ptr<char> fileData = std::shared_ptr<char>(new char[g_dio_indexer_size_], [](char* ptr) { delete[] ptr; });
  file_handle_.read(fileData.get(), g_dio_indexer_size_);
  std::streamsize bytesRead = file_handle_.gcount();
  if (bytesRead < g_dio_indexer_size_)
    return false;

  char Idx_c[5] = {0,};
  int archive_versoin = 0;
  char* p_pos = fileData.get();
  int read_size = 4;
  memcpy(Idx_c, p_pos, read_size);
  if (strcmp(Idx_c, "PFFT") != 0)  // File Verification..
    return false;

  p_pos += read_size;
  read_size = 2;
  memcpy(&archive_versoin, p_pos, read_size);
  p_pos += read_size;

  int headersize = sizeof(ArchiveIndexHeader);
  int beginpos = 6;
  // index_endpos = index total size - start position - writable size
  int index_endpos = g_dio_indexer_size_ - beginpos - headersize;
  for (int i = beginpos; i < index_endpos; i += headersize) {
    std::shared_ptr<ArchiveIndexHeader> archive_index_header = std::make_shared<ArchiveIndexHeader>();
    memcpy(archive_index_header.get(), p_pos, headersize);
    if (archive_index_header->type == kArchiveTypeNone)
      break;

    p_pos += headersize;
    archive_index_headers_.push_back(archive_index_header);
    data_index_++;
  }

  return true;
}

bool FileIndexer::IncludeTime(unsigned int timestamp) {
  for (auto index : archive_index_headers_) {
    if (index->begin_timestamp_msec < timestamp && index->end_timestamp_msec > timestamp)
      return true;
  }
  return false;
}

std::shared_ptr<ArchiveIndexHeader> FileIndexer::GetHeader(unsigned int timestamp) {
  if (archive_index_headers_.size() <= 0)
    return nullptr;

  std::shared_ptr<ArchiveIndexHeader> indexheader = nullptr;
  for (auto index : archive_index_headers_) {
    if (index->begin_timestamp_msec < timestamp && index->end_timestamp_msec > timestamp)
      indexheader = index;
  }

  return indexheader;
}

bool FileIndexer::SetIndexPosition(unsigned long long timestamp_msec, ArchiveReadType archive_read_type) {
  // Only video is specified.
  if (archive_index_headers_.size() <= 0) {
    data_index_ = -1;
    return false;
  }

  bool ret = false;
  int index = 0;
  data_index_ = 0;

  if (archive_read_type == kArchiveReadPrev && archive_index_headers_.begin()->get()->begin_timestamp_msec > timestamp_msec) {
    data_index_ = -1;
    return false;
  }
  for (index = 0; index < archive_index_headers_.size(); index++) {
    std::shared_ptr<ArchiveIndexHeader> index_header = archive_index_headers_[index];
    if (index_header->type == kArchiveTypeGopVideo) {
      ret = true;
      data_index_ = index;

      if (index_header->begin_timestamp_msec > timestamp_msec && index > 0) {
        if (archive_read_type == kArchiveReadPrev) {
          data_index_ = index - 1;
          break;
        }
      }

      if (index_header->end_timestamp_msec >= timestamp_msec) {
        break;
      }
    }
  }

  return ret;
}


std::shared_ptr<ArchiveIndexHeader> FileIndexer::GetIndexHeader() {
  if (archive_index_headers_.size() <= 0 || archive_index_headers_.size() <= data_index_)
    return nullptr;

  return archive_index_headers_[data_index_];
}

std::shared_ptr<ArchiveIndexHeader> FileIndexer::GetPrevIndex() {
  if (archive_index_headers_.size() <= 0 || data_index_ <= 0)
    return nullptr;

  return archive_index_headers_[--data_index_];
}

std::shared_ptr<ArchiveIndexHeader> FileIndexer::GetNextIndex() {
  if (archive_index_headers_.size() <= 0 || archive_index_headers_.size() <= data_index_ + 1)
    return nullptr;

  return archive_index_headers_[++data_index_];
}
