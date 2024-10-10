#include "debugger/debugger.hpp"
#include "process/process.hpp"
#include <print>
#include <thread>

int main() {
  auto vi = pp::find_process("vi").at(0);
  std::println("vi pid: {}", vi.pid());
  auto threads = vi.threads();
  pp::debugger deb{std::move(vi)};
  [[maybe_unused]] auto x = deb.allocate_memory(100);
  std::println("{:x}", x.begin());
  using namespace std::chrono_literals;
  std::this_thread::sleep_for(2000ms); // for (const auto &thread : threads) {
  //   const auto regs = deb.get_regs(thread);
  //   auto changed_regs{regs};
  //   std::cout << "prechange regs:\n" << regs.regs;
  //   changed_regs.regs.r10 = 0xDEADBEEF;
  //   changed_regs.regs.r11 = 0xDEADBEEF;
  //   changed_regs.regs.r12 = 0xDEADBEEF;
  //   changed_regs.regs.r13 = 0xDEADBEEF;
  //   changed_regs.regs.r14 = 0xDEADBEEF;
  //   changed_regs.regs.r15 = 0xDEADBEEF;
  //   deb.set_regs(thread, changed_regs);
  //   const auto new_regs = deb.get_regs(thread);
  //   std::cout << "after change regs:\n" << new_regs.regs;
  // deb.set_regs(thread, regs);
  // const auto regs_s = deb.get_regs(thread);
  // std::cout << "prechange change regs:\n" << regs_s.regs;
  // }
  return 0;
}
