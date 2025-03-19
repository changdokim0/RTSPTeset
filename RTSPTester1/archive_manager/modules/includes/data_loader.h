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
 * @file    data_loader.h
 * @brief
 * @author    cd.kim@hanwha.com
 *
 * @date    2024.06.19
 * @version    1.0
 */

#pragma once
#include <iostream>
#include <fstream>
#include <optional>
#include <filesystem>
#include <string>

#include "archive_define.h"
#include "archive_object.h"

class DataLoader {
 public:
  DataLoader();
  ~DataLoader();
  std::optional<std::pair<ArchiveIndexHeader, ArchiveObject>> LoadData(ArchiveChunkReadType achive_gov_read_type);
  bool LoadData(std::ifstream& file_handle_, ArchiveIndexHeader& index_header);
  const std::shared_ptr<char> GetData();
  int GetDataSize() { return data_size_; }

 private:
  unsigned int data_size_ = 0, data_buffer_size_ = 0;
  std::shared_ptr<char> data_buffer_ = nullptr;

};
