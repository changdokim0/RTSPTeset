#include "includes/raid_write_optimizer.h"

#include <fstream>
#include <sstream>
#include <string>
#include <algorithm>

namespace {

std::string Trim(const std::string& value) {
  const auto begin = value.find_first_not_of(" \t\r\n");
  if (begin == std::string::npos) {
    return "";
  }
  const auto end = value.find_last_not_of(" \t\r\n");
  return value.substr(begin, end - begin + 1);
}

std::string ReadSingleLine(const std::string& path) {
  std::ifstream in(path);
  if (!in.is_open()) {
    return "";
  }
  std::string line;
  std::getline(in, line);
  return Trim(line);
}

bool TryParsePositiveInt(const std::string& text, int* out) {
  if (out == nullptr) {
    return false;
  }

  try {
    const int value = std::stoi(text);
    if (value <= 0) {
      return false;
    }
    *out = value;
    return true;
  } catch (...) {
    return false;
  }
}

bool TryParsePositiveSize(const std::string& text, std::size_t* out) {
  if (out == nullptr) {
    return false;
  }

  try {
    const auto value = static_cast<std::size_t>(std::stoull(text));
    if (value == 0) {
      return false;
    }
    *out = value;
    return true;
  } catch (...) {
    return false;
  }
}

bool GetMountedDeviceForPath(const std::string& target_path, std::string* device_name) {
  if (device_name == nullptr) {
    return false;
  }

  std::ifstream mounts("/proc/self/mounts");
  if (!mounts.is_open()) {
    return false;
  }

  const std::string target = target_path;
  std::string best_mount;
  std::string best_device;
  std::string line;

  while (std::getline(mounts, line)) {
    std::istringstream iss(line);
    std::string device;
    std::string mount_point;
    if (!(iss >> device >> mount_point)) {
      continue;
    }

    if (target.rfind(mount_point, 0) != 0) {
      continue;
    }

    if (target.size() > mount_point.size() && mount_point != "/" && target[mount_point.size()] != '/') {
      continue;
    }

    if (mount_point.size() > best_mount.size()) {
      best_mount = mount_point;
      best_device = device;
    }
  }

  if (best_device.empty()) {
    return false;
  }

  *device_name = best_device;
  return true;
}

int GetDataDiskCount(const io_worker_internal::RaidGeometry& geom) {
  if (geom.level == 5) {
    return geom.raid_disks - 1;
  }
  if (geom.level == 6) {
    return geom.raid_disks - 2;
  }
  return 0;
}

std::size_t AlignUp(std::size_t value, std::size_t align) {
  if (align == 0) {
    return value;
  }
  return ((value + align - 1) / align) * align;
}

}  // namespace

namespace io_worker_internal {

bool TryGetRaidGeometryForPath(const std::string& target_path, RaidGeometry* out) {
  if (out == nullptr) {
    return false;
  }

  std::string device_name;
  if (!GetMountedDeviceForPath(target_path, &device_name)) {
    return false;
  }

  const std::string prefix = "/dev/md";
  if (device_name.rfind(prefix, 0) != 0) {
    return false;
  }

  const std::string md_name = device_name.substr(std::string("/dev/").size());
  const std::string md_base = std::string("/sys/block/") + md_name + "/md/";

  const std::string level_text = ReadSingleLine(md_base + "level");
  const std::string disks_text = ReadSingleLine(md_base + "raid_disks");
  const std::string chunk_text = ReadSingleLine(md_base + "chunk_size");

  RaidGeometry geom;
  if (level_text.find("raid1") != std::string::npos) {
    geom.level = 1;
  } else if (level_text.find("raid5") != std::string::npos) {
    geom.level = 5;
  } else if (level_text.find("raid6") != std::string::npos) {
    geom.level = 6;
  } else {
    return false;
  }

  if (!TryParsePositiveInt(disks_text, &geom.raid_disks)) {
    return false;
  }

  // RAID1 does not need chunk_size for downstream logic.
  if (geom.level == 1) {
    geom.chunk_size = 0;
  } else if (!TryParsePositiveSize(chunk_text, &geom.chunk_size)) {
    return false;
  }

  *out = geom;
  return true;
}

std::size_t CalcRaidOptimizedWriteSize(const RaidGeometry& geom, std::size_t min_write_size, std::size_t page_size) {
  const int data_disks = GetDataDiskCount(geom);
  if (data_disks <= 0 || geom.chunk_size == 0) {
    return 0;
  }

  const std::size_t full_stripe = geom.chunk_size * static_cast<std::size_t>(data_disks);
  if (full_stripe == 0) {
    return 0;
  }

  const std::size_t stripe_aligned_size = AlignUp(std::max(min_write_size, full_stripe), full_stripe);
  return AlignUp(stripe_aligned_size, page_size);
}

}  // namespace io_worker_internal
