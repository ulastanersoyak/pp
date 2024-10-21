#include "util/read_file.hpp"
#include "util/is_elf.hpp"

#include <cstring>
#include <fstream>
#include <optional>
#include <sstream>

#ifdef __linux__
#include <elf.h>
#endif

namespace pp {

[[nodiscard]] std::string read_file(std::string_view file_name) {
  const std::ifstream file{file_name.data(), std::ios_base::binary};
  if (!file.good() || file.bad() || !file.is_open()) {
    throw std::system_error(errno, std::generic_category(),
                            std::format("failed to read file: {}", file_name));
  }
  std::stringstream stream{};
  stream << file.rdbuf();
  return stream.str();
}

#ifdef __linux__
[[nodiscard]] std::optional<std::string> read_elf(std::string_view file_name) {
  if (!is_elf(file_name)) {
    return std::nullopt;
  }
  const auto elf_str = read_file(file_name);
  Elf64_Ehdr elf_header{};
  std::memcpy(&elf_header, elf_str.data(), sizeof(elf_header));
  if (std::memcmp(elf_header.e_ident, ELFMAG, SELFMAG) != 0) [[unlikely]] {
    return std::nullopt;
  }
  return elf_str;
}
#else
#error "only linux is supported"
#endif

} // namespace pp
