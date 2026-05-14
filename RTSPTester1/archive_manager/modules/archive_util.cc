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
 * @file    archive_util.cc
 * @brief
 * @author    cd.kim@hanwha.com
 *
 * @date    2024.06.19
 * @version    1.0
 */

#include "archive_util.h"
#ifndef _WIN32
#include <sys/sysinfo.h>
#endif
#include "blaze_logger.h"
short ArchiveUtil::GetPageSize() {
#ifdef _WIN32
  SYSTEM_INFO si = {};
  GetSystemInfo(&si);
  return si.dwPageSize;
#else
  return sysconf(_SC_PAGESIZE);
#endif
}

void ArchiveUtil::SetZeroAfter(void* buffer, size_t size, size_t offset) {
  if (offset < size) {
    std::memset(static_cast<char*>(buffer) + offset, 0, size - offset);
  }
}


int ArchiveUtil::PercentAvailableMemorySize() {
  int percent = 80;
#ifdef _WIN32
  MEMORYSTATUSEX memoryStatus = {};
  memoryStatus.dwLength = sizeof(memoryStatus);
  if (GlobalMemoryStatusEx(&memoryStatus) && 0 != memoryStatus.ullTotalPhys) {
    //BLAZE_LOG_INFO("[ArchiveL] Total physical memory: {} MB", memoryStatus.ullTotalPhys / (1024 * 1024));
    //BLAZE_LOG_INFO("[ArchiveL] Used physical memory: {} MB", (memoryStatus.ullTotalPhys - memoryStatus.ullAvailPhys) / (1024 * 1024));
    //BLAZE_LOG_INFO("[ArchiveL] Free physical memory: {} MB", memoryStatus.ullAvailPhys / (1024 * 1024));

    long long totalsize = memoryStatus.ullTotalPhys;
    long long usedsize = memoryStatus.ullAvailPhys;
    percent = (float(memoryStatus.ullTotalPhys - memoryStatus.ullAvailPhys) / (float)(memoryStatus.ullTotalPhys) * 100);
  } else {
    BLAZE_LOG_ERROR("Error retrieving memory status.");
  }
#else
  struct sysinfo memInfo = {};
  if (sysinfo(&memInfo) != 0) {
    BLAZE_LOG_ERROR("sysinfo() failed");
    return 100;
  }

  long long totalPhysMem = memInfo.totalram;
  totalPhysMem *= memInfo.mem_unit;

  long long freePhysMem = memInfo.freeram;
  freePhysMem *= memInfo.mem_unit;

  if (0 != totalPhysMem) {
    percent = (float(totalPhysMem - freePhysMem) / (float)(totalPhysMem) * 100);
  } else {
    BLAZE_LOG_ERROR("Error retrieving memory status.");
  }
#endif
  return percent;
}

void ArchiveUtil::RemoveIfEmpty(const std::filesystem::path& path, const std::string& folder_type) {
  try {
    std::error_code ec;
    if (!std::filesystem::exists(path, ec)) {
      return;
    }
    
    if (std::filesystem::is_empty(path, ec)) {
      if (ec) {
        BLAZE_LOG_WARN("Failed to check if {} folder is empty: {} ({})", folder_type, path.string(), ec.message());
        return;
      }
      
      std::filesystem::remove(path, ec);
      if (ec) {
        BLAZE_LOG_WARN("Failed to remove empty {} folder: {} ({})", folder_type, path.string(), ec.message());
      } else {
        BLAZE_LOG_INFO("Removed empty {} folder: {}", folder_type, path.string());
      }
    }
  } catch (const std::filesystem::filesystem_error& e) {
    BLAZE_LOG_ERROR("Filesystem error in RemoveIfEmpty for {} folder {}: {}", folder_type, path.string(), e.what());
  } catch (const std::exception& e) {
    BLAZE_LOG_ERROR("Exception in RemoveIfEmpty for {} folder {}: {}", folder_type, path.string(), e.what());
  }
}