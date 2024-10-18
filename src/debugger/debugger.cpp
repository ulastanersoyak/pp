#include "debugger/debugger.hpp"
#include "debugger/registers.hpp"
#include "memory_region/memio.hpp"

#include <cerrno>
#include <chrono>
#include <cstddef>
#include <cstring>
#include <format>
#include <stdexcept>
#include <system_error>

#ifdef __linux__
#include <sys/mman.h>
#include <sys/ptrace.h>
#include <sys/user.h>
#include <sys/wait.h>
#endif

namespace pp {

debugger::debugger(process &proc, std::optional<std::size_t> timeout)
    : proc_{std::move(proc)} {
#ifdef __linux__
  auto timeout_duration = std::chrono::milliseconds(
      timeout.value_or(std::numeric_limits<std::size_t>::max()));
  const auto start_time = std::chrono::steady_clock::now();
  const auto threads = proc.threads();

  for (const thread &t : threads) {
    if (ptrace(PTRACE_ATTACH, static_cast<std::int32_t>(t.tid()), nullptr,
               nullptr) == -1) {
      throw std::system_error(
          errno, std::generic_category(),
          std::format("failed to attach to tid: {}", t.tid()));
    }
    while (true) {
      std::int32_t status{0};
      if (waitpid(static_cast<std::int32_t>(t.tid()), &status, 0) == -1) {
        throw std::system_error(
            errno, std::generic_category(),
            std::format("failed to wait for tid: {}", t.tid()));
      }
      if (WIFSTOPPED(status)) {
        this->suspended_threads.emplace_back(proc.pid(), t.tid());
        break;
      } else {
        if (ptrace(PTRACE_CONT, static_cast<std::int32_t>(t.tid()), nullptr,
                   WSTOPSIG(status)) == -1) {
          throw std::system_error(
              errno, std::generic_category(),
              std::format("failed to continue to tid: {}", t.tid()));
        }
      }
      if (std::chrono::steady_clock::now() - start_time >= timeout_duration) {
        throw std::runtime_error("timeout while waiting for threads to stop");
      }
    }
  }
  if (this->suspended_threads.size() != threads.size()) {
    throw std::system_error(
        errno, std::generic_category(),
        std::format("failed to attach to all threads of pid: {}", proc.pid()));
  }
#endif
}

debugger::~debugger() noexcept {
  for (const auto &t : this->suspended_threads) {
    if (ptrace(PTRACE_DETACH, static_cast<std::int32_t>(t.tid()), nullptr,
               nullptr) == -1) {
      perror("PTRACE_DETACH failed");
      std::exit(EXIT_FAILURE);
    }
  }
}

[[nodiscard]] registers debugger::get_regs(const thread &t) const {
#ifdef __x86_64__
#ifdef __linux__
  user_regs_struct regs{};
  if (ptrace(PTRACE_GETREGS, static_cast<std::int32_t>(t.tid()), nullptr,
             &regs) == -1) {
    throw std::system_error(
        errno, std::generic_category(),
        std::format("failed to get registers of tid: {}", t.tid()));
  }

#endif
#else
#error "only x86_64 architecture is supported"
#endif
  return {.regs = regs};
}

void debugger::set_regs(const thread &t, const registers &regs) const {
#ifdef __x86_64__
#ifdef __linux__
  if (ptrace(PTRACE_SETREGS, static_cast<std::int32_t>(t.tid()), nullptr,
             &regs.regs) == -1) {
    throw std::system_error(
        errno, std::generic_category(),
        std::format("failed to set registers of tid: {}", t.tid()));
  }
#endif
#else
#error "only x86_64 architecture registers are supported"
#endif
}

[[nodiscard]] thread debugger::main_thread() const {
  if (this->suspended_threads.size() == 0) {
    throw std::range_error(
        std::format("no threads are being suspended by debugger"));
  }
  return this->suspended_threads.at(0);
}

} // namespace pp
