
#include <cxxabi.h>
#include <format>
#include <stdexcept>

namespace pp {
[[nodiscard]] constexpr std::string demangle(std::string_view raw_fn_name) {
  std::int32_t status{};
  const auto rs =
      abi::__cxa_demangle(raw_fn_name.data(), nullptr, nullptr, &status);
  if (status != 0) {
    throw std::runtime_error(
        std::format("failed to demangle function: {}", raw_fn_name));
  }
  return {rs};
}
} // namespace pp
