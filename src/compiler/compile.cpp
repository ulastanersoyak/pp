#include "compiler/compiler.hpp"
#include "util/read_file.hpp"
#include <cstring>
#include <system_error>

#ifdef __linux__
#include <elf.h>
#endif

namespace pp {

constexpr inline std::string_view compile_output_path{"/tmp/hook"};

[[nodiscard]] std::vector<std::byte>
compile_func(const std::filesystem::path &source_path) {
  // Compile as position-independent executable
  const std::string compile_cmd = "g++ " + source_path.string() + " -o " +
                                  std::string(compile_output_path) +
                                  " -O1"     // Basic optimization
                                  " -fPIC"   // Position Independent Code
                                  " -pie"    // Position Independent Executable
                                  " -Wl,-E"; // Export all symbols

  if (system(compile_cmd.c_str()) != 0) {
    throw std::runtime_error(
        std::format("failed to compile function {}", source_path.c_str()));
  }

  const auto elf_opt = read_elf(compile_output_path);
  if (!elf_opt.has_value()) {
    throw std::system_error(
        errno, std::generic_category(),
        std::format("failed to read the elf file {}", compile_output_path));
  }

  const auto *elf = elf_opt.value().data();
  Elf64_Ehdr elf_header{};
  std::memcpy(&elf_header, elf, sizeof(elf_header));

  std::vector<Elf64_Shdr> section_headers(elf_header.e_shnum);
  std::memcpy(section_headers.data(), elf + elf_header.e_shoff,
              sizeof(Elf64_Shdr) * section_headers.size());

  // Find symtab and strtab first to locate our hook function
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
    throw std::runtime_error("Symbol tables not found");
  }

  // Read symbol table to find hook function
  std::vector<Elf64_Sym> symbols(symtab->sh_size / sizeof(Elf64_Sym));
  std::memcpy(symbols.data(), elf + symtab->sh_offset, symtab->sh_size);

  std::vector<char> str_tab(strtab->sh_size);
  std::memcpy(str_tab.data(), elf + strtab->sh_offset, strtab->sh_size);

  // Find the hook function symbol
  const Elf64_Sym *hook_sym = nullptr;
  for (const auto &sym : symbols) {
    if (sym.st_name == 0)
      continue;
    const auto name = std::string_view(str_tab.data() + sym.st_name);
    if (name == "hook") {
      hook_sym = &sym;
      break;
    }
  }

  if (!hook_sym) {
    throw std::runtime_error("hook function not found in symbol table");
  }

  // Get the section containing the hook function
  if (hook_sym->st_shndx >= section_headers.size()) {
    throw std::runtime_error("invalid section index for hook function");
  }

  const auto &func_section = section_headers[hook_sym->st_shndx];

  // Extract just the hook function code
  std::vector<std::byte> function_code(hook_sym->st_size);
  std::memcpy(function_code.data(),
              elf + func_section.sh_offset + hook_sym->st_value -
                  func_section.sh_addr,
              hook_sym->st_size);

  return function_code;
}

} // namespace pp
