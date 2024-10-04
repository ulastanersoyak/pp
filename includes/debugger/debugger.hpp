#pragma once

#include "process/process.hpp"
#include <debugger/thread_detacher.hpp>

#include <cstddef>
#include <memory>
#include <optional>
#include <vector>

#ifdef __linux__
#include <sys/ptrace.h>
#endif

namespace pp {

class debugger {
  process proc_{0};
  std::unique_ptr<std::vector<thread>, thread_detacher_> suspended_threads{};

public:
  debugger(const process &proc,
           std::optional<std::size_t> timeout = std::nullopt);
};

} // namespace pp
