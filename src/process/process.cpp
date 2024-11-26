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
#else
#error "only linux is supported"
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
#else
#error "only linux is supported"
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
#else
#error "only linux is supported"
#endif
  return thread_vec;
}

[[nodiscard]] std::uintptr_t process::base_addr() const {
  return this->memory_regions().at(0).begin();
}

[[nodiscard]] std::size_t process::mem_usage() const {
  const auto comm_path = std::format("/proc/{}/statm", this->pid_);
  const auto contents = pp::read_file(comm_path);
  std::istringstream iss(contents);
  std::size_t total_pages, resident_pages;
  if (!(iss >> total_pages >> resident_pages)) {
    throw std::runtime_error(
        std::format("Failed to parse statm contents: {}", contents));
  }
  const auto page_size = static_cast<std::size_t>(sysconf(_SC_PAGESIZE));
  return resident_pages * page_size;
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
#else
#error "only linux is supported"
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

[[nodiscard]] std::string process::exe_path() const noexcept {
#ifdef __linux__
  return std::format("/proc/{}/exe", this->pid());
#else
#error "only linux is supported"
#endif
}

} // namespace pp
