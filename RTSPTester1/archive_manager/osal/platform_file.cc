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
 * @file    platform_file.cc
 * @brief
 * @author    cd.kim@hanwha.com
 *
 * @date    2024.06.19
 * @version    1.0
 */


#include "platform_file.h"
#include <filesystem>
#include <string.h>
#include "blaze_logger.h"
#ifdef _WIN32
#include <Windows.h>
#else
#include <unistd.h>
#endif

PlatformFile::PlatformFile() {}

PlatformFile::~PlatformFile() {}

FileHandle PlatformFile::FileDirectOpen(std::string file_name) {
  std::lock_guard<std::mutex> lock(file_mtx_);
  try {
    std::filesystem::path p(file_name);
    std::string path = p.parent_path().string();
    if (!std::filesystem::exists(path)) {
      if (!std::filesystem::create_directories(path)) {
        if (!std::filesystem::exists(path)) {
          BLAZE_LOG_ERROR("Failed to create directory[{}], error code[{}]", path.c_str(), strerror(errno));
          return InvalidHandle;
        }
      }
    }
    file_name_ = file_name;
#ifdef _WIN32
    std::wstring wcs2 = std::wstring().assign(file_name.begin(), file_name.end());
    HANDLE hFile = CreateFileW(wcs2.c_str(),
                              GENERIC_WRITE | GENERIC_READ,  //GENERIC_WRITE GENERIC_READ
                              FILE_SHARE_READ , NULL,
                              OPEN_ALWAYS,  //OPEN_EXISTING, CREATE_ALWAYS
                              FILE_FLAG_NO_BUFFERING, NULL);

    if (hFile == INVALID_HANDLE_VALUE) {
      BLAZE_LOG_ERROR("Failed to open file[{}], error code[{}]", file_name.c_str(), GetLastError());

      return InvalidHandle;
    }
    return hFile;
#else
    int fd = open(file_name.c_str(), O_RDWR | O_CREAT | O_DIRECT, 0644);
    if (fd == -1) {
      BLAZE_LOG_ERROR("Failed to open file[{}], error code[{}]", file_name.c_str(), strerror(errno));
      return InvalidHandle;
    }
    return fd;
#endif
  } catch (const std::exception& e) {
    //std::cout << "File[" << __FILE__ << "], Function[" << __FUNCTION__ << "], Line [" << __LINE__ << "], Message [" << file_name_ << "]: "
    //          << "Error code: " << e.what() << std::endl;

    BLAZE_LOG_ERROR("Failed to open file[{}], error[{}]", file_name_.c_str(), e.what());
    return InvalidHandle;
  }
}

bool PlatformFile::CloseFile(FileHandle& hFile) {
  std::lock_guard<std::mutex> lock(file_mtx_);
  bool ret = true;
  try {
#ifdef _WIN32
    ret = CloseHandle(hFile);
#else
    if (close(hFile) == -1) {
      ret = false;
      BLAZE_LOG_ERROR("Error closing file[{}]: {}", file_name_.c_str(), strerror(errno));
    } else
      ret = true;
#endif
    hFile = InvalidHandle;
  }
  catch (const std::exception& e) {
    /*std::cout << "File[" << __FILE__ << "], Function[" << __FUNCTION__ << "], Line [" << __LINE__ << "], Message [" << file_name_ << "]: "
            << "Error code: " << e.what() << std::endl;*/
    BLAZE_LOG_ERROR("Fail CloseFile[{}], error msg[{}]", file_name_.c_str(), e.what());
    return false;
  }
  return ret;
}

bool PlatformFile::FileWrite(FileHandle& hFile, char* buffer, int write_size) {
  std::lock_guard<std::mutex> lock(file_mtx_);
  if (write_size <= 0) {
    BLAZE_LOG_ERROR("Invalid write_size [{}] for file[{}]", write_size, file_name_.c_str());
    return false;
  }

  const int max_attempts = 3;
  int error_count = 0;
  int total_written = 0;

#ifdef _WIN32
  while (total_written < write_size && error_count < max_attempts) {
    DWORD bytesWritten = 0;
    int remaining = write_size - total_written;
    if (remaining <= 0 || remaining > write_size) {
      BLAZE_LOG_ERROR("Overflow/underflow detected: remaining [{}], write_size [{}], total_written [{}]", remaining, write_size, total_written);
      break;
    }

    if (WriteFile(hFile, buffer + total_written, remaining, &bytesWritten, NULL)) {
      if (bytesWritten > 0) {
        total_written += bytesWritten;
      } else {
        // WriteFile succeeded but wrote 0 bytes - treat as error
        ++error_count;
        BLAZE_LOG_WARN("WriteFile returned 0 bytes for file[{}], error count[{}/{}], written[{}/{}]",
                    file_name_.c_str(), error_count, max_attempts, total_written, write_size);
      }
    } else {
      ++error_count;
      BLAZE_LOG_ERROR("WriteFile failed for file[{}], error code[{}], error count[{}/{}], written[{}/{}]",
                   file_name_.c_str(), GetLastError(), error_count, max_attempts, total_written, write_size);
    }
  }
  
  if (total_written < write_size) {
    BLAZE_LOG_ERROR("Failed to write complete data to file[{}], written[{}/{}] bytes, errors[{}]",
                 file_name_.c_str(), total_written, write_size, error_count);
    return false;
  }
#else
  while (total_written < write_size && error_count < max_attempts) {
    int remaining = write_size - total_written;
    if (remaining <= 0 || remaining > write_size) {
      BLAZE_LOG_ERROR("Overflow/underflow detected: remaining [{}], write_size [{}], total_written [{}]", remaining, write_size, total_written);
      break;
    }

    ssize_t written = ::write(hFile, buffer + total_written, remaining);
    
    if (written > 0) {
      total_written += written;
    } else if (written == 0) {
      ++error_count;
      BLAZE_LOG_WARN("write() returned 0 bytes for file[{}], error count[{}/{}], written[{}/{}]",
                  file_name_.c_str(), error_count, max_attempts, total_written, write_size);
    } else {
      ++error_count;
      BLAZE_LOG_ERROR("write() failed for file[{}], error[{}], error count[{}/{}], written[{}/{}]",
                   file_name_.c_str(), strerror(errno), error_count, max_attempts, total_written, write_size);
    }
  }
  
  if (total_written < write_size) {
    BLAZE_LOG_ERROR("Failed to write complete data to file[{}], written[{}/{}] bytes, errors[{}]",
                 file_name_.c_str(), total_written, write_size, error_count);
    return false;
  }
#endif
  return true;
}

long long PlatformFile::GetDatFilePosition(FileHandle& file_handle) {
  std::lock_guard<std::mutex> lock(file_mtx_);
  long long newPosition = 0;
  try {
#ifdef _WIN32
    newPosition = SetFilePointer(file_handle, 0, NULL, FILE_CURRENT);
    if (newPosition == INVALID_SET_FILE_POINTER && GetLastError() != NO_ERROR) {
      BLAZE_LOG_ERROR("Fail GetDatFilePosition file[{}], error code[{}]", file_name_.c_str(), GetLastError());
      return -1;
    }
#else
    newPosition = lseek(file_handle, 0, SEEK_CUR);
    if (newPosition == (off_t)-1) {
      BLAZE_LOG_ERROR("Fail GetDatFilePosition file[{}], error code[{}]", file_name_.c_str(), strerror(errno));
      return -1;
    } 
#endif
  } catch (const std::exception& e) {
    BLAZE_LOG_ERROR("Fail GetDatFilePosition file[{}], error msg[{}]", file_name_.c_str(), e.what());
    return -1;
  }
  return newPosition;
}

bool PlatformFile::SetDatFilePosition(FileHandle& file_handle, long long offset){
  std::lock_guard<std::mutex> lock(file_mtx_);
  try {
#ifdef _WIN32
    long long newPosition = SetFilePointer(file_handle, offset, NULL, FILE_BEGIN);
    if (newPosition == INVALID_SET_FILE_POINTER && GetLastError() != NO_ERROR) {
      BLAZE_LOG_ERROR("Fail SetDatFilePosition file[{}], error msg[{}]", file_name_.c_str(), GetLastError());
      return false;
    }else
      return true;
#else
    off_t newPosition = lseek(file_handle, offset, SEEK_SET);
    if (newPosition == (off_t)-1) {
      BLAZE_LOG_ERROR("Fail SetDatFilePosition file[{}], error msg[{}]", file_name_.c_str(), strerror(errno));
      return false;
    }else
      return true;
#endif
  } catch (const std::exception& e) {

    BLAZE_LOG_ERROR("Fail SetDatFilePosition file[{}], error msg[{}]", file_name_.c_str(), e.what());
    return false;
  }
}
