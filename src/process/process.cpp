#include "process/process.hpp"
#include "memory_region/memio.hpp"
#include "util/is_elf.hpp"
#include "util/read_file.hpp"

#include <algorithm>
#include <cassert>
#include <cerrno>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <system_error>

#ifdef __linux__
#include <elf.h>
#include <sys/mman.h>
#include <sys/ptrace.h>
#include <sys/wait.h>
#endif

namespace pp {

[[nodiscard]] std::uint32_t process::pid() const noexcept { return this->pid_; }

[[nodiscard]] std::string process::name() const {
  std::string name;
#ifdef __linux__
  std::string comm_path = std::format("/proc/{}/comm", this->pid_);
  std::ifstream comm(comm_path);
  if (!comm.is_open()) {
    throw std::filesystem::filesystem_error(
        std::format("unable to open file: {}", comm_path), std::error_code());
  }
  std::getline(comm, name);
  if (name.empty()) {
    throw std::filesystem::filesystem_error(
        std::format("unable to read file: {}", comm_path), std::error_code());
  }
#endif
  return name;
}

[[nodiscard]] std::vector<memory_region> process::memory_regions() const {
  std::vector<memory_region> mem_region_vec{};
#ifdef __linux__
  std::string maps_path = std::format("/proc/{}/maps", this->pid_);
  std::ifstream maps{maps_path};
  if (!maps.is_open()) {
    throw std::filesystem::filesystem_error(
        std::format("unable to open file: {}", maps_path), std::error_code());
  }
  std::string line;
  while (std::getline(maps, line)) {
    mem_region_vec.emplace_back(line);
  }
  if (mem_region_vec.empty()) {
    throw std::filesystem::filesystem_error(
        std::format("unable to read file: {}", maps_path), std::error_code());
  }
#endif
  return mem_region_vec;
}

[[nodiscard]] std::vector<thread> process::threads() const {
  std::vector<thread> thread_vec{};
#ifdef __linux__
  for (const auto &file : std::filesystem::directory_iterator{
           std::format("/proc/{}/task", this->pid_)}) {
    const auto file_name = file.path().filename().string();
    if (std::ranges::all_of(file_name,
                            [](unsigned char c) { return std::isdigit(c); })) {
      thread_vec.emplace_back(this->pid_,
                              static_cast<std::uint32_t>(std::stoi(file_name)));
    }
  }
#endif
  return thread_vec;
}

[[nodiscard]] std::uintptr_t process::base_addr() const {
  return this->memory_regions().at(0).begin();
}

[[nodiscard]] std::vector<std::uint32_t> get_all_pids() {
  std::vector<std::uint32_t> pid_vec{};
#ifdef __linux__
  for (const auto &file : std::filesystem::directory_iterator{"/proc"}) {
    const auto file_name = file.path().filename().string();
    if (std::ranges::all_of(file_name,
                            [](unsigned char c) { return std::isdigit(c); })) {
      pid_vec.push_back(static_cast<std::uint32_t>(std::stoi(file_name)));
    }
  }
#endif
  return pid_vec;
}

[[nodiscard]] std::vector<process> find_process(std::string_view name) {
  std::vector<process> processes{};
  const auto pids = get_all_pids();
  for (const auto &pid : pids) {
    process proc{pid};
    if (proc.name() == name) {
      processes.push_back(proc);
    }
  }
  if (processes.empty()) {
    throw std::invalid_argument(
        std::format("no process found with the name: {}", std::string(name)));
  }
  return processes;
}

[[nodiscard]] std::string process::exe_path() const {
#ifdef __linux__
  const auto file = read_file(std::format("/proc/{}/comm", this->pid()));
  const auto proc_name = file.substr(0, file.size() - 2);
  for (const auto &region : this->memory_regions()) {
    const auto name = region.name();
    if (name.has_value() && name.value().contains(proc_name)) {
      return region.name().value();
    }
  }
  throw std::runtime_error(
      std::format("no path found for pid: {}", this->pid()));
#endif
}

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
      }
    }
  }
  return fn_names;
#endif
}

[[nodiscard]] std::optional<uintptr_t>
process::func_addr(std::string_view function_name) const {
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
      if (name.contains(function_name)) {
        const auto proc_base_addr = base_addr();
        uintptr_t func_address = proc_base_addr + sym.st_value;
        return func_address;
      }
    }
  }
#endif
  return std::nullopt;
}

} // namespace pp
