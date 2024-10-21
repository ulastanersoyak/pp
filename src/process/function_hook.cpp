#include "process/process.hpp"
#include "util/read_file.hpp"

#include <algorithm>
#include <cstring>
#include <format>
#include <string>
#include <unordered_map>

#ifdef __linux__
#include <elf.h>
#else
#error "only linux is supported"
#endif

namespace pp {

static std::unordered_map<std::string, std::uintptr_t> addr_cache{};

[[nodiscard]] std::vector<std::string> process::function_names() const {
#ifdef __linux__
  const auto path = this->exe_path();
  const auto elf_opt = read_elf(path);
  if (!elf_opt.has_value()) {
    throw std::runtime_error(std::format("failed to read the file: {}", path));
  }
  const auto elf = elf_opt.value().c_str();
  Elf64_Ehdr elf_header{};
  std::memcpy(&elf_header, elf, sizeof(elf_header));

  std::vector<Elf64_Shdr> section_headers{elf_header.e_shnum};
  std::memcpy(section_headers.data(), elf + elf_header.e_shoff,
              sizeof(Elf64_Shdr) * section_headers.size());

  //  symbol section headers
  std::vector<Elf64_Shdr> matching_symbols;
  std::ranges::copy_if(section_headers, std::back_inserter(matching_symbols),
                       [](const Elf64_Shdr &header) {
                         return header.sh_type == SHT_DYNSYM ||
                                header.sh_type == SHT_SYMTAB;
                       });

  if (matching_symbols.empty()) [[unlikely]] {
    throw std::system_error(
        errno, std::generic_category(),
        std::format("failed to find  symbols in elf file in pid: {}",
                    this->pid()));
  }
  std::vector<std::string> fn_names{};
  for (const auto &symbols : matching_symbols) {
    // read symbols
    std::vector<Elf64_Sym> syms(symbols.sh_size / sizeof(Elf64_Sym));
    std::memcpy(syms.data(), elf + symbols.sh_offset, symbols.sh_size);
    // get the string table associated with this symbol table
    const auto symbol_str_table = section_headers.at(symbols.sh_link);
    std::vector<char> str_table(symbol_str_table.sh_size);
    std::memcpy(str_table.data(), elf + symbol_str_table.sh_offset,
                str_table.size());
    // extract function names
    for (const auto &sym : syms) {
      const auto name = std::string_view(str_table.data() + sym.st_name);
      if (!name.empty()) {
        fn_names.push_back(name.data());
        if (!addr_cache.contains(std::string(name))) {
          const auto addr = this->base_addr() + sym.st_value;
          addr_cache.insert({std::string(name), addr});
        }
      }
    }
  }
  return fn_names;
#else
#error "only linux is supported"
#endif
}

[[nodiscard]] std::optional<uintptr_t>
process::func_addr(std::string_view function_name) const {
#ifdef __linux__
  if (addr_cache.contains(std::string(function_name))) {
    return addr_cache.at(std::string(function_name));
  }
  const auto path = this->exe_path();
  const auto elf_opt = read_elf(path);
  if (!elf_opt.has_value()) {
    throw std::runtime_error(std::format("failed to read the file: {}", path));
  }
  const auto elf = elf_opt.value().c_str();
  Elf64_Ehdr elf_header{};
  std::memcpy(&elf_header, elf, sizeof(elf_header));
  std::vector<Elf64_Shdr> section_headers{elf_header.e_shnum};
  std::memcpy(section_headers.data(), elf + elf_header.e_shoff,
              sizeof(Elf64_Shdr) * section_headers.size());
  // symbol section headers
  std::vector<Elf64_Shdr> matching_symbols;
  std::ranges::copy_if(section_headers, std::back_inserter(matching_symbols),
                       [](const Elf64_Shdr &header) {
                         return header.sh_type == SHT_DYNSYM ||
                                header.sh_type == SHT_SYMTAB;
                       });
  for (const auto &symbols : matching_symbols) {
    // read symbols
    std::vector<Elf64_Sym> syms(symbols.sh_size / sizeof(Elf64_Sym));
    std::memcpy(syms.data(), elf + symbols.sh_offset, symbols.sh_size);
    // get the string table associated with this symbol table
    const auto symbol_str_table = section_headers.at(symbols.sh_link);
    std::vector<char> str_table(symbol_str_table.sh_size);
    std::memcpy(str_table.data(), elf + symbol_str_table.sh_offset,
                str_table.size());
    // extract function addresses
    for (const auto &sym : syms) {
      const auto name = std::string_view(str_table.data() + sym.st_name);
      const auto proc_base_addr = base_addr();
      if (name.contains(function_name)) {
        uintptr_t func_address = proc_base_addr + sym.st_value;
        return func_address;
      }
    }
  }
  return std::nullopt;
#else
#error "only linux is supported"
#endif
}

} // namespace pp
