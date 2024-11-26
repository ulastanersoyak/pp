#include "debugger/debugger.hpp"
#include "process/process.hpp"
#include "util/demangle.hpp"
#include <cstdlib>
#include <print>

int main() {
  pp::process test_prog = pp::find_process("among_stars").at(0);
  pp::debugger deb{test_prog};
  const auto fns = test_prog.functions();
  for (const auto &fn : fns) {
    if (pp::demangle(fn.name).contains("hit")) {
      std::println("found fn named -> {}", fn.name);
      // deb.hook(fn, "/home/retro/hook.cpp");
    }
  }
  return 0;
}
