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
 * @file    reader_object.h
 * @brief
 * @author    cd.kim@hanwha.com
 *
 * @date    2024.06.19
 * @version    1.0
 */

#pragma once

class Archive_FileInfo {
 public:
  Archive_FileInfo(){};
  Archive_FileInfo(std::string file_name, std::string file_path, unsigned long long time_stamp)
      : file_name_(file_name), file_path_(file_path), time_stamp_(time_stamp){};
  Archive_FileInfo& operator=(const Archive_FileInfo& other) {
    this->file_name_ = other.file_name_;
    this->file_path_ = other.file_path_;
    this->time_stamp_ = other.time_stamp_;
    return *this;
  }
  std::string file_name_;
  std::string file_path_;
  unsigned long long time_stamp_ = 0;
};
