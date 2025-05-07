# pp

a linux-based process debugging and analysis tool that allows you to inspect, analyze, and modify running processes. it provides functionality for function hooking, memory analysis, and process inspection.

## features

- process inspection and analysis
- function hooking and modification
- memory region analysis
- function search and listing
- memory access checking
- library injection support

## commands

### process management
- `pidof <process_name>` - find pid of a process by name
- `ps` - list all running processes
- `info <pid>` - show detailed process info
- `name <pid>` - get process name from pid
- `attach <pid> [timeout_ms]` - attach debugger to process
- `threads <pid>` - show all threads and their registers
- `thread-info <pid> <tid>` - show detailed thread information

### memory operations
- `maps <pid>` - show process memory maps
- `allocate <pid> <size>` - allocate memory in process
- `chmod <pid> <address> <size> <permissions>` - change memory permissions
- `read <pid> <address> <size>` - read memory from region
- `write <pid> <address> <bytes...>` - write bytes to memory
- `search <pid> <pattern> [--string|-s]` - search for pattern in memory
- `replace <pid> <find_pattern> <replace_pattern> [occurrences] [--hex]` - find and replace pattern
- `load <pid> <address> <filename>` - load file into process memory
- `region <pid> <address>` - find memory region containing address
- `memstat <pid>` - show memory statistics of process

### function analysis
- `functions <pid> [--demangle]` - list all functions
- `find-fn <pid> <pattern> [--demangle]` - search functions by pattern
- `find-func <pid> <function_name>` - find function address
- `analyze-func <pid> <function_name>` - analyze function memory region
- `check-access <pid> <address>` - check memory access permissions
- `hook <pid> <function_name> <source_file>` - hook a function with source code

### library operations
- `inject <pid> <library_path>` - inject shared library into process

### memory region analysis
- `exec <pid>` - list executable memory regions
- `disasm <pid> <address> <size>` - disassemble memory region

## requirements

- linux operating system
- x86_64 architecture
- cmake 3.22 or higher
- c++23 compatible compiler
- capstone library

## building

```bash
mkdir build
cd build
cmake ..
make
```

## usage

after building, you can use the tool with various commands:

```bash
# list all functions in a process
./pp functions <pid>

# search for functions containing "main"
./pp find-fn <pid> main

# find exact function address
./pp find-func <pid> main

# analyze function memory region
./pp analyze-func <pid> main

# check memory access at address
./pp check-access <pid> 0x12345678
```

## notes

- the tool requires appropriate permissions to access target processes
- only supports linux x86_64 architecture
- function names can be demangled using the --demangle flag 
