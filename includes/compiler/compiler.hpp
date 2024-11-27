#pragma once

#include <filesystem>
#include <vector>

namespace pp {

constexpr inline std::string_view compile_output_path{"/tmp/hook"};

[[nodiscard]] std::vector<std::byte>
compile_func(const std::filesystem::path &source_path);
} // namespace pp
