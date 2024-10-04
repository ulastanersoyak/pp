#include "thread/thread.hpp"

#ifdef __linux__
#include <cerrno>
#include <fstream>
#include <sys/uio.h>
#include <system_error>
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
#endif
  return name;
}

} // namespace pp
