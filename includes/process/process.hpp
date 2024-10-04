#pragma once

#include "memory_region/memory_region.hpp"
#include "thread/thread.hpp"

#include <cstdint>
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

  [[nodiscard]] std::vector<thread> threads() const;
};

[[nodiscard]] std::vector<std::uint32_t> get_all_pids();
[[nodiscard]] std::vector<process> find_process(std::string_view name);

} // namespace pp
