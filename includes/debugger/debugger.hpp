#pragma once

#include "debugger/registers.hpp"
#include "memory_region/memory_region.hpp"
#include "process/process.hpp"

#include <cstddef>
#include <optional>
#include <string_view>
#include <vector>

#ifdef __linux__
#include <sys/ptrace.h>
#endif

namespace pp {

class debugger {
  process proc_{0};
  std::vector<thread> suspended_threads{};

public:
  debugger(process &proc, std::optional<std::size_t> timeout = std::nullopt);

  debugger(const debugger &debug) = delete;
  debugger &operator=(const debugger &debug) = delete;

  debugger(debugger &&debug) noexcept = default;
  debugger &operator=(debugger &&debug) noexcept = default;

  ~debugger() noexcept;

  [[nodiscard]] registers get_regs(const thread &t) const;
  void set_regs(const thread &t, const registers &regs) const;
  [[nodiscard]] thread main_thread() const;
  [[nodiscard]] memory_region allocate_memory(std::size_t bytes) const;
  void load_library(std::string_view path) const;
  [[nodiscard]] std::string demangle(std::string_view raw_fn_name) const;
  [[nodiscard]] std::vector<std::string> raw_function_names() const;
  [[nodiscard]] std::vector<std::string> demangled_function_names() const;
  [[nodiscard]] std::uintptr_t get_func_addr(std::string_view fn_name) const;
  void hook_function(std::string_view target_fn_name,
                     std::uintptr_t hook_fn_addr) const;
};

} // namespace pp
