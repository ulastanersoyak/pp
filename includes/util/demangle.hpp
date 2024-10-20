#include <cstdint>
#include <cxxabi.h>
#include <string>
#include <string_view>

namespace pp {
[[nodiscard]] constexpr std::string demangle(std::string_view raw_fn_name) {
  std::int32_t status{};
  const auto rs =
      abi::__cxa_demangle(raw_fn_name.data(), nullptr, nullptr, &status);
  return status != 0 ? std::string(raw_fn_name) : std::string(rs);
}
} // namespace pp
