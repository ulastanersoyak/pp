#pragma once

#ifdef __linux__
#include <sys/user.h>

#endif
#include <iostream>

struct registers {
#ifdef __linux__
  user_regs_struct regs{};
  user_fpregs_struct fp_regs{};
#endif
};
std::ostream &operator<<(std::ostream &os, const user_regs_struct &r);
std::ostream &operator<<(std::ostream &os, const user_fpregs_struct &r);
