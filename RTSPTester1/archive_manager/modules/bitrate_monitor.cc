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
 * @file    bitrate_monitor.cc
 * @brief
 * @author    cd.kim@hanwha.com
 *
 * @date    2024.11.21
 * @version    1.0
 */

#include "bitrate_monitor.h"

BitrateMonitor::BitrateMonitor() : totalBytes_(0) {
  interval_ = std::chrono::seconds(300);
}

BitrateMonitor::BitrateMonitor(std::chrono::seconds interval) : interval_(interval), totalBytes_(0) {}

void BitrateMonitor::SetInterval(std::chrono::seconds interval) {
  interval_ = interval;
}

void BitrateMonitor::AddData(uint64_t bytes) {
  auto now = std::chrono::steady_clock::now();
  dataPoints_.emplace_back(now, bytes);
  totalBytes_ += bytes;

  // Remove data points that are outside the interval
  while (!dataPoints_.empty() && now - dataPoints_.front().first > interval_) {
    totalBytes_ -= dataPoints_.front().second;
    dataPoints_.pop_front();
  }
}

int BitrateMonitor::GetBitrate() const {
  if (dataPoints_.empty()) {
    return 0.0;
  }

  auto duration = std::chrono::duration_cast<std::chrono::seconds>(dataPoints_.back().first - dataPoints_.front().first).count();

  if (duration == 0) {
    return 0.0;
  }

  return (int)(static_cast<double>(totalBytes_ * 8) / duration);  // Bitrate in bits per second
}