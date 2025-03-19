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
 * @file    platform_file.h
 * @brief
 * @author    cd.kim@hanwha.com
 *
 * @date    2024.06.19
 * @version    1.0
 */ 



#pragma once
#include <iostream>
#include <spdlog/spdlog.h>

#include "archive_define.h"

class PlatformFile {
 public:
  PlatformFile();
  ~PlatformFile();

  FileHandle FileDirectOpen(std::string file_name);
  bool CloseFile(FileHandle& hFile);
  bool FileWrite(FileHandle& hFile, char* buffer, int write_size);
  long long GetDatFilePosition(FileHandle& file_handle);
  bool SetDatFilePosition(FileHandle& file_handle, long long offset);

private:
  std::string file_name_;
  std::mutex file_mtx_;
};