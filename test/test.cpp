#include "debugger/debugger.hpp"
#include "process/process.hpp"
#include <cstdlib>
#include <print>

int main() {
  pp::process test_prog = pp::find_process("target").at(0);
  pp::debugger deb{test_prog};

  for (const auto &fn : test_prog.functions()) {
    std::println("fn name = {}", fn.name);
    if (fn.name.contains("is_password")) {
      std::println("found fn named -> {}", fn.name);
      deb.hook(fn, "hook", "/home/retro/hook.cpp");
    }
  }
  return 0;
}
