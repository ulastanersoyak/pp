#pragma once

#include "process/memory_region/permission.hpp"

#include <cstdint>

namespace pp {

class memory_region {
  std::uintptr_t begin_{0};
  std::size_t size_{0};
  permission permissions_{0};

public:
#ifdef __linux__
  memory_region(const std::string &region);
#endif
  [[nodiscard]] std::uintptr_t begin() const noexcept;
  [[nodiscard]] std::size_t size() const noexcept;
  [[nodiscard]] permission permissions() const noexcept;
  [[nodiscard]] bool has_permissions(permission perm) const noexcept;
};

} // namespace pp
