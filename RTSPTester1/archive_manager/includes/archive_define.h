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
 * @file    archive_define.h
 * @brief
 * @author    cd.kim@hanwha.com
 *
 * @date    2024.06.19
 * @version    1.0
 */



#pragma once

#ifdef _WIN32
#ifdef ARCHIVE_MANAGER_EXPORTS
#define ARCHIVE_MANAGER_API __declspec(dllexport)
#else
#define ARCHIVE_MANAGER_API __declspec(dllimport)
#endif
#else
#define ARCHIVE_MANAGER_API
#endif

#define ARCHIVE_MANAGER_API

#define _CRT_SECURE_NO_WARNINGS

#ifdef _WIN32
#define NOMINMAX
#include <Windows.h>
#else
#include <fcntl.h>
#endif
#include <ctime>
#include <string>
#include <sstream>
#include <iostream>
#include <mutex>
#include "phoenix/define_data.h"

///////////////////////////////////////
// Enum's 
//////////////////////////////////////

enum ArchiveType {
  kArchiveTypeNone = 0,
  kArchiveTypeGopVideo,
  kArchiveTypeFrameVideo,
  kArchiveTypeAudio,
  kArchiveTypeMeta,
  kArchiveTypeFlush,
};

enum ReadFrameType {
  kReadTypeIFrameOnly = 0,
  kReadTypeAllFrame,
};

// Pnx::Archive::ArchiveReadType::kArchiveReadNext: Forward gov containing an iframe for a time after the request timestamp.
// Pnx::Archive::ArchiveReadType::kArchiveReadPrev: Forward gov containing an iframe from a previous time in the request timestamp.
//enum class Pnx::Archive::ArchiveReadType {
enum ArchiveReadType {
  kArchiveReadNext,  // next gov
  kArchiveReadPrev,  // prev gov
};

// Pnx::Archive::ArchiveReadType::kArchiveReadNext: Forward entire gov spanning request timestamp
// Pnx::Archive::ArchiveReadType::kArchiveReadPrev: Only the iframe ~ timestamp section is delivered in the gov section of the request timestamp.
//enum class Pnx::Archive::ArchiveChunkReadType {
enum ArchiveChunkReadType {
  kArchiveChunkReadFull = 0,  // full gov
  kArchiveChunkReadTarget,    // target gov
};

//enum class ENCRYTION_TYPE {
enum EncryptionType {
  kEncryptionNone = 0,
  kEncryptionAES128,
  kEncryptionAES192,
  kEncryptionAES256,
};

///////////////////////////////////////
// external variable
///////////////////////////////////////

extern int g_dio_page_size_;
extern int g_dio_buffer_size_;
extern int g_dio_indexer_size_;

///////////////////////////////////////
// define param's
///////////////////////////////////////

#define IOWORKER_QUEUE_MAX_COUNT			10
#define ARCHIVER_BUFFER_MEMORY_PERCENT		70
#define ARCHIVER_SIMULATION_DELETE_TIME		7200 // 2 hour
#define ARCHIVER_VERSOIN					0001
#define SAVED_TIME							600


///////////////////////////////////////
// typedef's 
///////////////////////////////////////

#ifdef _WIN32
using FileHandle = HANDLE;
const FileHandle InvalidHandle = INVALID_HANDLE_VALUE;
#else
using FileHandle = int;
const FileHandle InvalidHandle = -1;
#endif

using SessionID = std::string;

///////////////////////////////////////
// Global function's
///////////////////////////////////////


///////////////////////////////////////
// Struct's
///////////////////////////////////////

#pragma pack(push, 1)
struct ArchiveIndexHeader {
  ArchiveType type = ArchiveType::kArchiveTypeNone;
  int file_offset = 0;
  int packet_size = 0;
  EncryptionType encryption_type = kEncryptionNone;

  unsigned long long begin_timestamp_msec = 1;  // utc time stamp
  unsigned long long end_timestamp_msec = 1;    // utc time stamp
};

 struct VideoHeader {
  unsigned int packet_size = 0;
  Pnx::Media::VideoCodecType codecType;		/*<! enum CODEC_TYPE */
  uint64_t packet_timestamp_msec = 0;			/*<! time_t */
  uint64_t device_pts = 0;                     /*<! presentation time stamp from device. microseconds)*/
  uint16_t width = 0;                          /*<! width of picture */
  uint16_t height = 0;                         /*<! height of picture */
  Pnx::Media::VideoFrameType frameType;    /*<! enum VIDEO_SLICE_TYPE */
  float fps = 0.0f;                        /*<! frame_rate */
  unsigned int bps = 0;                        /*<! data_rate (byte)*/
};

struct AudioHeader {
  unsigned int packet_size = 0;
  Pnx::Media::AudioCodecType codecType;        /*<! enum CODEC_TYPE */
  uint64_t packet_timestamp_msec = 0;			/*<! time_t */
  uint64_t device_pts = 0;                     /*<! presentation time stamp from device. microseconds)*/
  unsigned int audioChannels = 0;                    // number of audio channels (1:mono, 2:stereo)
  unsigned int audioSampleRate = 0;                  // Samples per seconds (Hz: 16000, 32000, 44100...)
  unsigned int audioBitPerSample = 0;                // Bit per Sample (bits)
  unsigned int audioBitrate = 0;                     // Bitrate (bits)
};

struct MetaHeader {
  unsigned int packet_size = 0;
  uint64_t packet_timestamp_msec = 0;			/*<! time_t */
};

struct FrameWriteIndexData {
  ArchiveType archive_type;
  uint64_t packet_timestamp_msec = 0;			/*<! time_t */
  std::filesystem::path save_drive_;
};
#pragma pack(pop)

std::string GetStreamID(SessionID session_id, MediaProfile profile);


///////////////////////////////////////
// LOG
///////////////////////////////////////

//enum LogLevel { AL_DEBUG, AL_INFO, AL_WARN, AL_ERROR, AL_FATAL };
//void set_color(LogLevel level);
//void reset_color();
//
//#ifndef LOG
//#define LOG(level, ...) LOG_(level, __FILE__, __LINE__, __VA_ARGS__)
//#endif
//void LOG_(LogLevel level, const char* file, int line, const std::string& message, ...);
