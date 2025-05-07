# PP(Process Processor) - Process Debugging and Analysis Tool

A Linux-based process debugging and analysis tool that allows you to inspect, analyze, and modify running processes. It provides functionality for function hooking, memory analysis, and process inspection.

## Features

- Process inspection and analysis
- Function hooking and modification
- Memory region analysis
- Function search and listing
- Memory access checking
- Library injection support

## Commands

### Process Analysis
- `functions <pid> [--demangle]` - List all functions in a process
- `find-fn <pid> <pattern> [--demangle]` - Search for functions by name pattern
- `find-func <pid> <function_name>` - Find function address by exact name
- `analyze-func <pid> <function_name>` - Analyze function memory region
- `check-access <pid> <address>` - Check memory access permissions at address

## Requirements

- Linux operating system
- x86_64 architecture
- CMake 3.22 or higher
- C++23 compatible compiler
- Capstone library

## Building

```bash
mkdir build
cd build
cmake ..
make
```

## Usage

After building, you can use the tool with various commands:

```bash
# List all functions in a process
./pp functions <pid>

# Search for functions containing "main"
./pp find-fn <pid> main

# Find exact function address
./pp find-func <pid> main

# Analyze function memory region
./pp analyze-func <pid> main

# Check memory access at address
./pp check-access <pid> 0x12345678
```

## Notes

- The tool requires appropriate permissions to access target processes
- Only supports Linux x86_64 architecture
- Function names can be demangled using the --demangle flag 