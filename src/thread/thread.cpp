#include "thread/thread.hpp"

#include <cerrno>
#include <filesystem>
#include <fstream>
#include <system_error>

#ifdef __linux__
#include <sys/uio.h>
#endif

namespace pp {

thread::thread(std::uint32_t pid, std::uint32_t tid) : pid_{pid}, tid_{tid} {}

[[nodiscard]] std::uint32_t thread::tid() const { return this->tid_; }

[[nodiscard]] std::string thread::name() const {
  std::string name;
#ifdef __linux__
  std::string comm_path =
      std::format("/proc/{}/task/{}/comm", this->pid_, this->tid_);
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
#else
#error "only linux is supported"
#endif
  return name;
}

} // namespace pp
