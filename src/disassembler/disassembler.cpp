#include "disassembler/disassembler.hpp"
#include <format>
#include <memory>
#include <stdexcept>

namespace {
class capstone_disassembler {
public:
  capstone_disassembler(::csh handle, std::span<const std::byte> data,
                        std::uintptr_t address)
      : instruction(nullptr), count(0) {
    count = ::cs_disasm(handle, reinterpret_cast<const uint8_t *>(data.data()),
                        data.size(), address, 0, &instruction);
    if (count == 0) {
      throw std::runtime_error("failed to disassemble data");
    }
  }

  ~capstone_disassembler() { ::cs_free(instruction, count); }

  capstone_disassembler(const capstone_disassembler &) = delete;
  capstone_disassembler &operator=(const capstone_disassembler &) = delete;

  ::cs_insn *instruction;
  std::size_t count;
};
} // namespace

namespace pp {

disassembler::disassembler() : handle(0) {
  if (::cs_open(CS_ARCH_X86, CS_MODE_64, &handle) != CS_ERR_OK) {
    throw std::runtime_error("failed to initialise capstone");
  }

  if (::cs_option(handle, CS_OPT_DETAIL, CS_OPT_ON) != CS_ERR_OK) {
    ::cs_close(&handle);
    throw std::runtime_error("failed to set capstone option");
  }
}

disassembler::~disassembler() { ::cs_close(&handle); }

std::vector<instruction>
disassembler::disassemble(std::span<const std::byte> data,
                          std::uintptr_t address) const {
  std::vector<instruction> instructions{};
  const capstone_disassembler cs_disasm{handle, data, address};

  for (auto i = 0u; i < cs_disasm.count; ++i) {
    const auto &inst = cs_disasm.instruction[i];
    instructions.emplace_back(inst.mnemonic, inst.op_str, inst.size,
                              inst.address);
  }

  return instructions;
}

template <thread_or_process T>
std::vector<instruction>
disassembler::disassemble(const T &t, const memory_region &region) const {
  auto mem = read_memory_region(t, region);
  return disassemble(mem, region.begin());
}

} // namespace pp
