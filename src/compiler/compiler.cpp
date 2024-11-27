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

  const std::string compile_cmd = "g++ " + source_path.string() + " -o " +
                                  std::string(compile_output_path) +
                                  " -O1"   // basic optimization
                                  " -fPIC" // position independent code
                                  " -pie"; // position independent executable

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

  // get the text section with all code
  std::vector<Elf64_Shdr> section_headers(elf_header.e_shnum);
  std::memcpy(section_headers.data(), elf + elf_header.e_shoff,
              sizeof(Elf64_Shdr) * section_headers.size());

  // find .text section
  const Elf64_Shdr &shstrtab = section_headers[elf_header.e_shstrndx];
  std::vector<char> shstr_tab(shstrtab.sh_size);
  std::memcpy(shstr_tab.data(), elf + shstrtab.sh_offset, shstrtab.sh_size);

  const Elf64_Shdr *text_section = nullptr;
  for (const auto &section : section_headers) {
    const auto name = std::string_view(shstr_tab.data() + section.sh_name);
    if (name == ".text") {
      text_section = &section;
      break;
    }
  }

  if (!text_section) {
    throw std::runtime_error("Text section not found");
  }

  // extract entire text section
  std::vector<std::byte> code(text_section->sh_size);
  std::memcpy(code.data(), elf + text_section->sh_offset,
              text_section->sh_size);

  return code;
}

} // namespace pp
