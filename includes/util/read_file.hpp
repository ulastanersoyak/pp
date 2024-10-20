#include <optional>
#include <string>
#include <string_view>

namespace pp {

[[nodiscard]] std::string read_file(std::string_view file_name);
[[nodiscard]] std::optional<std::string> read_elf(std::string_view file_name);

} // namespace pp
