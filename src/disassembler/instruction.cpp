#include "disassembler/instruction.hpp"

namespace pp {

instruction::instruction(std::string_view mnemonic, std::string_view operands,
                         std::size_t size, std::uintptr_t address)
    : mnemonic_(mnemonic), operands_(operands), size_(size), address_(address) {
}

std::string_view instruction::mnemonic() const { return mnemonic_; }
std::string_view instruction::operands() const { return operands_; }
std::size_t instruction::size() const { return size_; }
std::uintptr_t instruction::address() const { return address_; }

} // namespace pp
