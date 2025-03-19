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
 * @file    data_file_handle.cc
 * @brief
 * @author    cd.kim@hanwha.com
 *
 * @date    2024.06.19
 * @version    1.0
 */

#pragma once

#ifdef _WIN32
#define NOMINMAX
#include <Windows.h>
#else
#endif

#include <iostream>
#include <cstring>

#include "archive_define.h"

class DataFileHandle {
 public:
  DataFileHandle(int dio_buffer_size, int dio_memory_size) {
    dio_buffer_size_ = dio_buffer_size;
#ifdef _WIN32
    writebuffer = (char*)VirtualAlloc(nullptr, dio_memory_size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (writebuffer == nullptr) {
      std::cerr << "Failed to allocate memory : " << GetLastError() << std::endl;
      writebuffer = nullptr;
    }
#else
    if (posix_memalign((void**)&writebuffer, g_dio_page_size_, dio_memory_size) != 0) {
      std::cout << "Failed to allocate memory ";
      writebuffer = nullptr;
    }
#endif
    memset(writebuffer, 0, dio_memory_size);
    real_buffer_size = dio_memory_size;
  }
  ~DataFileHandle() {
    if (writebuffer) {
#ifdef _WIN32
      VirtualFree(writebuffer, 0, MEM_RELEASE);
#else
      free(writebuffer);
#endif
      writebuffer = nullptr;
    }
  }

  int buffer_pos = 0;
  std::string file_name_;
  char* writebuffer = nullptr;
  int dio_buffer_size_ = 0;
  int real_buffer_size = 0;     // actual buffer size
};
