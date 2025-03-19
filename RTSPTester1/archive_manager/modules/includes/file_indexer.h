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
 * @file    file_indexer.h
 * @brief
 * @author    cd.kim@hanwha.com
 *
 * @date    2024.06.19
 * @version    1.0
 */

#pragma once
#include <iostream>
#include <fstream>
#include <filesystem>
#include <vector>
#include <map>
#include <optional>
#include <stdio.h>
#include <string.h>
#include "archive_define.h"

class FileIndexer {
 public:
  FileIndexer();
  ~FileIndexer();

  bool IncludeTime(unsigned int timestamp);
  bool ParseMainHeader(std::ifstream& file_handle_);
  std::shared_ptr<ArchiveIndexHeader> GetHeader(unsigned int timestamp);
  bool SetIndexPosition(unsigned long long timestamp_msec, ArchiveReadType archive_read_type);  // only video
  std::shared_ptr<ArchiveIndexHeader> GetIndexHeader();
  std::shared_ptr<ArchiveIndexHeader> GetPrevIndex();
  std::shared_ptr<ArchiveIndexHeader> GetNextIndex();

 private:

  std::vector<std::shared_ptr<ArchiveIndexHeader>> archive_index_headers_;
  int data_index_ = 0;
};
