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
 * @file    file_write_index_info.h
 * @brief
 * @author    cd.kim@hanwha.com
 *
 * @date    2024.06.19
 * @version    1.0
 */

#pragma once
#include <vector>
#include <iostream>
#include <memory>
#include <mutex>
#include "archive_define.h"

class FileWriteIndexInfo {
 public:

  FileWriteIndexInfo();
  ~FileWriteIndexInfo();
  void AddFrameInfo(FrameWriteIndexData frameInfo);

  std::shared_ptr<std::vector<FrameWriteIndexData>> GetFrameInfos();
  void ResetInfo();
  void Clear();

 private:
  std::mutex queue_mtx_;
  std::vector<FrameWriteIndexData> frameInfos_;
};
