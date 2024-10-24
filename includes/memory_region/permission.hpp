#pragma once

#include <cstdint>
#include <string>

namespace pp {

enum class permission {
  NO_PERMISSION = 0 << 0,
  READ = 1 << 0,
  WRITE = 1 << 1,
  EXECUTE = 1 << 2
};

[[nodiscard]] std::string permission_to_str(permission prot) noexcept;
[[nodiscard]] std::int64_t to_native(permission perm);

permission operator|=(permission &perm1, permission perm2) noexcept;
[[nodiscard]] permission operator|(permission perm1, permission perm2) noexcept;
permission operator&=(permission &perm1, permission perm2) noexcept;
[[nodiscard]] permission operator&(permission perm1, permission perm2) noexcept;
permission operator^=(permission &perm1, permission perm2) noexcept;
[[nodiscard]] permission operator^(permission perm1, permission perm2) noexcept;
} // namespace pp
