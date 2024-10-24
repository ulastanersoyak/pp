#pragma once

#include "memory_region/memory_region.hpp"
#include "thread/thread.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace pp {

struct function {
  std::string name{};
  std::uintptr_t address{};
};

class process {
  std::uint32_t pid_{0};

public:
  explicit process(std::uint32_t pid) : pid_(pid) {}
  [[nodiscard]] std::uint32_t pid() const noexcept;
  [[nodiscard]] std::string name() const;
  [[nodiscard]] std::vector<memory_region> memory_regions() const;
  [[nodiscard]] std::vector<thread> threads() const;
  [[nodiscard]] std::uintptr_t base_addr() const;
  [[nodiscard]] std::string exe_path() const;
  [[nodiscard]] std::vector<std::string> function_names() const;
  [[nodiscard]] std::optional<std::uintptr_t>
  func_addr(std::string_view fn_name) const;
  [[nodiscard]] std::vector<function> functions() const;
  [[nodiscard]] memory_region addr_to_region(std::uintptr_t addr) const;
  void hook(const function &fn) const;
};

[[nodiscard]] std::vector<std::uint32_t> get_all_pids();
[[nodiscard]] std::vector<process> find_process(std::string_view name);

} // namespace pp
