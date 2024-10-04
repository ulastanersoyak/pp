#include "debugger/debugger.hpp"
#include "process/process.hpp"
#include <print>

int main() {
  const auto vi = pp::find_process("vi").at(0);
  pp::debugger deb{vi};
  return 0;
}
