#include "debugger/debugger.hpp"
#include <sys/wait.h>

namespace pp {

void debugger::change_region_permissions(const memory_region &target_region,
                                         permission perm) const {
  const auto main_thread_tid = this->main_thread().tid();
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
  const auto saved_instr = ptrace(PTRACE_PEEKTEXT, main_thread_tid,
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
  if (ptrace(PTRACE_POKETEXT, main_thread_tid, executable_region->begin(),
             hook_instr) == -1) {
    throw std::system_error(
        errno, std::generic_category(),
        std::format("failed to write to the memory of tid: {}",
                    main_thread_tid));
  }
  const auto saved_regs = this->get_regs(this->main_thread());
  registers new_regs{saved_regs};
  // https://chromium.googlesource.com/chromiumos/docs/+/master/constants/syscalls.md

  new_regs.regs.rip = executable_region->begin();
  new_regs.regs.rax = 0x0A;                  // mprotect syscall id
  new_regs.regs.rdi = target_region.begin(); // address
  new_regs.regs.rsi = target_region.size();  // length
  new_regs.regs.rdx = static_cast<std::uint64_t>(to_native(perm)); // protection
  this->set_regs(this->main_thread(), new_regs);
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
  if (ptrace(PTRACE_POKETEXT, main_thread_tid, executable_region->begin(),
             saved_instr) == -1) {
    throw std::system_error(
        errno, std::generic_category(),
        std::format("failed to restore instructions back to tid: {}",
                    main_thread_tid));
  }
  this->set_regs(this->main_thread(), saved_regs);
}
} // namespace pp
