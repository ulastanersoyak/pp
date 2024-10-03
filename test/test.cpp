#include "process/memory_region/permission.hpp"
#include "process/process.hpp"
#include <print>

int main() {
  for (const auto &p : pp::find_process("firefox")) {
    std::println("{} {}", p.name(), p.pid());
    for (const auto &reg : p.memory_regions()) {
      if (reg.has_permissions(pp::permission::READ)) {
        std::println("region permissions: {}\nregion begin:{:x}",
                     pp::permission_to_str(reg.permissions()), reg.begin());
        try {
          const auto mem = p.read_memory_region(reg);
        } catch (...) {
        }
      }
    }
  }
  return 0;
}
