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
 * @file    data_loader.cc
 * @brief
 * @author    cd.kim@hanwha.com
 *
 * @date    2024.06.19
 * @version    1.0
 */

#include "data_loader.h"

DataLoader::DataLoader() {}

DataLoader::~DataLoader() {}

std::optional<std::pair<ArchiveIndexHeader, ArchiveObject>> DataLoader::LoadData(ArchiveChunkReadType achive_gov_read_type) {
  std::optional<std::pair<ArchiveIndexHeader, ArchiveObject>> arhive_data = std::nullopt;
  return arhive_data;
}

bool DataLoader::LoadData(std::ifstream& file_handle_, ArchiveIndexHeader& index_header) {
  if (index_header.packet_size < 0 || index_header.packet_size > 100000000)
    return false;

  if (data_buffer_size_ < index_header.packet_size) {
    data_buffer_ = std::shared_ptr<char>(new char[index_header.packet_size], [](char* ptr) { delete[] ptr; });
    data_buffer_size_ = index_header.packet_size;
  }

  std::streampos currentPos = file_handle_.tellg(); 
  file_handle_.seekg(0, std::ios::end);             
  long long fileSize = file_handle_.tellg();       
  file_handle_.seekg(currentPos);           

  if (index_header.file_offset + index_header.packet_size >= fileSize)
    return false;

  data_size_ = index_header.packet_size;
  file_handle_.seekg(index_header.file_offset, std::ios::beg);
  if (file_handle_.fail()) {
    file_handle_.seekg(0, std::ios::beg);
    return false;
  }
  file_handle_.read(data_buffer_.get(), index_header.packet_size);
  std::streamsize bytesRead = file_handle_.gcount();
  if (bytesRead != index_header.packet_size)
    return false;

  return true;
}

const std::shared_ptr<char> DataLoader::GetData() {
  return data_buffer_;
}