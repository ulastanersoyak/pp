#include <print>

void print_usage(std::string_view program_name) noexcept {
  std::println("{}", program_name);
}

int main(int argc, char **argv) { return 0; }
