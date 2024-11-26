#pragma once

#include "debugger/registers.hpp"
#include "memory_region/memory_region.hpp"
#include "process/process.hpp"

#include <cstddef>
#include <filesystem>
#include <optional>
#include <string_view>
#include <vector>

#ifdef __linux__
#include <elf.h>
#include <sys/ptrace.h>
#else
#error "only linux is supported"
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
  void change_region_permissions(const memory_region &region,
                                 permission perm = permission::READ |
                                                   permission::WRITE |
                                                   permission::EXECUTE) const;

  void hook(const function &target, const std::filesystem::path &source) const;
};

} // namespace pp
