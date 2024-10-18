#include "memory_region/memory_region.hpp"
#include "memory_region/permission.hpp"

#include <algorithm>
#include <cstdint>
#include <regex>
#include <stdexcept>

namespace pp {
#ifdef __linux__

[[nodiscard]] constexpr static permission
parse_permission(std::string_view line) noexcept {
  permission perm{};
  if (std::ranges::contains(line, 'r')) {
    perm |= permission::READ;
  }
  if (std::ranges::contains(line, 'w')) {
    perm |= permission::WRITE;
  }
  if (std::ranges::contains(line, 'x')) {
    perm |= permission::EXECUTE;
  }
  return perm;
}

memory_region::memory_region(const std::string &region) {
  // start         end      permissions                         name
  //  ^             ^           ^                                ^
  //  |             |           |                                |
  // 7f5cca60f000-7f5cca633000 r--p 00000000 fe:01 1576211 /usr/lib/libc.so.6
  std::regex pattern{"([a-f0-9]+)-([a-f0-9]+) ([rwxps-]{4})\\s+.*\\s+(\\S.*)"};
  std::smatch match;
  if (std::regex_match(region, match, pattern)) {
    const std::string start_str = match[1];
    const std::string end_str = match[2];
    const std::string permissions = match[3];
    const std::string name = match[4];
    const std::uintptr_t begin =
        static_cast<std::uintptr_t>(std::stoull(start_str, nullptr, 16));
    const std::size_t size = static_cast<std::size_t>(
        (std::stoull(end_str, nullptr, 16)) - static_cast<std::size_t>(begin));
    this->begin_ = begin;
    this->size_ = size;
    this->permissions_ = parse_permission(permissions);
    this->name_ = name;
  } else {
    throw std::invalid_argument("given region was invalid :" + region);
  }
}
#endif

[[nodiscard]] std::uintptr_t memory_region::begin() const noexcept {
  return this->begin_;
}

[[nodiscard]] std::size_t memory_region::size() const noexcept {
  return this->size_;
}

[[nodiscard]] permission memory_region::permissions() const noexcept {
  return this->permissions_;
}

[[nodiscard]] std::optional<std::string> memory_region::name() const noexcept {
  return this->name_;
}

[[nodiscard]] bool
memory_region::has_permissions(permission perm) const noexcept {
  return (this->permissions_ & perm) == perm;
}

} // namespace pp
