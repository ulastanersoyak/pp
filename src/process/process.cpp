#include "process/process.hpp"
#include <cassert>

#ifdef __linux__
#include <algorithm>
#include <cerrno>
#include <filesystem>
#include <fstream>
#include <sys/uio.h>
#include <system_error>
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
    throw std::runtime_error(
        std::format("unable to open file: {}", comm_path.string()));
  }
  std::getline(comm, name);
  if (name.empty()) {
    throw std::runtime_error(
        std::format("unable to open file: {}", comm_path.string()));
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
    throw std::runtime_error(
        std::format("unable to open file: {}", maps_path.string()));
  }
  std::string line;
  while (std::getline(maps, line)) {
    mem_region_vec.emplace_back(line);
  }
#endif
  return mem_region_vec;
}

[[nodiscard]] std::vector<std::byte>
process::read_memory_region(const memory_region &region,
                            std::optional<std::size_t> read_size) const {
  std::vector<std::byte> mem{read_size.value_or(region.size())};
#ifdef __linux__
  iovec local{.iov_base = mem.data(), .iov_len = mem.size()};
  iovec remote{.iov_base = reinterpret_cast<void *>(region.begin()),
               .iov_len = region.size()};

  if (process_vm_readv(static_cast<std::int32_t>(this->pid_), &local, 1,
                       &remote, 1, 0) !=
      static_cast<ssize_t>(read_size.value_or(region.size()))) {
    throw std::system_error(
        errno, std::generic_category(),
        std::format("failed to read memory region beginning at: {:x}",
                    region.begin()));
  }
#endif
  return mem;
}

void process::write_memory_region(const memory_region &region,
                                  std::span<std::byte> data) const {
  assert(data.size() <= region.size());
#ifdef __linux__

  iovec local{.iov_base = const_cast<void *>(
                  reinterpret_cast<const void *>(data.data())),
              .iov_len = data.size()};

  iovec remote{.iov_base = reinterpret_cast<void *>(region.begin()),
               .iov_len = data.size()};

  if (process_vm_writev(static_cast<std::int32_t>(this->pid_), &local, 1,
                        &remote, 1, 0) != static_cast<ssize_t>(data.size())) {
    throw std::system_error(
        errno, std::generic_category(),
        std::format("failed to write to memory region beginning at: {:x}",
                    region.begin()));
  }
#endif
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
