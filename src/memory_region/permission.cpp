#include "memory_region/permission.hpp"

#include <string>

namespace pp {

[[nodiscard]] std::string permission_to_str(permission perm) noexcept {
  if (perm == permission::NO_PERMISSION) {
    return "NO_PERMISSION";
  }
  std::string permissions;
  if (static_cast<int>(perm) & static_cast<int>(permission::READ)) {
    permissions += "READ | ";
  }
  if (static_cast<int>(perm) & static_cast<int>(permission::WRITE)) {
    permissions += "WRITE | ";
  }
  if (static_cast<int>(perm) & static_cast<int>(permission::EXECUTE)) {
    permissions += "EXECUTE | ";
  }
  if (permissions.back() == ' ') {
    permissions = permissions.substr(0, permissions.size() - 3);
  }
  return permissions;
}

[[nodiscard]] std::int64_t to_native(permission perm) {
  return static_cast<std::int64_t>(perm);
}

permission operator|=(permission &perm1, permission perm2) noexcept {
  const auto perm1_ut = static_cast<std::underlying_type_t<permission>>(perm1);
  const auto perm2_ut = static_cast<std::underlying_type_t<permission>>(perm2);
  perm1 = static_cast<permission>(perm1_ut | perm2_ut);
  return perm1;
}

[[nodiscard]] permission operator|(permission perm1,
                                   permission perm2) noexcept {
  return perm1 |= perm2;
}

permission operator&=(permission &perm1, permission perm2) noexcept {
  const auto perm1_ut = static_cast<std::underlying_type_t<permission>>(perm1);
  const auto perm2_ut = static_cast<std::underlying_type_t<permission>>(perm2);
  perm1 = static_cast<permission>(perm1_ut & perm2_ut);
  return perm1;
}

[[nodiscard]] permission operator&(permission perm1,
                                   permission perm2) noexcept {
  return perm1 &= perm2;
}

permission operator^=(permission &perm1, permission perm2) noexcept {
  const auto perm1_ut = static_cast<std::underlying_type_t<permission>>(perm1);
  const auto perm2_ut = static_cast<std::underlying_type_t<permission>>(perm2);
  perm1 = static_cast<permission>(perm1_ut ^ perm2_ut);
  return perm1;
}

[[nodiscard]] permission operator^(permission perm1,
                                   permission perm2) noexcept {
  return perm1 ^= perm2;
}

} // namespace pp
