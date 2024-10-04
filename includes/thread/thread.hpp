#pragma once

#include <cstdint>
#include <string>

namespace pp {

class thread {
  std::uint32_t pid_{0};
  std::uint32_t tid_{0};

public:
  thread(std::uint32_t pid_, std::uint32_t tid);
  [[nodiscard]] std::uint32_t tid() const;
  [[nodiscard]] std::string name() const;
};

} // namespace pp
