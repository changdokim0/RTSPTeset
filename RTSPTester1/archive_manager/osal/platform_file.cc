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
        SPDLOG_ERROR("Failed to create directory[{}], error code[{}]", path.c_str(), strerror(errno));
        return InvalidHandle;
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
      SPDLOG_ERROR("ailed to open file[{}], error code[{}]", file_name.c_str(), GetLastError());

      return InvalidHandle;
    }
    return hFile;
#else
    int fd = open(file_name.c_str(), O_RDWR | O_CREAT | O_DIRECT, 0644);
    if (fd == -1) {
      SPDLOG_ERROR("ailed to open file[{}], error code[{}]", file_name.c_str(), strerror(errno));
      return InvalidHandle;
    }
    return fd;
#endif
  } catch (const std::exception& e) {
    //std::cout << "File[" << __FILE__ << "], Function[" << __FUNCTION__ << "], Line [" << __LINE__ << "], Message [" << file_name_ << "]: "
    //          << "Error code: " << e.what() << std::endl;

    SPDLOG_ERROR("ailed to open file[{}], error[{}]", file_name_.c_str(), e.what());
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
      printf("Error closing file: %s\n", strerror(errno));
    } else
      ret = true;
#endif
    hFile = InvalidHandle;
  }
  catch (const std::exception& e) {
    /*std::cout << "File[" << __FILE__ << "], Function[" << __FUNCTION__ << "], Line [" << __LINE__ << "], Message [" << file_name_ << "]: "
            << "Error code: " << e.what() << std::endl;*/
    SPDLOG_ERROR("Fail CloseFile[{}], error msg[{}]", file_name_.c_str(), e.what());
    return false;
  }
  return ret;
}

bool PlatformFile::FileWrite(FileHandle& hFile, char* buffer, int write_size) {
  std::lock_guard<std::mutex> lock(file_mtx_);

  const int max_attempts = 3;
  int attempt = 0;
  bool success = false;

#ifdef _WIN32
  DWORD bytesWritten = 0;
  while (attempt < max_attempts) {
    if (WriteFile(hFile, buffer, write_size, &bytesWritten, NULL)) {
      success = true;
      break;
    }
    ++attempt;
    int last_error = GetLastError();
    SPDLOG_ERROR("Failed to write to file [{}], attempt [{}], error msg[{}]", file_name_.c_str(), attempt, last_error);
  }
  if (!success) {
    hFile = InvalidHandle;
    return false;
  }
#else
  ssize_t written = -1;
  while (attempt < max_attempts) {
    written = ::write(hFile, buffer, write_size);
    if (written != -1) {
      success = true;
      break;
    }
    ++attempt;
    SPDLOG_ERROR("Failed to write to file [{}], attempt [{}], error msg[{}]", file_name_.c_str(), attempt, strerror(errno));
  }
  if (!success) {
    try {
      if (!close(hFile)) {
        throw std::runtime_error("Failed to close file handle.");
      }
    } catch (const std::exception& e) {
      std::cerr << "Error: " << e.what() << std::endl;
      return false;
    }
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
      SPDLOG_ERROR("Fail GetDatFilePosition file[{}], error code[{}]", file_name_.c_str(), GetLastError());
      return -1;
    }
#else
    newPosition = lseek(file_handle, 0, SEEK_CUR);
    if (newPosition == (off_t)-1) {
      SPDLOG_ERROR("Fail GetDatFilePosition file[{}], error code[{}]", file_name_.c_str(), strerror(errno));
      return -1;
    } 
#endif
  } catch (const std::exception& e) {
    SPDLOG_ERROR("Fail GetDatFilePosition file[{}], error msg[{}]", file_name_.c_str(), e.what());
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
      SPDLOG_ERROR("Fail SetDatFilePosition file[{}], error msg[{}]", file_name_.c_str(), GetLastError());
      return false;
    }else
      return true;
#else
    off_t newPosition = lseek(file_handle, offset, SEEK_SET);
    if (newPosition == (off_t)-1) {
      SPDLOG_ERROR("Fail SetDatFilePosition file[{}], error msg[{}]", file_name_.c_str(), strerror(errno));
      return false;
    }else
      return true;
#endif
  } catch (const std::exception& e) {

    SPDLOG_ERROR("Fail SetDatFilePosition file[{}], error msg[{}]", file_name_.c_str(), e.what());
    return false;
  }
}
