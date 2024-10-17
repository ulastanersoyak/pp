#include "debugger/debugger.hpp"
#include "debugger/registers.hpp"
#include "memory_region/memio.hpp"
#include "memory_region/memory_region.hpp"
#include "memory_region/permission.hpp"

#ifdef __linux__
#include <algorithm>
#include <bit>
#include <cerrno>
#include <chrono>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <format>
#include <fstream>
#include <limits>
#include <print>
#include <ranges>
#include <sstream>
#include <stdexcept>
#include <system_error>

#include <dlfcn.h>
#include <elf.h>
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

void debugger::load_library(std::string_view path) const {
#ifdef __x86_64__
#ifdef __linux__
  const auto regions = this->proc_.memory_regions();
  using namespace std::literals;
  const auto libc_region =
      std::ranges::find_if(regions, [](const memory_region &region) {
        return region.name()->contains("libc.so"sv);
      });
  if (libc_region == std::ranges::cend(regions)) [[unlikely]] {
    throw std::runtime_error(
        std::format("no libc region was found in pid: {}", this->proc_.pid()));
  }
  const std::ifstream file{libc_region->name()->c_str(), std::ios_base::binary};
  if (!file.good() || file.bad() || !file.is_open()) {
    throw std::system_error(
        errno, std::generic_category(),
        std::format("failed to read libc in pid: {}", this->proc_.pid()));
  }
  std::stringstream stream{};
  stream << file.rdbuf();

  const auto elf_str = stream.str();
  const auto *elf = elf_str.data();

  Elf64_Ehdr elf_header{};
  std::memcpy(&elf_header, elf, sizeof(elf_header));
  if (std::memcmp(elf_header.e_ident, ELFMAG, SELFMAG) != 0) [[unlikely]] {
    throw std::system_error(errno, std::generic_category(),
                            std::format("wrong file format for libc in pid: {}",
                                        this->proc_.pid()));
  }
  std::vector<Elf64_Shdr> section_headers{elf_header.e_shnum};
  std::memcpy(section_headers.data(), elf + elf_header.e_shoff,
              sizeof(Elf64_Shdr) * section_headers.size());
  // dynamic symbol section header
  const auto symbols =
      std::ranges::find_if(section_headers, [](const Elf64_Shdr &header) {
        return header.sh_type == SHT_DYNSYM;
      });

  if (symbols == std::ranges::cend(section_headers)) [[unlikely]] {
    throw std::system_error(
        errno, std::generic_category(),
        std::format("failed to find symbols in elf file in pid: {}",
                    this->proc_.pid()));
  }

  std::vector<Elf64_Sym> syms(symbols->sh_size / sizeof(Elf64_Sym));
  std::memcpy(syms.data(), elf + symbols->sh_offset, symbols->sh_size);
  const auto symbol_str_table = section_headers.at(symbols->sh_link);
  std::vector<char> str_table(symbol_str_table.sh_size);
  std::memcpy(str_table.data(), elf + symbol_str_table.sh_offset,
              str_table.size());
  [[maybe_unused]] std::uintptr_t dlopen_addr{};
  for (const auto &sym : syms) {
    if (std::string_view(str_table.data() + sym.st_name) == "dlopen"sv) {
      dlopen_addr = sym.st_value + libc_region->begin();
      break;
    }
  }
  // inject the .so file's path to the targets memory
  const auto mem_region = this->allocate_memory(4096U);
  auto mem_buffer = read_memory_region(this->proc_, mem_region);
  std::memcpy(mem_buffer.data(), path.data(), path.length());
  write_memory_region(this->proc_, mem_region, mem_buffer);

  const auto stack = this->allocate_memory(4096U);

  const auto main_thread_tid = this->main_thread().tid();
  const auto saved_regs = this->get_regs(this->main_thread());
  const auto executable_region =
      std::ranges::find_if(regions, [](const memory_region &region) {
        return region.has_permissions(permission::EXECUTE);
      });
  if (executable_region == std::ranges::cend(regions)) [[unlikely]] {
    throw std::range_error(
        std::format("couldnt find an executable memory region of tid: {}",
                    main_thread_tid));
  }
  errno = 0;
  const auto instr = ptrace(PTRACE_PEEKTEXT, main_thread_tid,
                            executable_region->begin(), nullptr);
  if (errno != 0) {
    throw std::system_error(
        errno, std::generic_category(),
        std::format("failed to read the memory of tid: {}", main_thread_tid));
  }
  // assembly for the instructions below
  // nop -> 90
  // nop -> 90
  // call rbx -> FF D3
  // int3 -> CC
  // nop -> 90
  // nop -> 90
  // nop -> 90
  const std::uint64_t hook_instr = 0x909090CCD3FF9090;
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
  edited_regs.regs.rip = executable_region->begin() + 2;
  edited_regs.regs.rbx = dlopen_addr;
  edited_regs.regs.rdi = mem_region.begin();
  edited_regs.regs.rsi = RTLD_NOW;
  edited_regs.regs.rsp = stack.begin() + stack.size();
  edited_regs.regs.rbp = edited_regs.regs.rsp;
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
}

} // namespace pp
