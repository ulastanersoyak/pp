#include "process/process.hpp"
#include "util/demangle.hpp"
#include <print>

int main() {
  pp::process test_prog = pp::find_process("test_prog").at(0);
  std::println("{}", test_prog.exe_path());
  for (const auto &name : test_prog.function_names()) {
    std::println("{}", pp::demangle(name));
  }
  const auto addr = test_prog.func_addr("is_args_true");
  std::println("{:x}", addr.value_or(-313131));
  return 0;
}
