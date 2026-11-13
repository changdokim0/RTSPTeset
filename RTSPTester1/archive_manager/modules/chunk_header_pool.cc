/***********************************************************************************
 * Copyright(c) 2026 by Hanwha Vision, Inc.
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
 * @file    chunk_header_pool.cc
 * @brief   Block pool for the ArchiveChunkBuffer header buffer
 * @author    cd.kim@hanwha.com
 *
 * @date    2026.10.02
 * @version    1.0
 */

#include "chunk_header_pool.h"

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#else
#include <stdlib.h>
#endif

#include "blaze_logger.h"

namespace {

char* AllocateBlock(std::size_t bytes) {
#ifdef _WIN32
  return static_cast<char*>(VirtualAlloc(nullptr, bytes, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
#else
  void* block = nullptr;
  return posix_memalign(&block, 4096, bytes) == 0 ? static_cast<char*>(block) : nullptr;
#endif
}

void FreeBlock(char* block) {
#ifdef _WIN32
  VirtualFree(block, 0, MEM_RELEASE);
#else
  free(block);
#endif
}

}  // namespace

ChunkHeaderPool& ChunkHeaderPool::GetInstance() {
  // Intentionally leaked, like PerfStats::GetInstance(): a chunk can drop its block on any thread, also during
  // static destruction or when archive_manager is unloaded, after a function-local static pool would be destroyed.
  static ChunkHeaderPool* instance = new ChunkHeaderPool();
  return *instance;
}

ChunkHeaderPool::ChunkHeaderPool() {
  free_.reserve(kMaxCachedBlocks);
}

char* ChunkHeaderPool::Acquire(std::size_t bytes, std::size_t* capacity) {
  *capacity = 0;
  if (bytes > kBlockBytes) {
    // More than ~2,900 frames in one chunk: exact one-off block, freed again on Release
    char* block = AllocateBlock(bytes);
    if (block == nullptr) {
      failures_.fetch_add(1, std::memory_order_relaxed);
      return nullptr;
    }
    if (oversize_.fetch_add(1, std::memory_order_relaxed) == 0)
      BLAZE_LOG_WARN("[ArchiveL] ChunkHeaderPool oversize header block. size[{}]", bytes);
    *capacity = bytes;
    return block;
  }

  char* block = nullptr;
  {
    std::lock_guard<std::mutex> lock(mtx_);
    if (!free_.empty()) {
      block = free_.back();
      free_.pop_back();
    }
  }

  uint64_t misses = 0;
  if (block != nullptr) {
    hits_.fetch_add(1, std::memory_order_relaxed);
  } else {
    block = AllocateBlock(kBlockBytes);  // outside mtx_
    if (block == nullptr) {
      failures_.fetch_add(1, std::memory_order_relaxed);
      return nullptr;
    }
    misses = misses_.fetch_add(1, std::memory_order_relaxed) + 1;
  }

  int64_t in_use = in_use_.fetch_add(1, std::memory_order_relaxed) + 1;
  int64_t peak = peak_in_use_.load(std::memory_order_relaxed);
  while (in_use > peak && !peak_in_use_.compare_exchange_weak(peak, in_use, std::memory_order_relaxed)) {}

  // 64, 128, 256 ...: far more concurrent writers than IO threads, or the free list keeps overflowing
  if (misses >= 64 && (misses & (misses - 1)) == 0)
    BLAZE_LOG_WARN("[ArchiveL] ChunkHeaderPool misses[{}] in_use[{}] peak_in_use[{}] overflow_frees[{}]", misses, in_use,
                   peak_in_use_.load(std::memory_order_relaxed), overflow_frees_.load(std::memory_order_relaxed));

  *capacity = kBlockBytes;
  return block;
}

void ChunkHeaderPool::Release(char* block, std::size_t capacity) {
  if (block == nullptr)
    return;
  if (capacity != kBlockBytes) {  // oversize one-off block
    FreeBlock(block);
    return;
  }

  in_use_.fetch_sub(1, std::memory_order_relaxed);
  {
    std::lock_guard<std::mutex> lock(mtx_);
    if (free_.size() < kMaxCachedBlocks) {
      free_.push_back(block);
      return;
    }
  }
  overflow_frees_.fetch_add(1, std::memory_order_relaxed);
  FreeBlock(block);  // outside mtx_
}

void ChunkHeaderPool::Trim(std::size_t keep_blocks) {
  char* victims[kMaxCachedBlocks];
  std::size_t count = 0;
  {
    std::lock_guard<std::mutex> lock(mtx_);
    while (free_.size() > keep_blocks && count < kMaxCachedBlocks) {
      victims[count++] = free_.back();
      free_.pop_back();
    }
  }
  for (std::size_t i = 0; i < count; ++i)
    FreeBlock(victims[i]);  // outside mtx_
}

ChunkHeaderPool::Stats ChunkHeaderPool::GetStats() const {
  Stats stats;
  stats.hits = hits_.load(std::memory_order_relaxed);
  stats.misses = misses_.load(std::memory_order_relaxed);
  stats.oversize = oversize_.load(std::memory_order_relaxed);
  stats.overflow_frees = overflow_frees_.load(std::memory_order_relaxed);
  stats.failures = failures_.load(std::memory_order_relaxed);
  stats.in_use = in_use_.load(std::memory_order_relaxed);
  stats.peak_in_use = peak_in_use_.load(std::memory_order_relaxed);
  std::lock_guard<std::mutex> lock(mtx_);
  stats.cached = free_.size();
  return stats;
}
