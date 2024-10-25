#include "debugger/debugger.hpp"
#include "process/process.hpp"
#include <cstdlib>
#include <print>

int main() {
  pp::process test_prog = pp::find_process("test_prog").at(0);
  pp::debugger deb{test_prog};

  for (const auto &fn : test_prog.functions()) {
    if (fn.name.contains("is_args_true")) {
      std::println("found fn named -> {}", fn.name);
      deb.hook(fn, "sum", "/home/retro/basic.cpp");
    }
  }
  return 0;
}
