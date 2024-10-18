#include "debugger/debugger.hpp"
#include "memory_region/memory_region.hpp"
#include "memory_region/permission.hpp"

#include <algorithm>
#include <bit>
#include <ranges>

#ifdef __linux__
#include <sys/mman.h>
#include <sys/wait.h>
#endif

namespace pp {

// TODO: auto release of injected code and registers if an exception occurs
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
  // destructive
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
  // destructive
  this->set_regs(this->main_thread(), edited_regs);

  if (ptrace(PTRACE_CONT, main_thread_tid, 0, 0) == -1) {
    throw std::system_error(
        errno, std::generic_category(),
        std::format("failed to continue to tid: {}", main_thread_tid));
  }
  std::int32_t wstatus = 0;
  // wait for a process to hit int3
  if (waitpid(static_cast<std::int32_t>(main_thread_tid), &wstatus, 0) == -1) {
    throw std::system_error(
        errno, std::generic_category(),
        std::format("failed to wait for tid: {}", main_thread_tid));
  }
  // check if given process got any signal other than sigtrap
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
