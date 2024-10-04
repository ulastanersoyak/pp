#pragma once

#include "type_traits.hpp"

#ifdef __linux__
#include <sys/ptrace.h>
#include <sys/wait.h>
#endif

namespace pp {

template <thread_or_process T> void get_registers(const T &t) {
  const auto id = get_id(t);
#ifdef __linux__
  ptrace(PTRACE_ATTACH, id, 0, 0);
  wait_pid(id);
#endif
}

} // namespace pp