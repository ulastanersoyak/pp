#include "debugger/debugger.hpp"
#include "process/process.hpp"
#include <print>

int main() {
  pp::process nano = pp::find_process("nano").at(0);
  std::println("{}", nano.exe_path());
  pp::debugger deb{nano};
  return 0;
}
