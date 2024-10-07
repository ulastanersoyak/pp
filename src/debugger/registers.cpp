#include "debugger/registers.hpp"

std::ostream &operator<<(std::ostream &os, const user_regs_struct &r) {
  os << "r15: " << std::hex << r.r15 << "\n";
  os << "r14: " << r.r14 << "\n";
  os << "r13: " << r.r13 << "\n";
  os << "r12: " << r.r12 << "\n";
  os << "rbp: " << r.rbp << "\n";
  os << "rbx: " << r.rbx << "\n";
  os << "r11: " << r.r11 << "\n";
  os << "r10: " << r.r10 << "\n";
  os << "r9:  " << r.r9 << "\n";
  os << "r8:  " << r.r8 << "\n";
  os << "rax: " << r.rax << "\n";
  os << "rcx: " << r.rcx << "\n";
  os << "rdx: " << r.rdx << "\n";
  os << "rsi: " << r.rsi << "\n";
  os << "rdi: " << r.rdi << "\n";
  os << "orig_rax: " << r.orig_rax << "\n";
  os << "rip: " << r.rip << "\n";
  os << "cs: " << r.cs << "\n";
  os << "eflags: " << r.eflags << "\n";
  os << "rsp: " << r.rsp << "\n";
  os << "ss: " << r.ss << "\n";
  os << "fs_base: " << r.fs_base << "\n";
  os << "gs_base: " << r.gs_base << "\n";
  os << "ds: " << r.ds << "\n";
  os << "es: " << r.es << "\n";
  os << "fs: " << r.fs << "\n";
  os << "gs: " << r.gs << "\n";

  return os;
}

std::ostream &operator<<(std::ostream &os, const user_fpregs_struct &r) {
  os << "\nfloating point registers:\n";
  os << "cwd: " << r.cwd << "\n";
  os << "swd: " << r.swd << "\n";
  os << "ftw: " << r.ftw << "\n";
  os << "fop: " << r.fop << "\n";
  os << "rip: " << r.rip << "\n";
  os << "rdp: " << r.rdp << "\n";
  os << "mxcsr: " << r.mxcsr << "\n";
  os << "mxcr_mask: " << r.mxcr_mask << "\n";
  return os;
}
