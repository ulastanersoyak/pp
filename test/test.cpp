#include "debugger/debugger.hpp"
#include "process/process.hpp"
#include <print>

int main() {
  pp::process nano = pp::find_process("nano").at(0);
  std::println("nano pid: {}", nano.pid());
  pp::debugger deb{nano};
  deb.load_library("/tmp/test.so");
  return 0;
}
