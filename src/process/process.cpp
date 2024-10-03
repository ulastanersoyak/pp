#include "process/process.hpp"

#ifdef __linux__
#include <algorithm>
#include <filesystem>
#include <fstream>
#endif

namespace pp {

[[nodiscard]] std::uint32_t process::pid() const noexcept { return this->pid_; }

[[nodiscard]] std::string process::name() const {
  std::string name;
#ifdef __linux__
  const std::filesystem::path comm_path =
      "/proc" / std::filesystem::path(std::to_string((this->pid_))) / "comm";
  std::ifstream comm(comm_path);
  if (!comm.is_open()) {
    throw std::invalid_argument("unable to open file: " + comm_path.string());
  }
  std::getline(comm, name);
  if (name.empty()) {
    throw std::runtime_error("unable to read file: +" + comm_path.string());
  }
#endif
  return name;
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

[[nodiscard]] std::vector<memory_region> process::memory_regions() const {
  std::vector<memory_region> mem_region_vec{};
#ifdef __linux__
  const std::filesystem::path maps_path =
      "/proc" / std::filesystem::path(std::to_string((this->pid_))) / "maps";

  std::ifstream maps{maps_path};
  if (!maps.is_open()) {
    throw std::runtime_error("unable to open file " + maps_path.string());
  }
  std::string line;
  while (std::getline(maps, line)) {
    mem_region_vec.emplace_back(line);
  }
#endif
  return mem_region_vec;
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
    throw std::invalid_argument("no process found with the name : " +
                                std::string(name));
  }
  return processes;
}

} // namespace pp
