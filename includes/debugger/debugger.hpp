#pragma once

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
  ~debugger() noexcept;
};

} // namespace pp
