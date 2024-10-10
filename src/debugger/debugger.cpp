#include "debugger/debugger.hpp"
#include "debugger/registers.hpp"
#include "memory_region/memory_region.hpp"
#include "memory_region/permission.hpp"
#include <bit>

#ifdef __linux__
#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cstddef>
#include <cstdlib>
#include <format>
#include <limits>
#include <ranges>
#include <stdexcept>

#include <sys/mman.h>
#include <sys/ptrace.h>
#include <sys/user.h>
#include <sys/wait.h>
#include <system_error>
#endif

namespace pp {

debugger::debugger(process &&proc, std::optional<std::size_t> timeout)
    : proc_{std::move(proc)} {
#ifdef __linux__
  auto timeout_duration = std::chrono::milliseconds(
      timeout.value_or(std::numeric_limits<std::size_t>::max()));
  const auto start_time = std::chrono::steady_clock::now();
  const auto threads = proc.threads();

  for (const thread &t : threads) {
    errno = 0;
    if (ptrace(PTRACE_ATTACH, static_cast<std::int32_t>(t.tid()), nullptr,
               nullptr) == -1) {
      throw std::system_error(
          errno, std::generic_category(),
          std::format("failed to attach to tid: {}", t.tid()));
    }
    while (true) {
      std::int32_t status{0};
      errno = 0;
      if (waitpid(static_cast<std::int32_t>(t.tid()), &status, 0) == -1) {
        throw std::system_error(
            errno, std::generic_category(),
            std::format("failed to wait for tid: {}", t.tid()));
      }
      if (WIFSTOPPED(status)) {
        this->suspended_threads.emplace_back(proc.pid(), t.tid());
        break;
      } else {
        errno = 0;
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
  errno = 0;
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
  errno = 0;
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

[[nodiscard]] memory_region debugger::allocate_memory(std::size_t bytes) const {
#ifdef __x86_64__
  const auto main_thread_tid = this->main_thread().tid();
  const auto saved_regs = this->get_regs(this->main_thread());
  const auto regions = this->proc_.memory_regions();
  const auto executable_region =
      std::ranges::find_if(regions, [](const memory_region &region) {
        return region.has_permissions(permission::EXECUTE);
      });
  if (executable_region == std::ranges::cend(regions)) [[unlikely]] {
    throw std::range_error(
        std::format("couldnt find an executable memory region of tid: {}",
                    main_thread_tid));
  }
#ifdef __linux__
  errno = 0;
  const auto instr = ptrace(PTRACE_PEEKTEXT, main_thread_tid,
                            executable_region->begin(), nullptr);
  if (errno != 0) {
    throw std::system_error(
        errno, std::generic_category(),
        std::format("failed to read the memory of tid: {}", main_thread_tid));
  }
  // assembly for the instructions below
  // syscall -> 0F 05
  // int3 -> CC
  // nop -> 90
  // nop -> 90
  // nop -> 90
  const std::uint64_t hook_instr = 0x90909090CC050F;
  errno = 0;
  if (ptrace(PTRACE_POKETEXT, main_thread_tid, executable_region->begin(),
             hook_instr) == -1) {
    throw std::system_error(
        errno, std::generic_category(),
        std::format("failed to write to the memory of tid: {}",
                    main_thread_tid));
  }
  auto edited_regs{saved_regs};
  // https://chromium.googlesource.com/chromiumos/docs/+/master/constants/syscalls.md
  // https://stackoverflow.com/questions/3642021/what-does-mmap-do
  edited_regs.regs.rip = executable_region->begin();
  edited_regs.regs.rax = 0x09; // hex code for syscall mmap
  edited_regs.regs.rdi = 0x0;  // first arg to mmap. nullptr cuz it lets kernel
  // choose an allocation address
  edited_regs.regs.rsi = bytes; // second arg to mmap. determines how large is
                                // the mmap allocation region.
  edited_regs.regs.rdx =
      PROT_READ | PROT_EXEC |
      PROT_WRITE; // third arg to mmap. flags that mmap function expect.
  edited_regs.regs.r10 = MAP_PRIVATE | MAP_ANONYMOUS; // fourth arg to mmap.
  edited_regs.regs.r8 = std::bit_cast<std::uint64_t>(
      static_cast<std::int64_t>(-1)); // fifth arg to mmap.
  // the file descriptor; not used .because the mapping is not backed by a file
  edited_regs.regs.r9 = 0; // sixth arg to mmap. determines offset.
  this->set_regs(this->main_thread(), edited_regs);

  errno = 0;
  if (ptrace(PTRACE_CONT, main_thread_tid, 0, 0) == -1) {
    throw std::system_error(
        errno, std::generic_category(),
        std::format("failed to continue to tid: {}", main_thread_tid));
  }
  std::int32_t wstatus = 0;
  // wait for a signal on process to resume
  if (waitpid(static_cast<std::int32_t>(main_thread_tid), &wstatus, 0) == -1) {
    throw std::system_error(
        errno, std::generic_category(),
        std::format("failed to wait for tid: {}", main_thread_tid));
  }
  // check if given signal that caused process to resume was a sigtrap(int3)
  if (WSTOPSIG(wstatus) != SIGTRAP) {
    throw std::system_error(
        errno, std::generic_category(),
        std::format("failed to wait for tid: {}", main_thread_tid));
  }
  const auto rs_regs = this->get_regs(this->main_thread());
  const auto mmap_res = rs_regs.regs.rax;
  if (reinterpret_cast<void *>(mmap_res) == MAP_FAILED) {
    throw std::system_error(
        errno, std::generic_category(),
        std::format("failed mmap execution for the tid: {}", main_thread_tid));
  }
  this->set_regs(this->main_thread(), saved_regs);
  if (ptrace(PTRACE_POKETEXT, main_thread_tid, executable_region->begin(),
             instr) == -1) {
    throw std::system_error(
        errno, std::generic_category(),
        std::format("failed to restore instructions back to tid: {}",
                    main_thread_tid));
  }

#endif
#else
#error "only x86_64 architecture is supported"
#endif
  return {mmap_res, bytes,
          permission::READ | permission::WRITE | permission::EXECUTE};
}

} // namespace pp
