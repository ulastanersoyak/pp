#pragma once

#include <iostream>

#ifdef __linux__
#include <sys/user.h>
#endif

struct registers {
#ifdef __x86_64__
#ifdef __linux__
  user_regs_struct regs{};
#endif
#else
#error "only x86_64 architecture registers are supported"
#endif
};

std::ostream &operator<<(std::ostream &os, const user_regs_struct &r);
