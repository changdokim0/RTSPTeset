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
 * @file    archive_object.h
 * @brief
 * @author    cd.kim@hanwha.com
 *
 * @date    2024.06.19
 * @version    1.0
 */

#pragma once
#include <iostream>
#include <memory>
#include "archive_define.h"
#include "Pnx++/Media/MediaSourceFrame.h"

class ArchiveObject {
 public:
  int size = 0;
  std::shared_ptr<char> data;
};


class BaseBuffer {
 public:
  BaseBuffer() {}
  virtual ~BaseBuffer() {}
};

class ARCHIVE_MANAGER_API StreamBuffer : public BaseBuffer {
 public:
  ArchiveType archive_type = kArchiveTypeNone;
  unsigned long long timestamp_msec = 0;
  unsigned int packet_size = 0;
  Pnx::Media::MediaSourceFramePtr buffer;

 protected:
  void _CopyFromPnxMediaData(PnxMediaData* pnx_media_data) {
    buffer = pnx_media_data->data;
    packet_size = pnx_media_data->data->dataSize();
    timestamp_msec = pnx_media_data->time_info.ToMilliSeconds();
  }
};

class ARCHIVE_MANAGER_API VideoData : public StreamBuffer {
 public:
  VideoHeader archive_header;
  VideoData() {};
  VideoData(PnxMediaData* p) {
    VideoData::_CopyFromPnxMediaData(p);
  };
  VideoData operator=(PnxMediaData* pnx_media_data) {
    VideoData::_CopyFromPnxMediaData(pnx_media_data);
    return *this;
  }

 private:
  void _CopyFromPnxMediaData(PnxMediaData* pnx_media_data) {
    StreamBuffer::_CopyFromPnxMediaData(pnx_media_data);
    if (auto video = std::dynamic_pointer_cast<Pnx::Media::VideoSourceFrame>(pnx_media_data->data); video) {
      archive_header.packet_size = pnx_media_data->data->dataSize();
      archive_header.packet_timestamp_msec = pnx_media_data->time_info.ToMilliSeconds();
      archive_header.fps = video->frameRate;
      archive_header.codecType = video->codecType;
      archive_header.frameType = video->frameType;
      archive_header.height = video->resolution.height;
      archive_header.width = video->resolution.width;
      archive_type = kArchiveTypeFrameVideo;
    } else {
      std::cerr << "Failed to get video source frame during copy process" << std::endl;
    }
  }
};

class ARCHIVE_MANAGER_API AudioData : public StreamBuffer {
 public:
  AudioHeader archive_header;
  AudioData() {};
  AudioData(PnxMediaData* p) {
    AudioData::_CopyFromPnxMediaData(p);
  };
  AudioData operator=(PnxMediaData* pnx_media_data) {
    AudioData::_CopyFromPnxMediaData(pnx_media_data);
    return *this;
  }
  void _CopyFromPnxMediaData(PnxMediaData* pnx_media_data) {
    StreamBuffer::_CopyFromPnxMediaData(pnx_media_data);
    if (auto audio = std::dynamic_pointer_cast<Pnx::Media::AudioSourceFrame>(pnx_media_data->data); audio) {
      archive_header.codecType = audio->codecType;
      archive_header.packet_size = pnx_media_data->data->dataSize();
      archive_header.packet_timestamp_msec = pnx_media_data->time_info.ToMilliSeconds();
      archive_header.audioChannels = audio->audioChannels;
      archive_header.audioSampleRate = audio->audioSampleRate;
      archive_header.audioBitPerSample = audio->audioBitPerSample;
      archive_header.audioBitrate = audio->audioBitrate;
      archive_type = kArchiveTypeAudio;
    } else {
      std::cerr << "Failed to get audio source frame during copy process" << std::endl;
    }
  }
};

class ARCHIVE_MANAGER_API MetaData : public StreamBuffer {
 public:
  MetaHeader archive_header;
  MetaData() {};
  MetaData(PnxMediaData* p) {
    MetaData::_CopyFromPnxMediaData(p);
  };
  MetaData operator=(PnxMediaData* pnx_media_data) {
    MetaData::_CopyFromPnxMediaData(pnx_media_data);
    return *this;
  }
  void _CopyFromPnxMediaData(PnxMediaData* pnx_media_data) {
    if (auto meta = std::dynamic_pointer_cast<Pnx::Media::MetaDataSourceFrame>(pnx_media_data->data); meta) {
      archive_header.packet_size = pnx_media_data->data->dataSize();
      archive_header.packet_timestamp_msec = pnx_media_data->time_info.ToMilliSeconds();
      archive_type = kArchiveTypeMeta;
      StreamBuffer::_CopyFromPnxMediaData(pnx_media_data);
    } else {
      std::cerr << "Failed to get metadata source frame during copy process" << std::endl;
    }
  }
};
