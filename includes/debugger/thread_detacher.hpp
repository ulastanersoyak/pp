#pragma once

#include <vector>

#include "thread/thread.hpp"

#ifdef __linux__
#include <sys/ptrace.h>
#endif

namespace pp {

struct thread_detacher_ {
  void operator()(std::vector<thread> *threads) const {
    if (!threads) {
      return;
    }
    for (const auto &t : *threads) {
      if (ptrace(PTRACE_DETACH, static_cast<int32_t>(t.tid()), 0, 0) == -1) {
        perror("ptrace PTRACE_DETACH failed");
      }
    }
  }
};

} // namespace pp
