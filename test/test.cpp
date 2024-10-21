#include "process/process.hpp"
#include "util/demangle.hpp"
#include <print>

int main() {
  pp::process test_prog = pp::find_process("test_prog").at(0);
  std::println("{}", test_prog.exe_path());
  for (const auto &name : test_prog.function_names()) {
    std::println("function named {} is resides at address 0x{:x} ",
                 pp::demangle(name), test_prog.func_addr(name).value_or(0));
  }
  return 0;
}
