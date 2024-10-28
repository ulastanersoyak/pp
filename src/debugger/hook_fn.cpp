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

void debugger::hook(const function &target, std::string_view hook,
                    const std::filesystem::path &source) const {
  // compile the user given function and inject it to the target process
  auto fn_bin = compile_func(hook, source);
  const auto allocated_region = this->allocate_memory(page_size);
  write_memory_region(this->proc_, allocated_region, fn_bin);
  // prepare to modify the target function
  const auto target_fn_region = addr_to_region(this->proc_, target.address);
  this->change_region_permissions(target_fn_region);

  auto destination_address = allocated_region.begin();
  std::vector<std::byte> instr;
  instr.emplace_back(std::byte(0x48)); // REX prefix for 64-bit
  instr.emplace_back(std::byte(0xB8)); // mov rax, imm64
  for (size_t i = 0; i < sizeof(destination_address); ++i) {
    // jmp addr
    instr.emplace_back(reinterpret_cast<std::byte *>(&destination_address)[i]);
  }
  instr.emplace_back(std::byte(0xFF)); // jmp r/m32
  instr.emplace_back(std::byte(0xE0)); // jmp eax
  instr.emplace_back(std::byte(0xC3)); // ret

  const auto fn_offset = target.address - target_fn_region.begin();
  auto target_region_context =
      read_memory_region(this->proc_, target_fn_region);

  if (fn_offset + instr.size() > target_region_context.size()) {
    throw std::runtime_error(
        "not enough space in target function to write instructions");
  }
  std::println("instsize {}", instr.size());
  std::println("fn_binsize {}", fn_bin.size());
  std::println("fn_addr {:x}", target.address);
  std::println("allocated_region {:x}", allocated_region.begin());
  for (std::size_t i = 0; i < instr.size(); i++) {
    target_region_context.at(fn_offset + i) = instr.at(i);
  }
  write_memory_region(this->proc_, target_fn_region, target_region_context);
}

} // namespace pp
