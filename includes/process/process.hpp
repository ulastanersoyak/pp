#pragma once

#include "process/memory_region/memory_region.hpp"
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace pp {

class process {
  std::uint32_t pid_{0};

public:
  explicit process(std::uint32_t pid) : pid_(pid) {}
  [[nodiscard]] std::uint32_t pid() const noexcept;
  [[nodiscard]] std::string name() const;
  [[nodiscard]] std::vector<memory_region> memory_regions() const;

  [[nodiscard]] std::vector<std::byte>
  read_memory_region(const memory_region &region,
                     std::optional<std::size_t> read_size = std::nullopt) const;

  void write_memory_region(const memory_region &region,
                           std::span<std::byte> data) const;
};

[[nodiscard]] std::vector<std::uint32_t> get_all_pids();
[[nodiscard]] std::vector<process> find_process(std::string_view name);

} // namespace pp
