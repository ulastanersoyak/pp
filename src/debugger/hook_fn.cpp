#include "compiler/compiler.hpp"
#include "debugger/debugger.hpp"
#include "memory_region/memio.hpp"
#include "util/addr_to_region.hpp"
#include "util/read_file.hpp"
#include <cstring>
#include <filesystem>
#include <print>

namespace pp {

constexpr inline std::uint16_t page_size{4096};

void debugger::hook(const function &hook_target, std::string_view function_name,
                    const std::filesystem::path &source_path) const {
  std::vector<std::byte> source_func_code =
      compile_func(function_name, source_path);
  const memory_region allocated_region = this->allocate_memory(page_size);
  write_memory_region(this->proc_, allocated_region, source_func_code);
  const auto rs = read_memory_region(this->proc_, allocated_region);
  for (const auto x : rs) {
    std::print("{:x} ", static_cast<std::uint8_t>(x));
  }

  const auto target_fn_region =
      addr_to_region(this->proc_, hook_target.address);
  this->change_region_permissions(target_fn_region);
  const auto saved_region = read_memory_region(this->proc_, target_fn_region);
  const auto saved_regs = this->get_regs(this->main_thread());
}

} // namespace pp
