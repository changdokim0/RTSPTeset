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
 * @file    archive_chunk_buffer.h
 * @brief
 * @author    cd.kim@hanwha.com
 *
 * @date    2024.06.19
 * @version    1.0
 */

#pragma once

#include <string>
#include <queue>
#include <list>
#include <mutex>
#include <memory>
#include <optional>
#include <algorithm>
#include <string.h>
#include <climits>

#include "archive_define.h"
#include "archive_object.h"

class ARCHIVE_MANAGER_API ArchiveChunkBuffer : public BaseBuffer {
 public :
  ArchiveChunkBuffer();
  ~ArchiveChunkBuffer();

  void PushQueue(std::shared_ptr<StreamBuffer> buffer);
  std::shared_ptr<StreamBuffer> PopQueue();
  void Clear();
  int GetCount();
  std::shared_ptr<char> GetHeadersMemory();
  unsigned long long GetChunkBeginTime();
  unsigned long long GetChunkEndTime();
  unsigned int GetChunkTotalSize();
  EncryptionType GetEncryptionType() { return encryption_type_; }
  void SetEncryptionType(EncryptionType type) { encryption_type_ = type; }
  bool IsIncludedTime(unsigned long long timestamp_msec);
  std::shared_ptr<std::vector<FrameWriteIndexData>> GetFrameInfos();
  int GetHeadersSize();
  void SetChunkTotalSize(unsigned int total_data_size) { total_data_size_ = total_data_size; }

  void TrimToLastGOP();

  std::list<std::shared_ptr<StreamBuffer>>::const_iterator begin() const { return chunk_block_buffers_.begin(); }
  std::list<std::shared_ptr<StreamBuffer>>::const_iterator end() const { return chunk_block_buffers_.end(); }

 private:
  bool end_flag_ = false;
  unsigned int total_data_size_ = 0;
  unsigned int total_header_size_ = 0;
  EncryptionType encryption_type_ = kEncryptionNone;
  unsigned long long begin_time_msec_ = ULLONG_MAX, end_time_msec_ = 0;
  std::mutex buffer_mtx_;
  std::list<std::shared_ptr<StreamBuffer>> chunk_block_buffers_;

  std::shared_ptr<char> buffer_headers_;
  int buffer_headers_size_ = 0;
};
