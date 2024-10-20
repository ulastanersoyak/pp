#include <cstring>
#ifdef __linux__
#include "util/read_file.hpp"
#include <elf.h>
#include <string_view>
namespace pp {
[[nodiscard]] constexpr bool is_elf(std::string_view file_path) {
  const auto elf_str = read_file(file_path);
  const auto *elf = elf_str.data();
  Elf64_Ehdr elf_header{};
  std::memcpy(&elf_header, elf, sizeof(elf_header));
  if (std::memcmp(elf_header.e_ident, ELFMAG, SELFMAG) != 0) [[unlikely]] {
    return false;
  }
  return true;
}
} // namespace pp
#endif
