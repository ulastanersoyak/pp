#include "debugger/debugger.hpp"
#include "memory_region/memio.hpp"
#include "memory_region/permission.hpp"
#include "util/is_elf.hpp"
#include "util/read_file.hpp"

#include <algorithm>
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

  // read libc from disk
  const auto elf_str = read_file(libc_region->name().value());
  const auto *elf = elf_str.data();

  // parse out the elf header
  ::Elf64_Ehdr elf_header{};
  std::memcpy(&elf_header, elf, sizeof(elf_header));
  if (std::memcmp(elf_header.e_ident, ELFMAG, SELFMAG) != 0) {
    throw std::runtime_error("not an ELF");
  }

  // get the section headers
  std::vector<::Elf64_Shdr> section_headers(elf_header.e_shnum);
  std::memcpy(section_headers.data(), elf + elf_header.e_shoff,
              sizeof(::Elf64_Shdr) * section_headers.size());

  // find the dynamic symbol section header
  const auto dynsym = std::ranges::find_if(
      section_headers, [](const auto &e) { return e.sh_type == SHT_DYNSYM; });
  if (dynsym == std::ranges::cend(section_headers)) {
    throw std::runtime_error("cannot find dynsym");
  }

  // get all the symbols from the dynsym
  std::vector<::Elf64_Sym> sym_table(dynsym->sh_size / sizeof(::Elf64_Sym));
  std::memcpy(sym_table.data(), elf + dynsym->sh_offset, dynsym->sh_size);

  // get the string table for synsym
  const auto dynstr = section_headers[dynsym->sh_link];

  // copy all the strings from the table for easier parsing
  std::vector<char> str_table(dynstr.sh_size);
  std::memcpy(str_table.data(), elf + dynstr.sh_offset, str_table.size());

  std::uintptr_t dlopen_addr{static_cast<uintptr_t>(-1)};

  // search for dlopen
  for (const auto &symbol : sym_table) {
    if (std::string_view(str_table.data() + symbol.st_name) == "dlopen"sv) {
      // found it, resolve the address of it in the process memory (offset +
      // address of libc)
      dlopen_addr = symbol.st_value + libc_region->begin();
      break;
    }
  }

  if (dlopen_addr == -1UL) {
    throw std::system_error(errno, std::generic_category(),
                            std::format("failed to find dlopen function"));
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
  // replace the executable section with:
  //   nop       ; padding
  //   nop
  //   nop
  //   call rbx  ; address of dlopen will be in rbx
  //   int3      ; suspend process and return to debugger
  //   nop       ; padding
  //   nop
  const auto hook_instr = 0x909090ccd3ff9090;
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
