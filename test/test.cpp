#include "debugger/debugger.hpp"
#include "debugger/registers.hpp"
#include "process/process.hpp"
#include <print>
#include <thread>

int main() {
  auto vi = pp::find_process("vi").at(0);
  pp::debugger deb{std::move(vi)};
  const auto regs = deb.get_regs();
  using namespace std::chrono_literals;
  std::this_thread::sleep_for(2000ms);
  for (const auto &reg : regs) {
    std::cout << reg.fp_regs << '\n' << reg.regs;
  }
  return 0;
}
