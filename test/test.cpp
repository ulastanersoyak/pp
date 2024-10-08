#include "debugger/debugger.hpp"
#include "debugger/registers.hpp"
#include "process/process.hpp"
#include <print>

int main() {
  auto vi = pp::find_process("vi").at(0);
  auto threads = vi.threads();
  pp::debugger deb{std::move(vi)};
  for (const auto &thread : threads) {
    const auto regs = deb.get_regs(thread);
    auto changed_regs{regs};
    std::cout << "prechange regs:\n" << regs.regs;
    changed_regs.regs.r10 = 0xDEADBEEF;
    changed_regs.regs.r11 = 0xDEADBEEF;
    changed_regs.regs.r12 = 0xDEADBEEF;
    changed_regs.regs.r13 = 0xDEADBEEF;
    changed_regs.regs.r14 = 0xDEADBEEF;
    changed_regs.regs.r15 = 0xDEADBEEF;
    deb.set_regs(thread, changed_regs);
    const auto new_regs = deb.get_regs(thread);
    std::cout << "after change regs:\n" << new_regs.regs;
    deb.set_regs(thread, regs);
    const auto regs_s = deb.get_regs(thread);
    std::cout << "prechange change regs:\n" << regs_s.regs;
  }
  return 0;
}
