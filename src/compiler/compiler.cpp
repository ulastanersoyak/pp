#include "compiler/compiler.hpp"
#include "util/read_file.hpp"
#include <cstring>
#include <system_error>
#ifdef __linux__
#include <elf.h>
#endif

namespace pp {
[[nodiscard]] std::vector<std::byte>
compile_func(const std::filesystem::path &source_path) {

  const auto obj_path = std::string(compile_output_path) + ".o";
  const auto lib_path = std::string(compile_output_path) + ".so";

  // Clean up any existing files
  std::filesystem::remove(obj_path);
  std::filesystem::remove(lib_path);
  std::filesystem::remove(compile_output_path);

  // First compile to object file
  const std::string compile_cmd = "g++ " + source_path.string() + " -c -o " +
                                  obj_path +
                                  " -O1"   // basic optimization
                                  " -fPIC" // position independent code
                                  " -fno-exceptions"
                                  " -fno-asynchronous-unwind-tables";

  if (system(compile_cmd.c_str()) != 0) {
    std::filesystem::remove(obj_path);
    throw std::runtime_error(
        std::format("failed to compile function {}", source_path.c_str()));
  }

  // Then link it into a shared library using a different output path
  const std::string link_cmd = "g++ -shared " + obj_path + " -o " + lib_path;

  if (system(link_cmd.c_str()) != 0) {
    std::filesystem::remove(obj_path);
    throw std::runtime_error("failed to link function");
  }

  // Clean up object file
  std::filesystem::remove(obj_path);

  // Move the library to final location
  std::filesystem::rename(lib_path, compile_output_path);

  const auto elf_opt = read_elf(compile_output_path);
  if (!elf_opt.has_value()) {
    throw std::system_error(
        errno, std::generic_category(),
        std::format("failed to read the elf file {}", compile_output_path));
  }

  const auto *elf = elf_opt.value().data();
  Elf64_Ehdr elf_header{};
  std::memcpy(&elf_header, elf, sizeof(elf_header));

  // Get all necessary section headers
  std::vector<Elf64_Shdr> section_headers(elf_header.e_shnum);
  std::memcpy(section_headers.data(), elf + elf_header.e_shoff,
              sizeof(Elf64_Shdr) * section_headers.size());

  // Get string tables
  const Elf64_Shdr &shstrtab = section_headers[elf_header.e_shstrndx];
  std::vector<char> shstr_tab(shstrtab.sh_size);
  std::memcpy(shstr_tab.data(), elf + shstrtab.sh_offset, shstrtab.sh_size);

  // Find .text and .symtab sections
  const Elf64_Shdr *text_section = nullptr;
  const Elf64_Shdr *symtab = nullptr;
  const Elf64_Shdr *strtab = nullptr;

  for (const auto &section : section_headers) {
    const auto name = std::string_view(shstr_tab.data() + section.sh_name);
    if (name == ".text")
      text_section = &section;
    else if (name == ".symtab")
      symtab = &section;
    else if (name == ".strtab")
      strtab = &section;
  }

  if (!text_section || !symtab || !strtab) {
    throw std::runtime_error("Required sections not found");
  }

  // Find hook_main in symbol table
  std::vector<Elf64_Sym> symbols(symtab->sh_size / sizeof(Elf64_Sym));
  std::memcpy(symbols.data(), elf + symtab->sh_offset, symtab->sh_size);

  std::vector<char> str_tab(strtab->sh_size);
  std::memcpy(str_tab.data(), elf + strtab->sh_offset, strtab->sh_size);

  const Elf64_Sym *hook_main_sym = nullptr;
  for (const auto &sym : symbols) {
    if (sym.st_name == 0)
      continue;
    const auto name = std::string_view(str_tab.data() + sym.st_name);
    if (name == "hook_main") {
      hook_main_sym = &sym;
      break;
    }
  }

  if (!hook_main_sym) {
    throw std::runtime_error("hook_main function not found");
  }

  // Extract just the hook_main function code
  const auto offset = hook_main_sym->st_value - text_section->sh_addr;
  const auto size = hook_main_sym->st_size;

  if (offset + size > text_section->sh_size) {
    throw std::runtime_error("Invalid function boundaries");
  }

  std::vector<std::byte> code(size);
  std::memcpy(code.data(), elf + text_section->sh_offset + offset, size);

  return code;
}
} // namespace pp
