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
 * @file    chunk_header_pool.h
 * @brief   Block pool for the ArchiveChunkBuffer header buffer
 * @author    cd.kim@hanwha.com
 *
 * @date    2026.10.02
 * @version    1.0
 */

#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <vector>

// Fixed-size block pool for the ArchiveChunkBuffer header buffer (GetHeadersMemory).
// A 100 KB new[] is above the 16 KB LFH limit, so every allocate/free waited on the process heap lock.
// Blocks come from VirtualAlloc (posix_memalign on other platforms), never from the CRT heap.
// mtx_ is a leaf lock held only for one free-list push/pop; OS calls and logging run outside it.
class ChunkHeaderPool {
 public:
  static constexpr std::size_t kBlockBytes = 128 * 1024;  // >= old 100 KB buffer, two 64 KB VirtualAlloc granules
  static constexpr std::size_t kMaxCachedBlocks = 128;    // keep >= IO thread count; retained = peak concurrent writers

  struct Stats {
    uint64_t hits = 0;            // kBlockBytes block reused from the free list
    uint64_t misses = 0;          // kBlockBytes block allocated from the OS
    uint64_t oversize = 0;        // exact one-off block for a request > kBlockBytes, never cached
    uint64_t overflow_frees = 0;  // kBlockBytes block freed on Release because the free list was full
    uint64_t failures = 0;        // OS allocation failed
    int64_t in_use = 0;           // kBlockBytes blocks handed out and not released yet
    int64_t peak_in_use = 0;
    std::size_t cached = 0;  // blocks on the free list
  };

  static ChunkHeaderPool& GetInstance();

  // Returns a block of at least `bytes` and stores its size in *capacity; pass both back to Release().
  // Returns nullptr only if the OS allocation fails.
  char* Acquire(std::size_t bytes, std::size_t* capacity);
  void Release(char* block, std::size_t capacity);
  // Frees cached blocks until at most keep_blocks remain.
  void Trim(std::size_t keep_blocks);
  Stats GetStats() const;

 private:
  ChunkHeaderPool();
  ~ChunkHeaderPool() = default;  // never runs: GetInstance() leaks the pool on purpose
  ChunkHeaderPool(const ChunkHeaderPool&) = delete;
  ChunkHeaderPool& operator=(const ChunkHeaderPool&) = delete;

  mutable std::mutex mtx_;   // MSVC: SRWLOCK based, no heap use
  std::vector<char*> free_;  // reserved to kMaxCachedBlocks, so push_back under mtx_ never allocates
  std::atomic<uint64_t> hits_{0};
  std::atomic<uint64_t> misses_{0};
  std::atomic<uint64_t> oversize_{0};
  std::atomic<uint64_t> overflow_frees_{0};
  std::atomic<uint64_t> failures_{0};
  std::atomic<int64_t> in_use_{0};
  std::atomic<int64_t> peak_in_use_{0};
};
