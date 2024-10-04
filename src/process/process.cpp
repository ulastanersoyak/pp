#include "process/process.hpp"

#include <cassert>
#include <cstring>
#include <string>

#ifdef __linux__
#include <algorithm>
#include <cerrno>
#include <filesystem>
#include <fstream>
#include <system_error>
#endif

namespace pp {

[[nodiscard]] std::uint32_t process::pid() const noexcept { return this->pid_; }

[[nodiscard]] std::string process::name() const {
  std::string name;
#ifdef __linux__
  std::string comm_path = std::format("/proc/{}/comm", this->pid_);
  std::ifstream comm(comm_path);
  if (!comm.is_open()) {
    throw std::system_error(errno, std::generic_category(),
                            std::format("unable to open file: {}", comm_path));
  }
  std::getline(comm, name);
  if (name.empty()) {
    throw std::system_error(errno, std::generic_category(),
                            std::format("unable to read file: {}", comm_path));
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
    throw std::system_error(errno, std::generic_category(),
                            std::format("unable to open file: {}", maps_path));
  }
  std::string line;
  while (std::getline(maps, line)) {
    mem_region_vec.emplace_back(line);
  }
  if (mem_region_vec.empty()) {
    throw std::system_error(errno, std::generic_category(),
                            std::format("unable to read file: {}", maps_path));
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

} // namespace pp
