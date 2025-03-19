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
 * @file    archive_object_buffer.cpp
 * @brief
 * @author    cd.kim@hanwha.com
 *
 * @date    2024.07.09
 * @version    1.0
 */


#include "archive_object_buffer.h"

ArchiveObjectBuffer::ArchiveObjectBuffer() {
  buffer_data = std::make_shared<ArchiveObject>();
}

ArchiveObjectBuffer::~ArchiveObjectBuffer() {

}

int ArchiveObjectBuffer::GetBufferSize() {
  return buffer_data.get()->size;
}

void ArchiveObjectBuffer::SetBufferSize(int size) {
  buffer_data.get()->size = size;  // size + header's
  buffer_data.get()->data = std::shared_ptr<char>(new char[buffer_data.get()->size], [](char* ptr) { delete[] ptr; });
}

const std::shared_ptr<ArchiveObject> ArchiveObjectBuffer::GetBuffer() {
  return buffer_data;
}