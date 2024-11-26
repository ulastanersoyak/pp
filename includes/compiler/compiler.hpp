#pragma once

#include <filesystem>
#include <vector>

namespace pp {
[[nodiscard]] std::vector<std::byte>
compile_func(const std::filesystem::path &source_path);
}
