#include "util/read_file.hpp"

#include <fstream>
#include <sstream>

namespace pp {

std::string read_file(std::string_view file_name) {
  const std::ifstream file{file_name.data(), std::ios_base::binary};
  if (!file.good() || file.bad() || !file.is_open()) {
    throw std::system_error(errno, std::generic_category(),
                            std::format("failed to read file: {}", file_name));
  }
  std::stringstream stream{};
  stream << file.rdbuf();
  return stream.str();
}

} // namespace pp
