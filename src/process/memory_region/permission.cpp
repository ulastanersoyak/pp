#include "process/memory_region/permission.hpp"

namespace pp {

[[nodiscard]] std::string permission_to_str(enum permission prot) noexcept {
  std::string perm_str;
  if (prot == permission::NO_PERMISSION) {
    return "NO_PERMISSION";
  }
  if (prot == permission::READ) {
    perm_str += "READ | ";
  }
  if (prot == permission::WRITE) {
    perm_str += "WRITE | ";
  }
  if (prot == permission::EXECUTE) {
    perm_str += "EXECUTE | ";
  }
  if (perm_str.back() == ' ') {
    perm_str = perm_str.substr(0, perm_str.size() - 3);
  }

  return perm_str;
}

permission operator|=(permission &perm1, permission perm2) noexcept {
  const auto prot1_ut = static_cast<std::underlying_type_t<permission>>(perm1);
  const auto prot2_ut = static_cast<std::underlying_type_t<permission>>(perm2);
  perm1 = static_cast<permission>(prot1_ut | prot2_ut);
  return perm1;
}

[[nodiscard]] permission operator|(permission perm1,
                                   permission perm2) noexcept {
  return perm1 |= perm2;
}

permission operator&=(permission &perm1, permission perm2) noexcept {
  const auto prot1_ut = static_cast<std::underlying_type_t<permission>>(perm1);
  const auto prot2_ut = static_cast<std::underlying_type_t<permission>>(perm2);
  perm1 = static_cast<permission>(prot1_ut & prot2_ut);
  return perm1;
}

[[nodiscard]] permission operator&(permission perm1,
                                   permission perm2) noexcept {
  return perm1 &= perm2;
}

permission operator^=(permission &perm1, permission perm2) noexcept {
  const auto prot1_ut = static_cast<std::underlying_type_t<permission>>(perm1);
  const auto prot2_ut = static_cast<std::underlying_type_t<permission>>(perm2);
  perm1 = static_cast<permission>(prot1_ut ^ prot2_ut);
  return perm1;
}

[[nodiscard]] permission operator^(permission perm1,
                                   permission perm2) noexcept {
  return perm1 ^= perm2;
}

} // namespace pp
