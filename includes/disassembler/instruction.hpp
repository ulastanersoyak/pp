#pragma once

#include <cstddef>
#include <format>
#include <string>
#include <string_view>

namespace pp {
class instruction {
public:
  instruction(std::string_view mnemonic, std::string_view operands,
              std::size_t size, std::uintptr_t address);

  std::string_view mnemonic() const;
  std::string_view operands() const;
  std::size_t size() const;
  std::uintptr_t address() const;

private:
  std::string mnemonic_;
  std::string operands_;
  std::size_t size_;
  std::uintptr_t address_;
};
} // namespace pp

template <> struct std::formatter<pp::instruction> {
  constexpr auto parse(std::format_parse_context &ctx) {
    return std::begin(ctx);
  }

  auto format(const pp::instruction &obj, std::format_context &ctx) const {
    return std::format_to(ctx.out(), "{:#x}: {} {} ({})", obj.address(),
                          obj.mnemonic(), obj.operands(), obj.size());
  }
};
