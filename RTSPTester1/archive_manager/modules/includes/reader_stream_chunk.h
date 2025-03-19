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
 * @file    reader_gov_frames.h
 * @brief
 * @author    cd.kim@hanwha.com
 *
 * @date    2024.06.19
 * @version    1.0
 */ 

#pragma once
#include <memory>
#include <vector>
#include <iomanip>
#include <string.h>
#include <spdlog/spdlog.h>

#include "archive_define.h"
#include "archive_object.h"
#include "archive_chunk_buffer.h"
#include "stream_cryptor.h"

class ReaderStreamChunk {
 public: 
  ReaderStreamChunk();
  ~ReaderStreamChunk();

  bool LoadFrames(std::shared_ptr<char> data, int data_size, EncryptionType encryption_type);
  std::shared_ptr<StreamBuffer> GetForwardData();
  std::shared_ptr<StreamBuffer> GetCurrentData();
  std::shared_ptr<ArchiveChunkBuffer> GetStreamGop(unsigned long long& timestamp_msec, ArchiveChunkReadType GovReadType);
  std::shared_ptr<ArchiveChunkBuffer> GetStreamChunk(unsigned long long& timestamp_msec, ArchiveChunkReadType GovReadType);
  bool SeekToTime(unsigned long long& t_msec, ArchiveReadType archive_read_type);
  bool IsInGOP(const PnxMediaTime& time);

 private:
  StreamCryptor stream_crytor_;
  std::vector<std::shared_ptr<StreamBuffer>> datas_;
  int data_index_ = 0;

};
