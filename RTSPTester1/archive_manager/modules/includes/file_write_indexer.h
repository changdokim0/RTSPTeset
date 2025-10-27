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
 * @file    file_write_indexer.h
 * @brief
 * @author    cd.kim@hanwha.com
 *
 * @date    2024.06.19
 * @version    1.0
 */

#pragma once

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#else
#endif

#include <iostream>
#include <vector>
#include <cstring>

#include "archive_define.h"

class FileWriteIndexer {
 public:
  FileWriteIndexer() {
#ifdef _WIN32
    writebuffer = (char*)VirtualAlloc(nullptr, g_dio_indexer_size_, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!writebuffer) {
      std::cerr << "Failed to allocate memory : " << GetLastError() << std::endl;
      writebuffer = nullptr;
      return;
    }
#else
    if (posix_memalign((void**)&writebuffer, g_dio_page_size_, g_dio_indexer_size_) != 0) {
      std::cout << "Failed to allocate memory ";
      writebuffer = nullptr;
      return;
    }
#endif
    memset(writebuffer, 0, g_dio_indexer_size_);
  }

  ~FileWriteIndexer() {
    if (writebuffer) {
#ifdef _WIN32
      VirtualFree(writebuffer, 0, MEM_RELEASE);
#else
      free(writebuffer);
#endif
      writebuffer = nullptr;
    }
  }

  void Reset() { 
    buffer_pos = 0;
    archive_indexers_.clear(); // 
  }


  std::vector<ArchiveIndexHeader> archive_indexers_;
  int buffer_pos = 0;
  char* writebuffer = nullptr;
};
