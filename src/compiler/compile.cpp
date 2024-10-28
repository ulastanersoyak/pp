#include "compiler/compiler.hpp"
#include "util/read_file.hpp"
#include <cstring>
#include <system_error>

#ifdef __linux__
#include <elf.h>
#else
#error "only linux is supported"
#endif

namespace pp {

constexpr inline std::string_view compile_output_path{"/tmp/hook.o"};
[[nodiscard]] std::vector<std::byte>
compile_func(std::string_view function_name,
             const std::filesystem::path &source_path) {
  if (system(("g++ -c " + source_path.string() + " -o " +
              compile_output_path.data() + " -fPIC -O0")
                 .c_str()) != 0) {
    throw std::runtime_error(
        std::format("failed to compile function {}", source_path.c_str()));
  }

  using namespace std::string_view_literals;
  const auto elf_opt = read_elf("/tmp/hook.o"sv);
  if (!elf_opt.has_value()) {
    throw std::system_error(
        errno, std::generic_category(),
        std::format("failed to read the elf file /tmp/hook.o"));
  }

  const auto *elf = elf_opt.value().data();
  Elf64_Ehdr elf_header{};
  std::memcpy(&elf_header, elf, sizeof(elf_header));

  std::vector<Elf64_Shdr> section_headers(elf_header.e_shnum);
  std::memcpy(section_headers.data(), elf + elf_header.e_shoff,
              sizeof(Elf64_Shdr) * section_headers.size());

  const Elf64_Shdr &shstrtab = section_headers[elf_header.e_shstrndx];
  std::vector<char> shstr_tab(shstrtab.sh_size);
  std::memcpy(shstr_tab.data(), elf + shstrtab.sh_offset, shstrtab.sh_size);

  auto get_section_name = [&](const Elf64_Shdr &sh) {
    return std::string_view(shstr_tab.data() + sh.sh_name);
  };

  Elf64_Shdr *text_section = nullptr;
  Elf64_Shdr *symtab_section = nullptr;
  Elf64_Shdr *strtab_section = nullptr;

  for (auto &section : section_headers) {
    const auto name = get_section_name(section);
    if (name == ".text")
      text_section = &section;
    else if (name == ".symtab")
      symtab_section = &section;
    else if (name == ".strtab")
      strtab_section = &section;
  }

  if (!text_section || !symtab_section || !strtab_section) {
    throw std::runtime_error("required sections not found in elf file");
  }

  std::vector<Elf64_Sym> symbols(symtab_section->sh_size / sizeof(Elf64_Sym));
  std::memcpy(symbols.data(), elf + symtab_section->sh_offset,
              symtab_section->sh_size);

  std::vector<char> strtab(strtab_section->sh_size);
  std::memcpy(strtab.data(), elf + strtab_section->sh_offset,
              strtab_section->sh_size);

  const Elf64_Sym *target_sym = nullptr;
  for (const auto &sym : symbols) {
    if (sym.st_name == 0)
      continue; // skip symbols without names
    const auto name = std::string_view(strtab.data() + sym.st_name);
    if (ELF64_ST_TYPE(sym.st_info) == STT_FUNC &&
        name.contains(function_name)) {
      target_sym = &sym;
      break;
    }
  }

  if (!target_sym) {
    throw std::runtime_error(
        std::format("function {} not found in object file", function_name));
  }

  if (target_sym->st_size == 0) {
    // if symbol size is 0, calculate size from section boundaries
    size_t function_size = 0;
    // find the next symbol in the same section to determine size
    for (const auto &sym : symbols) {
      if (sym.st_shndx == target_sym->st_shndx &&
          sym.st_value > target_sym->st_value) {
        function_size = sym.st_value - target_sym->st_value;
        break;
      }
    }

    // if no next symbol found, use section boundary
    if (function_size == 0) {
      function_size = text_section->sh_size - target_sym->st_value;
    }

    std::vector<std::byte> function_code(function_size);
    std::memcpy(function_code.data(),
                elf + text_section->sh_offset + target_sym->st_value,
                function_size);
    return function_code;
  }

  std::vector<std::byte> function_code(target_sym->st_size);
  std::memcpy(function_code.data(),
              elf + text_section->sh_offset + target_sym->st_value,
              target_sym->st_size);
  return function_code;
}

} // namespace pp
