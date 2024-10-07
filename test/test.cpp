#include "debugger/debugger.hpp"
#include "process/process.hpp"
#include <print>
#include <thread>

int main() {
  auto vi = pp::find_process("vi").at(0);
  pp::debugger deb{std::move(vi)};
  using namespace std::chrono_literals;
  std::this_thread::sleep_for(2000ms);
  return 0;
}
