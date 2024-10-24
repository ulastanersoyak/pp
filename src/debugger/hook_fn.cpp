#include "debugger/debugger.hpp"
#include "debugger/registers.hpp"
#include "memory_region/permission.hpp"
#include "util/addr_to_region.hpp"

#ifdef __linux__
#include <print>
#include <sys/wait.h>
#else
#error "only linux is supported"
#endif

namespace pp {

void debugger::hook([[maybe_unused]] const function &fn) const {
#ifdef __x86_64__
#ifdef __linux__

#else
#error "only linux is supported"
#endif
#else
#error "only x86_64 architecture is supported"
#endif
}

} // namespace pp
