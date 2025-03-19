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
 * http://www.hanwhaVision.co.kr
 *********************************************************************************/

/**
 * @file	define_time.h
 * @brief
 * @author	hyo-jin.kim@hanwha.com
 *
 * @date	2024.07.28
 * @version	0.1
 */

#pragma once
#include <stdint.h>

struct PnxMediaTime {
  uint32_t sec = 0;
  uint32_t usec = 0;
  uint64_t ToMilliSeconds() const {
    uint64_t ret = (uint64_t(sec) * 1000) + (uint64_t(usec) / 1000);
    return ret;
  }
  uint64_t ToMicroSeconds() const {
    uint64_t ret = (uint64_t(sec) * 1000000) + (uint64_t(usec));
    return ret;
  }
  void FromMilliSeconds(uint64_t t) {
    sec = t / 1000;
    usec = (t % 1000) * 1000;
  };
  void FromMicroSeconds(uint64_t t) {
    sec = t / 1000000;
    usec = (t % 1000000) * 1000000;
  };
  PnxMediaTime() {
  sec = 0;
  usec = 0;
  }
  PnxMediaTime(uint32_t new_sec) {
    sec = new_sec;
    usec = 0;
  }
  PnxMediaTime(uint32_t new_sec, uint32_t new_usec) {
    sec = new_sec;
    usec = new_usec;
  }
#ifdef __PNX_TIME_SUPPORT_FLOAT__
  PnxMediaTime(float f) {
    sec = uint32_t(f);
    usec = float(f - sec) * 1000000;
  }
#endif
  PnxMediaTime operator=(uint32_t t) {
    sec = t;
    usec = 0;
    return *this;
  }
#ifdef __PNX_TIME_SUPPORT_FLOAT__
  PnxMediaTime operator=(float f) {
    PnxMediaTime ret;
    ret.sec = uint32_t(f);
    ret.usec = uint32_t(float(f - sec) * 1000000.0f);
    return ret;
  }
#endif
  PnxMediaTime operator=(const PnxMediaTime& a) {
    sec = a.sec;
    usec = a.usec;
    return *this;
  }
  PnxMediaTime operator+(const PnxMediaTime& a) {
    uint64_t tmp = this->ToMicroSeconds() + a.ToMicroSeconds();
    PnxMediaTime ret;
    ret.sec = _Sec(tmp);
    ret.usec = _Usec(tmp);
    return ret;
  }
  PnxMediaTime operator-(const PnxMediaTime& a) {
    uint64_t tmp = this->ToMicroSeconds() - a.ToMicroSeconds();
    PnxMediaTime ret;
    ret.sec = _Sec(tmp);
    ret.usec = _Usec(tmp);
    return ret;
  }
#ifdef __PNX_TIME_SUPPORT_FLOAT__
  PnxMediaTime operator+(float f) {
    uint64_t tmp = this->ToMicroSeconds() + PnxMediaTime(f).ToMicroSeconds();
    PnxMediaTime ret;
    ret.sec = _Sec(tmp);
    ret.usec = _Usec(tmp);
    return ret;
  }
  PnxMediaTime operator-(float f) {
    uint64_t tmp = this->ToMicroSeconds() - PnxMediaTime(f).ToMicroSeconds();
    PnxMediaTime ret;
    ret.sec = _Sec(tmp);
    ret.usec = _Usec(tmp);
    return ret;
  }
#endif
  bool operator<(const PnxMediaTime& a) { return this->ToMicroSeconds() < a.ToMicroSeconds();}
  bool operator>(const PnxMediaTime& a) { return this->ToMicroSeconds() > a.ToMicroSeconds();}
  bool operator==(const PnxMediaTime& a) { return this->ToMicroSeconds() == a.ToMicroSeconds();}
  bool operator!=(const PnxMediaTime& a) { return this->ToMicroSeconds() != a.ToMicroSeconds();}
  bool operator<=(const PnxMediaTime& a) { return this->ToMicroSeconds() <= a.ToMicroSeconds();}
  bool operator>=(const PnxMediaTime& a) { return this->ToMicroSeconds() >= a.ToMicroSeconds();}

  static PnxMediaTime Now() {
    PnxMediaTime ret;
    ret.FromMilliSeconds(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count());
    return ret;
  }

  private:
  uint32_t _Sec(uint64_t t) { return t / 1000000; }
  uint32_t _Usec(uint64_t t) { return t % 1000000; }
};
inline PnxMediaTime operator+(const PnxMediaTime& a, const PnxMediaTime& b) {
  uint64_t micro_ret = a.ToMicroSeconds() + b.ToMicroSeconds();
  PnxMediaTime ret = {(uint32_t)(micro_ret / 1000000), (uint32_t)(micro_ret % 1000000)};
  return ret;
}
inline PnxMediaTime operator-(const PnxMediaTime& a, const PnxMediaTime& b) {
  uint64_t micro_ret = a.ToMicroSeconds() - b.ToMicroSeconds();
  PnxMediaTime ret = {(uint32_t)(micro_ret / 1000000), (uint32_t)(micro_ret % 1000000)};
  return ret;
}


