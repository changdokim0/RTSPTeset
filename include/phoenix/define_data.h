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
 * @file	define_data.h
 * @brief
 * @author	hyo-jin.kim@hanwha.com
 *
 * @date	2024.08.02
 * @version	0.1
 */

#pragma once
#include <filesystem>

#include "define_time.h"
#include "Pnx++/Media/MediaDefine.h"
#include "Pnx++/Media/MediaSourceFrame.h"

struct MediaProfile {
 public:
  enum EProfile { kPrimary = 0, kSecondary, kAuto, kUnknown };

  MediaProfile(){};
  MediaProfile(EProfile profile) : profile_(profile){};
  MediaProfile operator=(MediaProfile::EProfile profile) {
    profile_ = profile;
    return *this;
  }
  MediaProfile operator=(const MediaProfile& profile) {
    profile_ = profile.profile_;
    return *this;
  }
  bool operator==(const MediaProfile::EProfile profile) const { return profile == profile_; };
  bool operator!=(const MediaProfile::EProfile profile) const { return profile != profile_; };
  bool operator>(const MediaProfile profile) const { return profile.ToInt() > ToInt(); };
  bool operator<(const MediaProfile profile) const { return profile.ToInt() < ToInt(); };
  bool operator==(const MediaProfile profile) const { return profile.profile_ == profile_; };
  bool operator!=(const MediaProfile profile) const { return profile.profile_ != profile_; };
  EProfile ToEProfile() const { return profile_; };
  std::string ToString(void) { return ToString(profile_); };
  static const std::string ToString(MediaProfile::EProfile profile) {
    if (profile == kPrimary) {
      return "primary";
    } else if (profile == kSecondary) {
      return "secondary";
    } else {
      return "unknown";
    }
  };
  int ToInt() const {
    return (int)profile_;
  }
  void SetFlexible(bool is_flexible) {
    is_flexible_ = is_flexible;
  }
  bool IsFlexible(void) const {
    return profile_ == kAuto ? true : is_flexible_;
  }
 private:
  EProfile profile_ = kUnknown;
  bool is_flexible_ = false;
};
template <>
struct std::hash<MediaProfile> {
  std::size_t operator()(const MediaProfile& a) const {
    using std::hash;
    return hash<int>()(a.ToInt());
  }
};

struct PhoenixPlayInfo {
  unsigned char cseq = 0;
  bool c_flag = false;
  bool e_flag = false;
  bool d_flag = false;
  bool t_flag = false;
  bool p_flag = false;
};

typedef std::string ChannelUUID;
typedef std::string ServerUUID;
typedef std::string ClientSessionUUID;

struct PnxMedia {
  PnxMedia() : first_address_(this){};  // Temporary just for debuging. TODO (hyo-jin.kim) : remove later.
  virtual ~PnxMedia(){};

 private:
  void* first_address_;
};

struct PnxMediaInfo : virtual PnxMedia {
  Pnx::Media::MediaType type = Pnx::Media::MediaType::Unknown;
  ChannelUUID channel_uuid;
  ServerUUID server_uuid;
  MediaProfile profile = MediaProfile::kUnknown;

 public:
  PnxMediaInfo operator=(const PnxMediaInfo& a) {
    _CopyMediaInfo(a);
    return *this;
  }

 protected:
  void _CopyMediaInfo(const PnxMediaInfo& a) {
    this->type = a.type;
    this->channel_uuid = a.channel_uuid;
    this->server_uuid = a.server_uuid;
    this->profile = a.profile;
  };
};

struct PnxMediaData : virtual PnxMediaInfo {
  PnxMediaTime time_info;
  PhoenixPlayInfo phoenix_play_info;
  Pnx::Media::MediaSourceFramePtr data = nullptr;

 public:
  PnxMediaData operator=(const PnxMediaInfo& a) {
    _CopyMediaInfo(a);
    return *this;
  }
  PnxMediaData operator=(const PnxMediaData& a) {
    _CopyMediaData(a);
    return *this;
  }

 protected:
  void _CopyMediaData(const PnxMediaData& a) {
    _CopyMediaInfo(a);
    time_info = a.time_info;
    phoenix_play_info = a.phoenix_play_info;
    data = a.data;
  };
};

struct PnxMediaDataGroup : virtual PnxMediaInfo {
  PnxMediaDataGroup() { data_vector_ = std::make_shared<std::vector<std::shared_ptr<PnxMediaData>>>(); };
  std::shared_ptr<PnxMediaData> GetFirstMediaData() {
    if (GetNumber() <= 0) {
      return nullptr;
    }
    return data_vector_->front();
  };
  std::shared_ptr<PnxMediaData> GetLastMediaData() {
    if (GetNumber() <= 0) {
      return nullptr;
    }
    return data_vector_->back();
  };
  std::shared_ptr<PnxMediaData> At(size_t index) {
    return data_vector_->at(index);
  }
  const PnxMediaTime& GetTheLatestTime() { return the_latest_time_; }
  void PushBack(std::shared_ptr<PnxMediaData> data) {
    data_vector_->push_back(data);
    if (data->type == Pnx::Media::MediaType::Video)
      the_latest_time_ = the_latest_time_ < data->time_info ? data->time_info : the_latest_time_;
  };
  std::shared_ptr<PnxMediaData> PopFront() {
    if (GetNumber() <= 0) {
      return nullptr;
    }
    std::shared_ptr<PnxMediaData> ret = data_vector_->front();
    data_vector_->erase(data_vector_->begin());
    return ret;
  }
  size_t GetNumber() { return data_vector_->size(); };

 public:
  PnxMediaDataGroup operator=(const PnxMediaInfo& a) {
    _CopyMediaInfo(a);
    return *this;
  }
  PnxMediaDataGroup operator=(const PnxMediaDataGroup& a) {
    _CopyMediaInfo(a);
    data_vector_ = a.data_vector_;
    the_latest_time_ = a.the_latest_time_;
    return *this;
  }

 protected:
  std::shared_ptr<std::vector<std::shared_ptr<PnxMediaData>>> data_vector_;  // data structure is not fixed. (std::vector is temporary)
  PnxMediaTime the_latest_time_ = 0;
};

struct PnxMediaArchiveInfo : virtual PnxMediaInfo {
  void SetArchivingDrive(std::filesystem::path archiving_drive) { drive_ = archiving_drive; };
  std::filesystem::path GetArchivingDrive(void) const { return drive_; };

 public:
  PnxMediaArchiveInfo operator=(const PnxMediaInfo& a) {
    _CopyMediaInfo(a);
    return *this;
  }
  PnxMediaArchiveInfo operator=(const PnxMediaArchiveInfo& a) {
    _CopyMediaArchiveInfo(a);
    return *this;
  }

 protected:
  void _CopyMediaArchiveInfo(const PnxMediaArchiveInfo& a) {
    _CopyMediaInfo(a);
    this->drive_ = a.drive_;
  }
  std::filesystem::path drive_;
};

struct PnxMediaArchiveData : PnxMediaArchiveInfo, PnxMediaData {
 public:
  PnxMediaArchiveData operator=(const PnxMediaInfo& a) {
    _CopyMediaInfo(a);
    return *this;
  }
  PnxMediaArchiveData operator=(const PnxMediaArchiveInfo& a) {
    _CopyMediaArchiveInfo(a);
    return *this;
  }
  PnxMediaArchiveData operator=(const PnxMediaData& a) {
    _CopyMediaData(a);
    return *this;
  }
};

struct PnxMediaArchiveDataEx : PnxMediaArchiveData {
  std::shared_ptr<PnxMedia> ex_data;
};

struct PnxMediaArchiveDataGroup : PnxMediaArchiveInfo, PnxMediaDataGroup {
  bool isArchiving = false;

 public:
  PnxMediaArchiveDataGroup operator=(const PnxMediaInfo& a) {
    _CopyMediaInfo(a);
    return *this;
  }
};

struct PnxMediaBackchannelData : virtual PnxMediaInfo {
  std::vector<unsigned char> data;

  public:
    PnxMediaBackchannelData operator=(const PnxMediaInfo& a) {
      _CopyMediaInfo(a);
      return *this;
    }
    PnxMediaBackchannelData operator=(const PnxMediaBackchannelData& a) {
      _CopyMediaBackchannelData(a);
      return *this;
    }
  protected:
    void _CopyMediaBackchannelData(const PnxMediaBackchannelData& a) {
      _CopyMediaInfo(a);
      data = a.data;
    };
};
