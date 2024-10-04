#pragma once

#include "process/memory_region/memory_region.hpp"
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <ranges>
#include <span>
#include <string>
#include <vector>

namespace pp {

class process {
  std::uint32_t pid_{0};

public:
  explicit process(std::uint32_t pid) : pid_(pid) {}
  [[nodiscard]] std::uint32_t pid() const noexcept;
  [[nodiscard]] std::string name() const;
  [[nodiscard]] std::vector<memory_region> memory_regions() const;

  [[nodiscard]] std::vector<std::byte>
  read_memory_region(const memory_region &region,
                     std::optional<std::size_t> read_size = std::nullopt) const;

  void write_memory_region(const memory_region &region,
                           std::span<std::byte> data) const;
};

[[nodiscard]] std::vector<std::uint32_t> get_all_pids();
[[nodiscard]] std::vector<process> find_process(std::string_view name);
void replace_memory(const process proc, const memory_region &region,
                    std::span<const std::byte> find,
                    std::span<const std::byte> replace,
                    std::optional<std::size_t> occurrences = std::nullopt);

template <typename R>
void replace_memory(const process proc, const memory_region &region, R &&find,
                    R &&replace,
                    std::optional<std::size_t> occurrences = std::nullopt)
  requires std::ranges::contiguous_range<R>
{
  const auto find_bytes =
      reinterpret_cast<const std::byte *>(std::ranges::data(find));
  const auto replace_bytes =
      reinterpret_cast<const std::byte *>(std::ranges::data(replace));

  replace_memory(proc, region,
                 {find_bytes, std::ranges::size(find) *
                                  sizeof(std::ranges::range_value_t<R>)},
                 {replace_bytes, std::ranges::size(replace) *
                                     sizeof(std::ranges::range_value_t<R>)},
                 occurrences);
}

} // namespace pp
