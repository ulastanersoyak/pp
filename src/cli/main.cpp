#include "cli/parser.hpp"

#include <expected>
#include <print>

int main(int argc, char **argv) {
  pp::cli_parser parser{};
  pp::load_commands(parser);

  if (auto result = parser.parse(argc, argv); !result) {
    std::println(stderr, "Error: {}", result.error());
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
