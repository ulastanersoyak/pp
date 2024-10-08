#pragma once

#include "debugger/registers.hpp"
#include "process/process.hpp"

#include <cstddef>
#include <optional>
#include <vector>

#ifdef __linux__
#include <sys/ptrace.h>
#endif

namespace pp {

class debugger {
  process proc_{0};
  std::vector<thread> suspended_threads{};

public:
  debugger(process &&proc, std::optional<std::size_t> timeout = std::nullopt);

  debugger(const debugger &debug) = delete;
  debugger &operator=(const debugger &debug) = delete;

  debugger(debugger &&debug) noexcept = default;
  debugger &operator=(debugger &&debug) noexcept = default;

  ~debugger() noexcept;

  [[nodiscard]] registers get_regs(const thread &t) const;
  void set_regs(const thread &t, const registers &regs) const;
  // void set_regs() const;
};

} // namespace pp
