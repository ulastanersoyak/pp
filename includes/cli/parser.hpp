#pragma once

#include <cstdlib>
#include <expected>
#include <functional>
#include <print>
#include <span>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace pp {

struct command {

  std::string_view name{};
  std::string_view description{};
  std::vector<std::string_view> args{};
  std::function<std::expected<void, std::string>(
      std::span<const std::string_view>)>
      handler{};
};

class cli_parser {
public:
  void add_command(command cmd);
  void print_usage() const;

  [[nodiscard]] auto parse(int argc, char **argv) const {
    if (argc < 2) {
      print_usage();
      exit(EXIT_FAILURE);
    }

    std::string_view cmd_name{argv[1]};

    if (cmd_name == "-h" || cmd_name == "--help") {
      print_usage();
      exit(EXIT_SUCCESS);
    }

    const auto it = this->commands.find(cmd_name);
    if (it == this->commands.end()) {
      std::println(stderr, "Unknown command: {}", cmd_name);
      print_usage();
      exit(EXIT_FAILURE);
    }

    const auto &cmd = it->second;
    std::vector<std::string_view> args;
    for (int i = 2; i < argc; ++i) {
      args.emplace_back(argv[i]);
    }

    return cmd.handler(args);
  }

private:
  std::unordered_map<std::string_view, command> commands{};
};

void load_commands(cli_parser &parser);

} // namespace pp
