#include "debugger/debugger.hpp"
#include "process/process.hpp"
#include <print>
#include <thread>

int main() {
  const auto vi = pp::find_process("vi").at(0);
  pp::debugger deb{vi};
  using namespace std::chrono_literals;
  std::this_thread::sleep_for(2000ms);
  return 0;
}
