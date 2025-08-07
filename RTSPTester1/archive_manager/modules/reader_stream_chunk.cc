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
 * @file    reader_stream_chunk.cc
 * @brief
 * @author    cd.kim@hanwha.com
 *
 * @date    2024.06.19
 * @version    1.0
 */ 

#include "reader_stream_chunk.h"

ReaderStreamChunk::ReaderStreamChunk() {

}

ReaderStreamChunk::~ReaderStreamChunk() {

}


bool ReaderStreamChunk::LoadFrames(std::shared_ptr<char> data, int data_size, EncryptionType encryption_type) {
  data_index_ = 0;
  datas_.clear();
  if (data_size < sizeof(int))
    return false;

  int mempos = 0;
  int header_size = 0;
  int header_count = 0;
  memcpy(&header_size, data.get(), sizeof(int));
  mempos += sizeof(int);
  memcpy(&header_count, data.get() + mempos, sizeof(int));
  mempos += sizeof(int);

  if (data_size < header_size || header_count < 0 || header_count > ARCHIVER_CHUNK_MAX_SIZE) {
    SPDLOG_ERROR("FAIL LoadFrames Header header_size[{}], data_size[{}], header_count[{}]", header_size, data_size, header_count);
    return false;
  }

  for (int i = 0; i < header_count; i++) {
    ArchiveType archive_type = kArchiveTypeNone; 
    memcpy(&archive_type, data.get() + mempos, sizeof(ArchiveType));
    mempos += sizeof(ArchiveType);
    if (archive_type == kArchiveTypeFrameVideo) {
      std::shared_ptr<VideoData> video = std::make_shared<VideoData>();
      memcpy(&video->archive_header, data.get() + mempos, sizeof(VideoHeader));
      video->packet_size = video->archive_header.packet_size;
      video->timestamp_msec = video->archive_header.packet_timestamp_msec;
      video->archive_type = kArchiveTypeFrameVideo;
      datas_.push_back(video);
      mempos += sizeof(VideoHeader);
    } else if (archive_type == kArchiveTypeAudio) {
      std::shared_ptr<AudioData> audio = std::make_shared<AudioData>();
      memcpy(&audio->archive_header, data.get() + mempos, sizeof(AudioHeader));
      audio->packet_size = audio->archive_header.packet_size;
      audio->timestamp_msec = audio->archive_header.packet_timestamp_msec;
      audio->archive_type = kArchiveTypeAudio;
      datas_.push_back(audio);
      mempos += sizeof(AudioHeader);
    } else if (archive_type == kArchiveTypeMeta) {
      std::shared_ptr<MetaData> meta = std::make_shared<MetaData>();
      memcpy(&meta->archive_header, data.get() + mempos, sizeof(MetaHeader));
      meta->packet_size = meta->archive_header.packet_size;
      meta->timestamp_msec = meta->archive_header.packet_timestamp_msec;
      meta->archive_type = kArchiveTypeMeta;
      datas_.push_back(meta);
      mempos += sizeof(MetaHeader);
    }
  }

  if (datas_.size() == 0)
    return false;
    
  for (auto frame : datas_) {
    if (mempos + frame->packet_size > data_size) {
      datas_.clear();
      return false;
    }
    int packet_size = frame->packet_size;
    if (encryption_type == EncryptionType::kEncryptionNone) {
      if (frame->archive_type == kArchiveTypeFrameVideo)
        frame->buffer = std::make_shared<Pnx::Media::VideoSourceFrame>((unsigned char*)data.get() + mempos, frame->packet_size);
      else if (frame->archive_type == kArchiveTypeAudio)
        frame->buffer = std::make_shared<Pnx::Media::AudioSourceFrame>((unsigned char*)data.get() + mempos, frame->packet_size);
      else if (frame->archive_type == kArchiveTypeMeta)
        frame->buffer = std::make_shared<Pnx::Media::MetaDataSourceFrame>((unsigned char*)data.get() + mempos, frame->packet_size);
    } else {
      frame->buffer = stream_crytor_.Decrypt((unsigned char*)data.get() + mempos, frame->packet_size, frame->archive_type, encryption_type);
      frame->packet_size = frame->buffer->dataSize();

      if (auto v_buffer = std::dynamic_pointer_cast<VideoData>(frame)) {
        v_buffer->archive_header.packet_size = frame->packet_size;
      } else if (auto a_buffer = std::dynamic_pointer_cast<AudioData>(frame)) {
        a_buffer->archive_header.packet_size = frame->packet_size;
      } else if (auto m_buffer = std::dynamic_pointer_cast<MetaData>(frame)) {
        m_buffer->archive_header.packet_size = frame->packet_size;
      }
    }
    mempos += packet_size;

    // ********for data verification*********
    //std::cout << std::dec << std::endl << index++ << "   ";
    //for (size_t i = 0;  i < 10; i++) {
    //  std::cout << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(static_cast<unsigned char>(frame.buffer.get()->data.get()[i]))
    //            << " ";
    //}
  }
  return true;
}

std::shared_ptr<ArchiveChunkBuffer> ReaderStreamChunk::GetStreamChunk(unsigned long long& timestamp_msec, ArchiveChunkReadType GovReadType) {
  if (datas_.size() < data_index_) {
    return nullptr;
  }

  unsigned long long cut_timestamp = timestamp_msec;
  unsigned long long select_time = 0;
  std::shared_ptr<ArchiveChunkBuffer> archive_gov_buffer = std::make_shared<ArchiveChunkBuffer>();

  for (data_index_ = 0; data_index_ < datas_.size();) {
    std::shared_ptr<StreamBuffer> data = datas_[data_index_++];
    timestamp_msec = data.get()->timestamp_msec;
    archive_gov_buffer->PushQueue(data);

    if (GovReadType == kArchiveChunkReadTarget && data->timestamp_msec >= cut_timestamp) {
      select_time = data->timestamp_msec;
      break;
    }
  }

  while (data_index_ < datas_.size()) {
    auto data = datas_[data_index_];
    if (data->archive_type == kArchiveTypeFrameVideo)
      break;
    else 
      data_index_++;    
  }

  SPDLOG_INFO("GetStreamChunk index [{}], request time[{}],  request select_time[{}]", data_index_, cut_timestamp, select_time);
  return archive_gov_buffer;
}

std::shared_ptr<ArchiveChunkBuffer> ReaderStreamChunk::GetStreamGop(unsigned long long& timestamp_msec, ArchiveChunkReadType GovReadType) {
  if (datas_.size() < data_index_) {
    return nullptr;
  }

  unsigned long long cut_timestamp = timestamp_msec;
  unsigned long long select_time = 0;
  std::shared_ptr<ArchiveChunkBuffer> archive_gov_buffer = std::make_shared<ArchiveChunkBuffer>();

  for (data_index_ = 0; data_index_ < datas_.size();) {
    std::shared_ptr<StreamBuffer> data = datas_[data_index_++];
    timestamp_msec = data.get()->timestamp_msec;

    auto videoframe = std::dynamic_pointer_cast<VideoData>(data);
    if (data_index_ > 1 && videoframe != nullptr && videoframe->archive_header.frameType == Pnx::Media::VideoFrameType::I_FRAME) {
      if (cut_timestamp >= data->timestamp_msec) {
        archive_gov_buffer->Clear();
      } else {
        select_time = data->timestamp_msec;
        data_index_--;
        break;
      }
    }

    archive_gov_buffer->PushQueue(data);
    if (GovReadType == kArchiveChunkReadTarget && data->timestamp_msec >= cut_timestamp) {
      select_time = data->timestamp_msec;
      break;
    }
  }

  while (data_index_ < datas_.size()) {
    auto data = datas_[data_index_];
    if (data->archive_type == kArchiveTypeFrameVideo)
      break;
    else
      data_index_++;    
  }

  SPDLOG_INFO("GetStreamGop index [{}], request time[{}],  request select_time[{}]", data_index_, cut_timestamp, select_time);
  return archive_gov_buffer;
}

bool ReaderStreamChunk::SeekToTime(unsigned long long& t_msec, ArchiveReadType archive_read_type) {
  if (datas_.size() <= data_index_) {
    return false;
  }
  std::pair<unsigned long long, int> min_gap = {ULLONG_MAX, 0};
  for (int i = 0; i < datas_.size(); i++) {
    if (datas_[i]->archive_type != kArchiveTypeFrameVideo)
      continue;

    long long diff = static_cast<long long>(datas_[i]->timestamp_msec - t_msec);
    if (archive_read_type != ArchiveReadType::kArchiveReadPrev && diff < 0) {
      continue;
    }
    if (archive_read_type == ArchiveReadType::kArchiveReadPrev && diff > 0) {
      continue;
    }
    unsigned long long gap = std::abs(diff);
    if (min_gap.first > gap) {
      min_gap = {gap, i};
    }
  }
  if (min_gap.first == ULLONG_MAX) {
    SPDLOG_ERROR("+++++++ FAILED TO SEEK (target:[{}]/is_forward:[{}]) +++++++\n", t_msec, (int)archive_read_type);
    for (int i = 0; i < datas_.size(); i++) {
      SPDLOG_ERROR("....... LoadedData[{}] : timestamp_msec[{}]ArchiveType[{}]\n", i, datas_[i]->timestamp_msec, (int)datas_[i]->archive_type);
    }
    SPDLOG_ERROR("------- FAILED TO SEEK (target:[{}]/is_forward:[{}]) -------\n", t_msec, (int)archive_read_type);
    return false;
  }
  data_index_ = min_gap.second;
  t_msec = datas_[min_gap.second]->timestamp_msec;
  return true;
}

std::shared_ptr<StreamBuffer> ReaderStreamChunk::GetForwardData() {

  if (datas_.size() <= data_index_)
    return nullptr;

  return datas_[data_index_++];
}

std::shared_ptr<StreamBuffer> ReaderStreamChunk::GetCurrentData() {

  if (datas_.size() <= data_index_ )
    return nullptr;

  return datas_[data_index_ ];
}

bool ReaderStreamChunk::IsInGOP(const PnxMediaTime& time) {
  if (datas_.size() <= 0) {
    return false;
  }
  auto first_video_frame = std::dynamic_pointer_cast<VideoData>(datas_[0]);
  if(first_video_frame == nullptr) {
	return false;
  }
  float gap_between_frames = 1 / first_video_frame->archive_header.fps;
  int frame_duration_msec = (int)(gap_between_frames * 1000);
  unsigned long long start_time_msec = 0;
  unsigned long long end_time_msec = 0;
  if (datas_[0]->archive_type != kArchiveTypeFrameVideo) {
    SPDLOG_ERROR("The first data of the datas_[] is not a video frame!\nPlease report this issue to hyo-jin.kim@hanwha.com.\nVERY IMPORTATNT\n");
    SPDLOG_ERROR("The first data of the datas_[] is not a video frame!\nPlease report this issue to hyo-jin.kim@hanwha.com.\nVERY IMPORTATNT\n");
    SPDLOG_ERROR("The first data of the datas_[] is not a video frame!\nPlease report this issue to hyo-jin.kim@hanwha.com.\nVERY IMPORTATNT\n");
    SPDLOG_ERROR("The first data of the datas_[] is not a video frame!\nPlease report this issue to hyo-jin.kim@hanwha.com.\nVERY IMPORTATNT\n");
    SPDLOG_ERROR("The first data of the datas_[] is not a video frame!\nPlease report this issue to hyo-jin.kim@hanwha.com.\nVERY IMPORTATNT\n");
    SPDLOG_ERROR("The first data of the datas_[] is not a video frame!\nPlease report this issue to hyo-jin.kim@hanwha.com.\nVERY IMPORTATNT\n");
    SPDLOG_ERROR("The first data of the datas_[] is not a video frame!\nPlease report this issue to hyo-jin.kim@hanwha.com.\nVERY IMPORTATNT\n");
    return false;
  } else {
    start_time_msec = datas_[0]->timestamp_msec;
  }
  for (int i = 0; i < datas_.size(); i ++) {
    if (datas_[i]->archive_type == kArchiveTypeFrameVideo) {
      end_time_msec = datas_[i]->timestamp_msec;
    }
  }
  end_time_msec += frame_duration_msec;
  if (time.ToMilliSeconds() >= start_time_msec && time.ToMilliSeconds() <= end_time_msec) {
    return true;
  }
  return false;
}

