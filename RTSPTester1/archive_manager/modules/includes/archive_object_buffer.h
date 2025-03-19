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
 * @file    archive_object_buffer.h
 * @brief
 * @author    cd.kim@hanwha.com
 *
 * @date    2024.07.09
 * @version    1.0
 */

#pragma once
#include "archive_object.h"

class ArchiveObjectBuffer {
 public:
  ArchiveObjectBuffer();
  ~ArchiveObjectBuffer();

  int GetBufferSize();
  void SetBufferSize(int size);

  const std::shared_ptr<ArchiveObject> GetBuffer();

private:
  int buffer_size_ = 0;
 std::shared_ptr<ArchiveObject> buffer_data;
};
