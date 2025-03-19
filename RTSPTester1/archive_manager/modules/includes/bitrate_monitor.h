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
 * @file    bitrate_monitor.h
 * @brief
 * @author    cd.kim@hanwha.com
 *
 * @date    2024.11.21
 * @version    1.0
 */

#pragma once
#include <deque>
#include <chrono>
#include <cstdint>

class BitrateMonitor {
 public:
  BitrateMonitor();
  BitrateMonitor(std::chrono::seconds interval);
  void SetInterval(std::chrono::seconds interval);
  void AddData(uint64_t bytes);
  int GetBitrate() const;

 private:
  std::chrono::seconds interval_;
  std::deque<std::pair<std::chrono::time_point<std::chrono::steady_clock>, uint64_t>> dataPoints_;
  uint64_t totalBytes_;
};
