# pp

**process processor** — a linux tool for inspecting and modifying running processes from the outside.

pp is built from first principles on top of the primitive interfaces the kernel exposes — `ptrace`, the `/proc` filesystem, the cross-process memory system calls and the elf format — instead of wrapping an existing debugger. it can read and write another process's memory, search and patch it, list and disassemble its functions, allocate memory inside it, change page permissions, inject shared libraries and hook functions with freshly compiled code.

the full write-up is in [`docs/thesis.pdf`](docs/thesis.pdf) (latex source in [`docs/thesis.tex`](docs/thesis.tex)).

## how it works

a process may only allocate or re-protect its *own* address space. so for every modifying operation, pp makes the target do the work itself:

1. attach to the target with `ptrace` and stop its threads
2. save a thread's registers and set them up for the needed call
3. plant a small instruction stub and let the thread run it
4. catch the breakpoint at the end of the stub, read the result, then restore the original registers and code

this one mechanism powers memory allocation (`mmap`), permission changes (`mprotect`), library injection (`dlopen`) and function hooking.

## requirements

- linux on x86_64
- a c++23 compiler (recent gcc or clang)
- cmake 3.22 or newer
- pkg-config
- [capstone](https://www.capstone-engine.org/) (used by `disasm`)
- g++ at runtime (used by `hook` to compile the replacement function)

## building

```sh
mkdir build && cd build
cmake ..
cmake --build .
```

the binary is written to `build/src/pp`.

## usage

```sh
pp <command> [args...]
```

run `pp` with no arguments to print every command. addresses are given in hex; `write` bytes are hex too. most commands need permission to trace the target: run as root, or lower `kernel.yama.ptrace_scope`.

### processes and threads

| command | description |
| --- | --- |
| `ps` | list all processes |
| `pidof <process_name>` | find the pid of a process by name |
| `name <pid>` | get a process's name from its pid |
| `info <pid>` | show detailed process information |
| `memstat <pid>` | show memory statistics |
| `attach <pid> [timeout_ms]` | attach the debugger to a process |
| `threads <pid>` | show all threads and their registers |
| `thread-info <pid> <tid>` | show detailed thread information |

### memory

| command | description |
| --- | --- |
| `maps <pid>` | show memory maps |
| `exec <pid>` | list executable memory regions |
| `region <pid> <address>` | find the region containing an address |
| `check-access <pid> <address>` | check memory access at an address |
| `read <pid> <address> <size>` | read memory |
| `write <pid> <address> <bytes...>` | write bytes to memory |
| `search <pid> <pattern> [--string\|-s]` | search memory for a hex or string pattern |
| `replace <pid> <find> <replace> [occurrences] [--hex]` | find and replace a pattern |
| `allocate <pid> <size>` | allocate memory inside the process |
| `chmod <pid> <address> <size> <permissions>` | change region permissions, e.g. `rwx` |
| `load <pid> <address> <filename>` | load a file into process memory |
| `disasm <pid> <address> <size>` | disassemble a memory range |

### functions and code

| command | description |
| --- | --- |
| `functions <pid> [--demangle]` | list all functions |
| `find-fn <pid> <pattern> [--demangle]` | search functions by name pattern |
| `find-func <pid> <function_name>` | find a function's address by exact name |
| `analyze-func <pid> <function_name>` | analyze a function's memory region |
| `inject <pid> <library_path>` | inject a shared library |
| `hook <pid> <function_name> <source_file>` | replace a function with code compiled from a source file |

### examples

```sh
# find the target
pp pidof my_app

# look up a function and disassemble its first bytes
pp find-func 4242 compute
pp disasm 4242 5581a2c01189 64

# patch a string in memory
pp search 4242 "hello" --string
pp replace 4242 "hello" "howdy"

# load a library into the process
sudo pp inject 4242 /path/to/libpayload.so
```

## project layout

```
includes/   public headers
src/
  cli/            command-line parser and commands
  compiler/       compiles hook sources into position-independent code
  debugger/       ptrace attach, register control, mmap / mprotect / dlopen injection, hooking
  disassembler/   capstone wrapper
  memory_region/  region model, permissions and cross-process memory io
  process/        /proc parsing, elf symbol lookup, function resolution
  thread/         thread enumeration
  util/           helpers (demangling, elf detection, file reading)
test/       tests (currently not part of the build)
docs/       thesis (pdf and latex source)
```

## limitations

- linux x86_64 only.
- `hook` works reliably only for self-contained functions. code that calls other functions or touches globals can break, because the injected code is not relocated and stack alignment is not guaranteed. see the thesis for details.
- this is a research tool for studying the mechanisms and their limits, not a production debugger. only use it on processes you own or are allowed to inspect.

## license

gpl-3.0. see the license file in the repository root.
