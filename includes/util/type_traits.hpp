#pragma once

#include "process/process.hpp"
#include "thread/thread.hpp"
#include <type_traits>

namespace pp {
template <typename T>
concept thread_or_process =
    std::is_same_v<std::remove_reference_t<T>, thread> ||
    std::is_same_v<std::remove_reference_t<T>, process>;

template <thread_or_process T> static constexpr bool is_process(const T &) {
  return std::is_same_v<std::remove_reference_t<T>, process>;
}

template <thread_or_process T>
[[nodiscard]] constexpr std::uint32_t get_id(const T &t) {
  std::uint32_t id{0};
  if constexpr (is_process(t)) {
    id = t.pid();
  } else {
    id = t.tid();
  }
  return id;
}

} // namespace pp
