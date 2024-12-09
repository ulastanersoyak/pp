#pragma once

#include "instruction.hpp"
#include "memory_region/memio.hpp"
#include "util/type_traits.hpp"
#include <capstone/capstone.h>
#include <cstdint>
#include <span>
#include <vector>

namespace pp {

class disassembler {
public:
  disassembler();
  ~disassembler();

  // Delete copy operations since we manage a resource
  disassembler(const disassembler &) = delete;
  disassembler &operator=(const disassembler &) = delete;

  // Disassemble raw binary data
  [[nodiscard]] std::vector<instruction>
  disassemble(std::span<const std::byte> data, std::uintptr_t address) const;

  // Disassemble process memory
  template <thread_or_process T>
  [[nodiscard]] std::vector<instruction>
  disassemble(const T &t, const memory_region &region) const;

private:
  csh handle; // Capstone handle
};

} // namespace pp
