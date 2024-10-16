#include "debugger/debugger.hpp"
#include "process/process.hpp"
#include <print>

int main() {
  // auto vi = pp::find_process("vi").at(0);
  // std::println("vi pid: {}", vi.pid());
  // auto threads = vi.threads();
  // pp::debugger deb{std::move(vi)};
  // [[maybe_unused]] auto x = deb.allocate_memory(100);
  // std::println("{:x}", x.begin());
  // using namespace std::chrono_literals;
  // std::this_thread::sleep_for(2000ms); // for (const auto &thread : threads)

  pp::process nano = pp::find_process("nano").at(0);
  std::println("nano pid: {}", nano.pid());
  pp::debugger deb{nano};
  deb.load_library("TEST_STR");
  return 0;
}
