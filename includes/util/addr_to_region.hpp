#pragma once

#include "memory_region/memory_region.hpp"
#include "process/process.hpp"
#include <stdexcept>

namespace pp {
[[nodiscard]] constexpr memory_region addr_to_region(const process &proc,
                                                     std::uintptr_t addr) {
  for (const auto &region : proc.memory_regions()) {
    if (addr >= region.begin() && addr < region.begin() + region.size()) {
      return region;
    }
  }
  throw std::invalid_argument("");
}
} // namespace pp
