#include "debugger/debugger.hpp"

namespace pp {

[[nodiscard]] std::vector<std::string> debugger::raw_function_names() const {
  return {};
}

[[nodiscard]] std::vector<std::string>
debugger::demangled_function_names() const {
  return {};
}

[[nodiscard]] std::uintptr_t
debugger::get_func_addr([[maybe_unused]] std::string_view fn_name) const {
  return {};
}

void debugger::hook_function(
    [[maybe_unused]] std::string_view target_fn_name,
    [[maybe_unused]] std::uintptr_t hook_fn_addr) const {}
} // namespace pp
