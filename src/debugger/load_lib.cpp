#include "debugger/debugger.hpp"
#include "memory_region/memio.hpp"
#include "util/is_elf.hpp"
#include "util/read_file.hpp"

#include <cstdlib>
#include <cstring>
#include <dlfcn.h>

#ifdef __linux__
#include <elf.h>
#include <sys/mman.h>
#include <sys/ptrace.h>
#include <sys/wait.h>
#else
#error "only linux is supported"
#endif

namespace pp {

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
  auto elf_str = read_file(path);
  const auto *elf = elf_str.data();

  Elf64_Ehdr elf_header{};
  std::memcpy(&elf_header, elf, sizeof(elf_header));
  if (!is_elf(path)) [[unlikely]] {
    throw std::system_error(
        errno, std::generic_category(),
        std::format("wrong file format for pid: {}", this->proc_.pid()));
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
  std::uintptr_t dlopen_addr{};
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
  // nop; 90
  // nop; 90
  // call rbx; FF D3
  // int3; CC
  // nop; 90
  // nop; 90
  // nop; 90
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
#else
#error "only linux is supported"
#endif
#else
#error "only x86_64 architecture is supported"
#endif
}

} // namespace pp
