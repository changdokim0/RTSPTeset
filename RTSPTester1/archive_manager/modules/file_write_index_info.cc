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
 * @file    file_write_index_info.cc
 * @brief
 * @author    cd.kim@hanwha.com
 *
 * @date    2024.06.19
 * @version    1.0
 */

#include "file_write_index_info.h"


FileWriteIndexInfo::FileWriteIndexInfo(){
}
FileWriteIndexInfo ::~FileWriteIndexInfo() {
}

void FileWriteIndexInfo::AddFrameInfo(FrameWriteIndexData timestamp) {
  std::lock_guard<std::mutex> lock(queue_mtx_);
  frameInfos_.push_back(timestamp);
}


std::shared_ptr<std::vector<FrameWriteIndexData>> FileWriteIndexInfo::GetFrameInfos(std::string driver) {
  std::lock_guard<std::mutex> lock(queue_mtx_);
  std::shared_ptr<std::vector<FrameWriteIndexData>> frameinfos = std::make_shared<std::vector<FrameWriteIndexData>>();
  for (auto frameinfo : frameInfos_) {
    frameinfos->push_back(frameinfo);
    frameinfo.save_drive_ = driver;
  }
  return frameinfos;
}

void FileWriteIndexInfo::ResetInfo() {
  std::lock_guard<std::mutex> lock(queue_mtx_);
  if (frameInfos_.empty())
    return;

  auto it = std::find_if(frameInfos_.rbegin(), frameInfos_.rend(), [](const FrameWriteIndexData& data) { return data.archive_type == kArchiveTypeFrameVideo; });
  std::vector<FrameWriteIndexData> extracted;
  if (it != frameInfos_.rend()) {
    extracted.push_back(*it);
    extracted.insert(extracted.end(), it.base(), frameInfos_.end());
  }
  frameInfos_.clear();
  frameInfos_.insert(frameInfos_.end(), extracted.begin(), extracted.end());
}

void FileWriteIndexInfo::Clear() {
  std::lock_guard<std::mutex> lock(queue_mtx_);
  if (frameInfos_.empty())
    return;

  frameInfos_.clear();
}