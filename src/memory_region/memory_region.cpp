#include "memory_region/memory_region.hpp"
#include "memory_region/permission.hpp"
#include <algorithm>
#include <cstdint>
#ifdef __linux__
#include <regex>
#include <stdexcept>
#endif

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
  std::regex pattern{"([a-f0-9]+)-([a-f0-9]+) ([rwxps-]{4}).*"};
  std::smatch match;
  if (std::regex_match(region, match, pattern)) {
    const std::string start_str = match[1];
    const std::string end_str = match[2];
    const std::string permissions = match[3];
    const std::uintptr_t begin =
        static_cast<std::uintptr_t>(std::stoull(start_str, nullptr, 16));
    const std::size_t size = static_cast<std::size_t>(
        (std::stoull(end_str, nullptr, 16)) - static_cast<std::size_t>(begin));
    this->begin_ = begin;
    this->size_ = size;
    this->permissions_ = parse_permission(permissions);
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

[[nodiscard]] bool
memory_region::has_permissions(permission perm) const noexcept {
  return (this->permissions_ & perm) == perm;
}

} // namespace pp
