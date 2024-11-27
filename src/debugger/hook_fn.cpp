#include "compiler/compiler.hpp"
#include "debugger/debugger.hpp"
#include "memory_region/memio.hpp"
#include "util/addr_to_region.hpp"
#include "util/read_file.hpp"

#include <cstring>
#include <filesystem>
#include <print>

namespace pp {

constexpr inline std::uint16_t page_size{4096};

void debugger::hook(const function &target,
                    const std::filesystem::path &source) const {
  // compile and get the binary
  auto fn_bin = compile_func(source);

  // find hook_main offset in the compiled code
  const auto elf_opt = read_elf(compile_output_path);
  if (!elf_opt.has_value()) {
    throw std::runtime_error("Failed to read compiled output");
  }

  const auto *elf = elf_opt.value().data();
  Elf64_Ehdr elf_header{};
  std::memcpy(&elf_header, elf, sizeof(elf_header));

  // get symbol tables
  std::vector<Elf64_Shdr> section_headers(elf_header.e_shnum);
  std::memcpy(section_headers.data(), elf + elf_header.e_shoff,
              sizeof(Elf64_Shdr) * section_headers.size());

  Elf64_Shdr *symtab = nullptr;
  Elf64_Shdr *strtab = nullptr;

  const Elf64_Shdr &shstrtab = section_headers[elf_header.e_shstrndx];
  std::vector<char> shstr_tab(shstrtab.sh_size);
  std::memcpy(shstr_tab.data(), elf + shstrtab.sh_offset, shstrtab.sh_size);

  for (auto &section : section_headers) {
    const auto name = std::string_view(shstr_tab.data() + section.sh_name);
    if (name == ".symtab")
      symtab = &section;
    else if (name == ".strtab")
      strtab = &section;
  }

  if (!symtab || !strtab) {
    throw std::runtime_error("symbol tables not found");
  }

  // find hook_main
  std::vector<Elf64_Sym> symbols(symtab->sh_size / sizeof(Elf64_Sym));
  std::memcpy(symbols.data(), elf + symtab->sh_offset, symtab->sh_size);

  std::vector<char> str_tab(strtab->sh_size);
  std::memcpy(str_tab.data(), elf + strtab->sh_offset, strtab->sh_size);

  std::uintptr_t hook_main_offset = 0;
  for (const auto &sym : symbols) {
    if (sym.st_name == 0)
      continue;
    const auto name = std::string_view(str_tab.data() + sym.st_name);
    if (name == "hook_main") {
      hook_main_offset = sym.st_value;
      break;
    }
  }

  if (hook_main_offset == 0) {
    throw std::runtime_error("hook_main function not found");
  }

  // allocate memory and write the compiled code
  const auto allocated_region = this->allocate_memory(page_size);
  write_memory_region(this->proc_, allocated_region, fn_bin);

  // calculate actual hook_main address
  auto destination_address = allocated_region.begin() + hook_main_offset;

  // prepare to modify the target function
  const auto target_fn_region = addr_to_region(this->proc_, target.address);
  this->change_region_permissions(target_fn_region);

  // create jump instruction
  std::vector<std::byte> instr;
  instr.emplace_back(std::byte(0x48)); // REX prefix for 64-bit
  instr.emplace_back(std::byte(0xB8)); // mov rax, imm64

  // write hook_main address
  for (size_t i = 0; i < sizeof(destination_address); ++i) {
    instr.emplace_back(reinterpret_cast<std::byte *>(&destination_address)[i]);
  }

  instr.emplace_back(std::byte(0xFF)); // jmp r/m32
  instr.emplace_back(std::byte(0xE0)); // jmp eax
  instr.emplace_back(std::byte(0xC3)); // ret

  // write jump instruction to target function
  const auto fn_offset = target.address - target_fn_region.begin();
  auto target_region_context =
      read_memory_region(this->proc_, target_fn_region);

  if (fn_offset + instr.size() > target_region_context.size()) {
    throw std::runtime_error(
        "not enough space in target function to write instructions");
  }

  for (std::size_t i = 0; i < instr.size(); i++) {
    target_region_context.at(fn_offset + i) = instr.at(i);
  }

  write_memory_region(this->proc_, target_fn_region, target_region_context);
}

} // namespace pp
