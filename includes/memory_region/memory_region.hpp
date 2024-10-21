#pragma once

#include "permission.hpp"

#include <cstdint>
#include <optional>

namespace pp {

class memory_region {
  std::uintptr_t begin_{0};
  std::size_t size_{0};
  permission permissions_{0};
  std::optional<std::string> name_{std::nullopt};

public:
#ifdef __linux__
  memory_region(const std::string &region);
#else
#error "only linux is supported"
#endif
  memory_region(std::uintptr_t begin, std::size_t size, permission permissions,
                const std::optional<std::string> &name = std::nullopt)
      : begin_{begin}, size_{size}, permissions_{permissions}, name_{name} {}
  [[nodiscard]] std::uintptr_t begin() const noexcept;
  [[nodiscard]] std::size_t size() const noexcept;
  [[nodiscard]] permission permissions() const noexcept;
  [[nodiscard]] std::optional<std::string> name() const noexcept;
  [[nodiscard]] bool has_permissions(permission perm) const noexcept;
};

} // namespace pp
