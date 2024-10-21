#include "memory_region.hpp"
#include "util/type_traits.hpp"

#include <cassert>
#include <cstddef>
#include <cstring>
#include <format>
#include <optional>
#include <system_error>
#include <vector>

#ifdef __linux__
#include <sys/uio.h>
#else
#error "only linux is supported"
#endif

namespace pp {

template <thread_or_process T>
[[nodiscard]] std::vector<std::byte>
read_memory_region(const T &t, const memory_region &region,
                   std::optional<std::size_t> read_size = std::nullopt) {
  {
    std::vector<std::byte> mem{read_size.value_or(region.size())};
#ifdef __linux__
    std::uint32_t id = get_id(t);
    iovec local{.iov_base = mem.data(), .iov_len = mem.size()};
    iovec remote{.iov_base = reinterpret_cast<void *>(region.begin()),
                 .iov_len = region.size()};

    if (process_vm_readv(static_cast<std::int32_t>(id), &local, 1, &remote, 1,
                         0) !=
        static_cast<ssize_t>(read_size.value_or(region.size()))) {
      throw std::system_error(
          errno, std::generic_category(),
          std::format("failed to read memory region beginning at: {:x}",
                      region.begin()));
    }
#else
#error "only linux is supported"
#endif
    return mem;
  }
}

template <thread_or_process T>
void write_memory_region(const T &t, const memory_region &region,
                         std::span<std::byte> data) {
  assert(data.size() <= region.size());
#ifdef __linux__
  iovec local{.iov_base = const_cast<void *>(
                  reinterpret_cast<const void *>(data.data())),
              .iov_len = data.size()};

  iovec remote{.iov_base = reinterpret_cast<void *>(region.begin()),
               .iov_len = data.size()};

  std::uint32_t id = get_id(t);
  if (process_vm_writev(static_cast<std::int32_t>(id), &local, 1, &remote, 1,
                        0) != static_cast<ssize_t>(data.size())) {
    throw std::system_error(
        errno, std::generic_category(),
        std::format("failed to write to memory region beginning at: {:x}",
                    region.begin()));
  }
#else
#error "only linux is supported"
#endif
}

template <thread_or_process T>
void replace_memory(const T &t, const memory_region &region,
                    std::span<const std::byte> find,
                    std::span<const std::byte> replace,
                    std::optional<std::size_t> occurrences = std::nullopt) {
  auto mem = read_memory_region(t, region);
  auto mem_span = std::span(mem);
  auto remaining_occurrences =
      occurrences.value_or(std::numeric_limits<std::size_t>::max());
  while (!mem_span.empty() && (remaining_occurrences > 0)) {
    if (const auto found = std::ranges::search(mem_span, find);
        !found.empty()) {
      const auto begin =
          std::ranges::distance(std::cbegin(mem_span), std::cbegin(found));
      std::memcpy(mem_span.data() + begin, replace.data(), replace.size());
      write_memory_region(t, region, mem);
      remaining_occurrences--;
    } else {
      break;
    }
  }
}

template <thread_or_process T, typename R>
void replace_memory(const T &t, const memory_region &region, R &&find,
                    R &&replace,
                    std::optional<std::size_t> occurrences = std::nullopt)
  requires std::ranges::contiguous_range<R>
{
  const auto find_bytes =
      reinterpret_cast<const std::byte *>(std::ranges::data(find));
  const auto replace_bytes =
      reinterpret_cast<const std::byte *>(std::ranges::data(replace));

  replace_memory(t, region,
                 {find_bytes, std::ranges::size(find) *
                                  sizeof(std::ranges::range_value_t<R>)},
                 {replace_bytes, std::ranges::size(replace) *
                                     sizeof(std::ranges::range_value_t<R>)},
                 occurrences);
}

} // namespace pp
