#pragma once

#include <cstddef>
#include <string>

namespace io_worker_internal {

struct RaidGeometry {
  int level = -1;
  int raid_disks = 0;
  std::size_t chunk_size = 0;
};

bool TryGetRaidGeometryForPath(const std::string& target_path, RaidGeometry* out);

std::size_t CalcRaidOptimizedWriteSize(const RaidGeometry& geom, std::size_t min_write_size, std::size_t page_size);

}  // namespace io_worker_internal
