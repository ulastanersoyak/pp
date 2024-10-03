#include "process/process.hpp"
#include <print>
int main() {
  for (const auto &p : pp::find_process("firefox")) {
    for (const auto &reg : p.memory_regions()) {
      std::println("{} {}", reg.begin(), reg.size());
    }
  }
  return 0;
}
