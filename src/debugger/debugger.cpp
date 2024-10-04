#include "debugger/debugger.hpp"
#include <cstddef>
#include <cstdlib>
#include <limits>

#ifdef __linux__
#include <cerrno>
#include <format>
#include <sys/ptrace.h>
#include <sys/wait.h>
#include <system_error>
#endif

namespace pp {
debugger::debugger(const process &proc, std::optional<std::size_t> timeout)
    : proc_{proc} {
#ifdef __linux__
  auto timeout_val = timeout.value_or(std::numeric_limits<std::size_t>::max());
  for (const thread &t : proc.threads()) {
    if (ptrace(PTRACE_ATTACH, t.tid(), 0, 0) == -1) {
      throw std::system_error(
          errno, std::generic_category(),
          std::format("failed to attach to tid: {}", t.tid()));
    }
    do {
      std::int32_t status{0};
      if (waitpid(static_cast<std::int32_t>(t.tid()), &status, 0) == -1) {
        throw std::system_error(
            errno, std::generic_category(),
            std::format("failed to wait for tid: {}", t.tid()));
      }
      if (WIFSTOPPED(status)) {
        // this->suspended_threads->push_back({proc.pid(), t.tid()});
        break;
      } else {
        if (ptrace(PTRACE_CONT, t.tid(), 0, WSTOPSIG(status)) == -1) {
          throw std::system_error(
              errno, std::generic_category(),
              std::format("failed to continue to tid: {}", t.tid()));
        }
      }
      timeout_val--;
    } while (timeout_val != 0);
  }
#endif
}

} // namespace pp
