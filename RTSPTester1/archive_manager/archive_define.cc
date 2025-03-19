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
 * @file    archive_define.cc
 * @brief
 * @author    cd.kim@hanwha.com
 *
 * @date    2024.06.19
 * @version    1.0
 */

#define _CRT_SECURE_NO_WARNINGS
#define _CRT_NO_VA_START_VALIDATION
#include "archive_define.h"
#include <string>
#include <stdarg.h>

int g_dio_page_size_ = 4096;
int g_dio_buffer_size_ = g_dio_page_size_ * (244 * 1.5);
int g_dio_indexer_size_ = g_dio_page_size_ * 20;  // 45kbyte 

std::string GetStreamID(SessionID session_id, MediaProfile profile) {
  return session_id + "_" + profile.ToString();
}

//std::mutex log_mutex_;

//void set_color(LogLevel level) {
//  switch (level) {
//    case AL_DEBUG:
//      std::cout << "\033[90m";
//      break;  // gray
//    case AL_INFO:
//      std::cout << "\033[92m";
//      break;  // green
//    case AL_WARN:
//      std::cout << "\033[93m";
//      break;  // yellow
//    case AL_ERROR:
//      std::cout << "\033[91m";
//      break;  // red
//    case AL_FATAL:
//      std::cout << "\033[91m";
//      break;  // red
//  }
//}
//
//void reset_color() {
//  std::cout << "\033[0m";
//}
//void LOG_(LogLevel level, const char* file, int line, const std::string& message, ...) {
//  std::string fullPath = file;
//  size_t lastSlashPos = fullPath.find_last_of("/\\");
//  if (lastSlashPos != std::string::npos) {
//    fullPath = fullPath.substr(lastSlashPos + 1);
//  }
//  auto now = std::chrono::system_clock::now();
//  auto time_point = std::chrono::system_clock::to_time_t(now);
//  std::tm* local_time = std::localtime(&time_point);
//
//  std::string level_str;
//  switch (level) {
//    case AL_DEBUG: level_str = "[DEBUG]";  break;
//    case AL_INFO:  level_str = "[INFO]";   break;
//    case AL_WARN:  level_str = "[WARN]";   break;
//    case AL_ERROR: level_str = "[ERROR]";  break;
//    case AL_FATAL: level_str = "[FATAL]";  break;
//  }
//  char buffer[256];
//  va_list args;
//  va_start(args, message);
//  vsprintf(buffer, message.c_str(), args);
//  va_end(args);
//
//  log_mutex_.lock();
//  set_color(level);
//  std::cout << "[" << std::put_time(local_time, "%Y-%m-%d %H:%M:%S") << "] " << level_str << " " << fullPath << ":" << line << " - " << buffer << std::endl;
//  log_mutex_.unlock();
//  reset_color();
//
//}
