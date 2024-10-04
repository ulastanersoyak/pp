#include "memory_region/io.hpp"
#include "memory_region/permission.hpp"
#include "process/process.hpp"
#include <print>

int main() {
  for (const auto &p : pp::find_process("vi")) {
    std::println("{} {}", p.name(), p.pid());
    for (const auto &reg : p.memory_regions()) {
      if (reg.has_permissions(pp::permission::READ | pp::permission::WRITE)) {
        using namespace std::literals;
        pp::replace_memory(p, reg, "Rap"sv, "ABU"sv);
      }
    }
  }
  return 0;
}
