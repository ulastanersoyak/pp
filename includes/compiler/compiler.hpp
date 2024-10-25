#pragma once

#include <filesystem>
#include <string_view>
#include <vector>

namespace pp {
[[nodiscard]] std::vector<std::byte>
compile_func(std::string_view function_name,
             const std::filesystem::path &source_path);
}
