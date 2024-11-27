#include "cli/parser.hpp"
#include "debugger/debugger.hpp"
#include "debugger/registers.hpp"
#include "memory_region/memio.hpp"
#include "memory_region/permission.hpp"
#include "process/process.hpp"
#include "util/addr_to_region.hpp"
#include "util/demangle.hpp"
#include "util/read_file.hpp"

namespace pp {
void cli_parser::add_command(command cmd) {
  this->commands[cmd.name] = std::move(cmd);
}

void cli_parser::print_usage() const {
  std::println("usage: pp <command> [args...]\n");
  std::println("available commands:");
  for (const auto &[name, cmd] : commands) {
    std::println("  {:<15} {}", name, cmd.description);
    if (!cmd.args.empty()) {
      std::string args_str;
      for (const auto &arg : cmd.args) {
        if (!args_str.empty())
          args_str += " ";
        args_str += std::string{arg};
      }
      std::println("    arguments: {}", args_str);
    }
  }
}

void load_commands(cli_parser &parser) {
  parser.add_command({.name = "pidof",
                      .description = "returns pid of the given process",
                      .args = {"<process_name>"},
                      .handler = [](std::span<const std::string_view> args)
                          -> std::expected<void, std::string> {
                        if (args.empty()) {
                          return std::unexpected{"Process name required"};
                        }

                        try {
                          const auto processes = pp::find_process(args[0]);
                          // Print all matching PIDs
                          for (const auto &proc : processes) {
                            std::println("{}", proc.pid());
                          }
                          return {};
                        } catch (const std::invalid_argument &e) {
                          return std::unexpected{std::string{e.what()}};
                        } catch (const std::exception &e) {
                          return std::unexpected{std::format(
                              "Error finding process: {}", e.what())};
                        }
                      }});
  parser.add_command(
      {.name = "info",
       .description = "show detailed process information",
       .args = {"<pid>"},
       .handler = [](std::span<const std::string_view> args)
           -> std::expected<void, std::string> {
         if (args.empty()) {
           return std::unexpected{"PID required"};
         }

         try {
           const auto pid =
               static_cast<std::uint32_t>(std::stoul(std::string{args[0]}));
           pp::process proc{pid};

           std::println("Process Information:");
           std::println("  PID: {}", proc.pid());
           std::println("  Name: {}", proc.name());
           std::println("  Base Address: 0x{:x}", proc.base_addr());
           std::println("  Memory Usage: {} bytes", proc.mem_usage());
           std::println("  Executable: {}", proc.exe_path());

           const auto threads = proc.threads();
           std::println("  Threads: {}", threads.size());

           return {};
         } catch (const std::exception &e) {
           return std::unexpected{
               std::format("Error getting process info: {}", e.what())};
         }
       }});

  parser.add_command(
      {.name = "maps",
       .description = "show process memory maps",
       .args = {"<pid>"},
       .handler = [](std::span<const std::string_view> args)
           -> std::expected<void, std::string> {
         if (args.empty()) {
           return std::unexpected{"PID required"};
         }

         try {
           const auto pid =
               static_cast<std::uint32_t>(std::stoul(std::string{args[0]}));
           pp::process proc{pid};

           std::println("Memory regions for process {}:", pid);
           std::println("ADDRESS RANGE                SIZE       PERMISSIONS   "
                        "      NAME");
           for (const auto &region : proc.memory_regions()) {
             // Format size in readable format
             std::string size_str;
             auto size = region.size();
             if (size >= 1024 * 1024 * 1024) {
               size_str = std::format("{:.1f}G", static_cast<double>(size) /
                                                     (1024.0 * 1024 * 1024));
             } else if (size >= 1024 * 1024) {
               size_str = std::format("{:.1f}M", static_cast<double>(size) /
                                                     (1024.0 * 1024));
             } else if (size >= 1024) {
               size_str =
                   std::format("{:.1f}K", static_cast<double>(size) / 1024.0);
             } else {
               size_str = std::format("{}B", size);
             }

             // Print region info with permissions string
             std::println("0x{:012x}-0x{:012x} {:>8} {:<16} {}", region.begin(),
                          region.begin() + region.size(), size_str,
                          pp::permission_to_str(region.permissions()),
                          region.name().value_or("[anonymous]"));
           }

           return {};
         } catch (const std::exception &e) {
           return std::unexpected{
               std::format("Error getting memory maps: {}", e.what())};
         }
       }});

  parser.add_command(
      {.name = "allocate",
       .description = "allocate memory in a process",
       .args = {"<pid>", "<size>"},
       .handler = [](std::span<const std::string_view> args)
           -> std::expected<void, std::string> {
         if (args.size() < 2) {
           return std::unexpected{"Usage: allocate <pid> <size>"};
         }

         try {
           const auto pid =
               static_cast<std::uint32_t>(std::stoul(std::string{args[0]}));
           const auto size = std::stoull(std::string{args[1]});
           pp::process proc{pid};
           pp::debugger dbg{proc};
           const auto region = dbg.allocate_memory(size);

           std::println("Successfully allocated memory:");
           std::println("  Address: 0x{:x}", region.begin());
           std::println("  Size: {} bytes", region.size());
           std::println("  Permissions: {}",
                        pp::permission_to_str(region.permissions()));

           return {};
         } catch (const std::exception &e) {
           return std::unexpected{
               std::format("Error allocating memory: {}", e.what())};
         }
       }});

  parser.add_command(
      {.name = "ps",
       .description = "list all processes",
       .args = {},
       .handler = []([[maybe_unused]] std::span<const std::string_view> args)
           -> std::expected<void, std::string> {
         try {
           const auto pids = pp::get_all_pids();
           std::println("PID\tNAME");
           for (const auto pid : pids) {
             try {
               pp::process proc{pid};
               std::println("{}\t{}", pid, proc.name());
             } catch (...) {
               continue;
             }
           }
           return {};
         } catch (const std::exception &e) {
           return std::unexpected{
               std::format("Error listing processes: {}", e.what())};
         }
       }});

  // Attach to a process command
  parser.add_command(
      {.name = "attach",
       .description = "attach debugger to a process",
       .args = {"<pid>", "[timeout_ms]"},
       .handler = [](std::span<const std::string_view> args)
           -> std::expected<void, std::string> {
         if (args.empty()) {
           return std::unexpected{"PID required"};
         }

         try {
           const auto pid =
               static_cast<std::uint32_t>(std::stoul(std::string{args[0]}));
           std::optional<std::size_t> timeout;

           if (args.size() > 1) {
             timeout = std::stoull(std::string{args[1]});
           }

           pp::process proc{pid};
           pp::debugger dbg{proc, timeout};

           std::println("Successfully attached to process {}:", pid);
           std::println("  Main thread: {}", dbg.main_thread().tid());

           // Show current registers of main thread
           auto regs = dbg.get_regs(dbg.main_thread());
           std::println("\nMain thread registers:");
           std::stringstream ss;
           ss << regs.regs;
           std::println("{}", ss.str());

           return {};
         } catch (const std::exception &e) {
           return std::unexpected{
               std::format("Error attaching debugger: {}", e.what())};
         }
       }});

  // Change memory region permissions
  parser.add_command(
      {.name = "chmod",
       .description = "change memory region permissions",
       .args = {"<pid>", "<address>", "<size>", "<permissions>"},
       .handler = [](std::span<const std::string_view> args)
           -> std::expected<void, std::string> {
         if (args.size() < 4) {
           return std::unexpected{
               "Usage: chmod <pid> <address> <size> <permissions>"};
         }

         try {
           const auto pid =
               static_cast<std::uint32_t>(std::stoul(std::string{args[0]}));
           const auto addr = std::stoull(std::string{args[1]}, nullptr, 16);
           const auto size = std::stoull(std::string{args[2]});
           const auto perm_str = std::string{args[3]};

           // Parse permissions
           pp::permission perm = pp::permission::NO_PERMISSION;
           if (perm_str.find('r') != std::string::npos)
             perm |= pp::permission::READ;
           if (perm_str.find('w') != std::string::npos)
             perm |= pp::permission::WRITE;
           if (perm_str.find('x') != std::string::npos)
             perm |= pp::permission::EXECUTE;

           pp::process proc{pid};
           pp::debugger dbg{proc};

           pp::memory_region region{addr, size, perm};
           dbg.change_region_permissions(region, perm);

           std::println("Successfully changed permissions:");
           std::println("  Region: 0x{:x}-0x{:x}", region.begin(),
                        region.begin() + region.size());
           std::println("  New permissions: {}", permission_to_str(perm));

           return {};
         } catch (const std::exception &e) {
           return std::unexpected{
               std::format("Error changing permissions: {}", e.what())};
         }
       }});

  // Load library into process
  parser.add_command(
      {.name = "inject",
       .description = "inject shared library into process",
       .args = {"<pid>", "<library_path>"},
       .handler = [](std::span<const std::string_view> args)
           -> std::expected<void, std::string> {
         if (args.size() < 2) {
           return std::unexpected{"Usage: inject <pid> <library_path>"};
         }

         try {
           const auto pid =
               static_cast<std::uint32_t>(std::stoul(std::string{args[0]}));
           const auto lib_path = std::string{args[1]};

           pp::process proc{pid};
           pp::debugger dbg{proc};

           dbg.load_library(lib_path);

           std::println("Successfully injected library:");
           std::println("  Process: {}", pid);
           std::println("  Library: {}", lib_path);

           return {};
         } catch (const std::exception &e) {
           return std::unexpected{
               std::format("Error injecting library: {}", e.what())};
         }
       }});

  // Read memory command
  parser.add_command(
      {.name = "read",
       .description = "read memory from region",
       .args = {"<pid>", "<address>", "<size>"},
       .handler = [](std::span<const std::string_view> args)
           -> std::expected<void, std::string> {
         if (args.size() < 3) {
           return std::unexpected{"Usage: read <pid> <address> <size>"};
         }

         try {
           const auto pid =
               static_cast<std::uint32_t>(std::stoul(std::string{args[0]}));
           const auto addr = std::stoull(std::string{args[1]}, nullptr, 16);
           const auto size = std::stoull(std::string{args[2]});

           pp::process proc{pid};
           pp::memory_region region{addr, size, pp::permission::READ};
           const auto memory = pp::read_memory_region(proc, region);

           // Print in hex + ASCII format
           std::println("Memory at 0x{:x} (size: {} bytes):", addr, size);
           for (size_t i = 0; i < memory.size(); i += 16) {
             // Address
             std::print("0x{:016x}  ", addr + i);

             // Hex values
             for (size_t j = i; j < std::min(i + 16, memory.size()); ++j) {
               std::print("{:02x} ", static_cast<unsigned char>(memory[j]));
             }

             // Padding for incomplete lines
             if (memory.size() - i < 16) {
               for (size_t j = 0; j < 16 - (memory.size() - i); ++j) {
                 std::print("   ");
               }
             }

             // ASCII representation
             std::print(" |");
             for (size_t j = i; j < std::min(i + 16, memory.size()); ++j) {
               char c = static_cast<char>(memory[j]);
               std::print("{}", std::isprint(c) ? c : '.');
             }
             std::println("|");
           }

           return {};
         } catch (const std::exception &e) {
           return std::unexpected{
               std::format("Error reading memory: {}", e.what())};
         }
       }});

  // Write memory command
  parser.add_command(
      {.name = "write",
       .description = "write bytes to memory",
       .args = {"<pid>", "<address>", "<bytes...>"},
       .handler = [](std::span<const std::string_view> args)
           -> std::expected<void, std::string> {
         if (args.size() < 3) {
           return std::unexpected{
               "Usage: write <pid> <address> <byte1> [byte2...]"};
         }

         try {
           const auto pid =
               static_cast<std::uint32_t>(std::stoul(std::string{args[0]}));
           const auto addr = std::stoull(std::string{args[1]}, nullptr, 16);

           std::vector<std::byte> bytes;
           for (size_t i = 2; i < args.size(); ++i) {
             bytes.push_back(static_cast<std::byte>(
                 std::stoul(std::string{args[i]}, nullptr, 16)));
           }

           pp::process proc{pid};
           pp::memory_region region{addr, bytes.size(),
                                    pp::permission::READ |
                                        pp::permission::WRITE};
           pp::write_memory_region(proc, region, bytes);

           std::println("Successfully wrote {} bytes to 0x{:x}", bytes.size(),
                        addr);
           return {};
         } catch (const std::exception &e) {
           return std::unexpected{
               std::format("Error writing memory: {}", e.what())};
         }
       }});

  parser.add_command(
      {.name = "replace",
       .description = "find and replace pattern in process memory",
       .args = {"<pid>", "<find_pattern>", "<replace_pattern>", "[occurrences]",
                "[--hex]"},
       .handler = [](std::span<const std::string_view> args)
           -> std::expected<void, std::string> {
         if (args.size() < 3) {
           return std::unexpected{"Usage: replace <pid> <find_pattern> "
                                  "<replace_pattern> [occurrences] [--hex]"};
         }

         try {
           const auto pid =
               static_cast<std::uint32_t>(std::stoul(std::string{args[0]}));
           const bool is_hex = args.size() > 3 && args[3] == "--hex";

           // Convert pattern to bytes
           auto to_bytes =
               [is_hex](std::string_view pattern) -> std::vector<std::byte> {
             if (is_hex) {
               std::vector<std::byte> bytes;
               for (size_t i = 0; i < pattern.length(); i += 2) {
                 bytes.push_back(static_cast<std::byte>(std::stoul(
                     std::string{pattern.substr(i, 2)}, nullptr, 16)));
               }
               return bytes;
             } else {
               std::vector<std::byte> bytes(pattern.size());
               std::transform(pattern.begin(), pattern.end(), bytes.begin(),
                              [](char c) { return std::byte(c); });
               return bytes;
             }
           };

           const auto find_pattern = to_bytes(args[1]);
           auto replace_pattern = to_bytes(args[2]);

           if (replace_pattern.size() < find_pattern.size()) {
             const auto diff = find_pattern.size() - replace_pattern.size();
             for (std::size_t i = 0; i < diff; ++i) {
               replace_pattern.emplace_back(std::byte(' '));
             }
           }

           std::optional<std::size_t> occurrences;
           if (args.size() > 3 && args[3] != "--hex") {
             occurrences = std::stoull(std::string{args[3]});
           }

           pp::process proc{pid};
           size_t total_replacements = 0;

           // Search through all readable and writable regions
           for (const auto &region : proc.memory_regions()) {
             if (region.has_permissions(pp::permission::READ |
                                        pp::permission::WRITE)) {
               try {
                 pp::replace_memory(proc, region, find_pattern, replace_pattern,
                                    occurrences);
                 if (occurrences) {
                   total_replacements += *occurrences;
                 }
               } catch (...) {
                 // Skip regions we can't access
                 continue;
               }
             }
           }

           std::println("Successfully replaced pattern in process {}", pid);
           if (occurrences) {
             std::println("Replacements made: {}", total_replacements);
           }
           return {};
         } catch (const std::exception &e) {
           return std::unexpected{
               std::format("Error replacing pattern: {}", e.what())};
         }
       }});

  // Search memory command
  parser.add_command(
      {.name = "search",
       .description = "search for pattern in memory regions",
       .args = {"<pid>", "<pattern>"},
       .handler = [](std::span<const std::string_view> args)
           -> std::expected<void, std::string> {
         if (args.size() < 2) {
           return std::unexpected{"Usage: search <pid> <pattern>"};
         }

         try {
           const auto pid =
               static_cast<std::uint32_t>(std::stoul(std::string{args[0]}));

           // Convert hex string pattern to bytes
           auto hex_to_bytes = [](std::string_view hex) {
             std::vector<std::byte> bytes;
             for (size_t i = 0; i < hex.length(); i += 2) {
               bytes.push_back(static_cast<std::byte>(
                   std::stoul(std::string{hex.substr(i, 2)}, nullptr, 16)));
             }
             return bytes;
           };

           const auto pattern = hex_to_bytes(args[1]);
           pp::process proc{pid};

           std::println("Searching for pattern in process {} ({}):", pid,
                        proc.name());
           for (const auto &region : proc.memory_regions()) {
             if (region.has_permissions(pp::permission::READ)) {
               try {
                 const auto memory = pp::read_memory_region(proc, region);

                 // Search for pattern in this region
                 auto it = std::search(memory.begin(), memory.end(),
                                       pattern.begin(), pattern.end());

                 while (it != memory.end()) {
                   const auto offset = std::distance(memory.begin(), it);
                   std::println("Found at: 0x{:x}",
                                region.begin() +
                                    static_cast<std::size_t>(offset));

                   // Continue searching
                   it = std::search(it + 1, memory.end(), pattern.begin(),
                                    pattern.end());
                 }
               } catch (...) {
                 // Skip regions we can't read
                 continue;
               }
             }
           }

           return {};
         } catch (const std::exception &e) {
           return std::unexpected{
               std::format("Error searching memory: {}", e.what())};
         }
       }});

  // Show all threads command
  parser.add_command(
      {.name = "threads",
       .description = "show all threads and their registers",
       .args = {"<pid>"},
       .handler = [](std::span<const std::string_view> args)
           -> std::expected<void, std::string> {
         if (args.empty()) {
           return std::unexpected{"PID required"};
         }

         try {
           const auto pid =
               static_cast<std::uint32_t>(std::stoul(std::string{args[0]}));
           pp::process proc{pid};
           pp::debugger dbg{proc};

           const auto threads = proc.threads();
           std::println("Threads for process {} ({}):", pid, proc.name());
           for (const auto &thread : threads) {
             std::println("\nThread ID: {}", thread.tid());
             const auto regs = dbg.get_regs(thread);
             std::stringstream ss;
             ss << regs.regs;
             std::println("{}", ss.str());
           }

           return {};
         } catch (const std::exception &e) {
           return std::unexpected{
               std::format("Error listing threads: {}", e.what())};
         }
       }});

  // Dump memory to file
  // parser.add_command(
  //     {.name = "dump",
  //      .description = "dump memory region to file",
  //      .args = {"<pid>", "<address>", "<size>", "<filename>"},
  //      .handler = [](std::span<const std::string_view> args)
  //          -> std::expected<void, std::string> {
  //        if (args.size() < 4) {
  //          return std::unexpected{
  //              "Usage: dump <pid> <address> <size> <filename>"};
  //        }
  //
  //        try {
  //          const auto pid =
  //              static_cast<std::uint32_t>(std::stoul(std::string{args[0]}));
  //          const auto addr = std::stoull(std::string{args[1]}, nullptr, 16);
  //          const auto size = std::stoull(std::string{args[2]});
  //          const auto filename = std::string{args[3]};
  //
  //          pp::process proc{pid};
  //          pp::memory_region region{addr, size, pp::permission::READ};
  //          const auto memory = pp::read_memory_region(proc, region);
  //
  //          std::basic_ofstream<char> file(filename.c_str(),
  //          std::ios::binary); if (!file) {
  //            return std::unexpected{
  //                std::format("Failed to open file: {}", filename)};
  //          }
  //
  //          file.write(reinterpret_cast<const char *>(memory.data()),
  //                     memory.size());
  //          std::println("Successfully dumped {} bytes to {}", size,
  //          filename); return {};
  //        } catch (const std::exception &e) {
  //          return std::unexpected{
  //              std::format("Error dumping memory: {}", e.what())};
  //        }
  //      }});

  // Load file into memory
  parser.add_command(
      {.name = "load",
       .description = "load file into process memory",
       .args = {"<pid>", "<address>", "<filename>"},
       .handler = [](std::span<const std::string_view> args)
           -> std::expected<void, std::string> {
         if (args.size() < 3) {
           return std::unexpected{"Usage: load <pid> <address> <filename>"};
         }

         try {
           const auto pid =
               static_cast<std::uint32_t>(std::stoul(std::string{args[0]}));
           const auto addr = std::stoull(std::string{args[1]}, nullptr, 16);
           const auto filename = std::string{args[2]};

           // Read file using your utility
           const auto file_content = pp::read_file(filename);
           std::vector<std::byte> buffer(file_content.size());
           std::memcpy(buffer.data(), file_content.data(), file_content.size());

           pp::process proc{pid};
           pp::memory_region region{addr, buffer.size(),
                                    pp::permission::READ |
                                        pp::permission::WRITE};
           pp::write_memory_region(proc, region, buffer);

           std::println("Successfully loaded {} bytes from {} to 0x{:x}",
                        buffer.size(), filename, addr);
           return {};
         } catch (const std::exception &e) {
           return std::unexpected{
               std::format("Error loading file: {}", e.what())};
         }
       }});

  // Find executable memory regions
  parser.add_command(
      {.name = "exec",
       .description = "list executable memory regions",
       .args = {"<pid>"},
       .handler = [](std::span<const std::string_view> args)
           -> std::expected<void, std::string> {
         if (args.empty()) {
           return std::unexpected{"PID required"};
         }

         try {
           const auto pid =
               static_cast<std::uint32_t>(std::stoul(std::string{args[0]}));
           pp::process proc{pid};

           std::println("Executable regions for process {} ({}):", pid,
                        proc.name());
           std::println("ADDRESS RANGE                SIZE       PERMISSIONS   "
                        "      NAME");

           for (const auto &region : proc.memory_regions()) {
             if (region.has_permissions(pp::permission::EXECUTE)) {
               // Format size
               std::string size_str;
               auto size = region.size();
               if (size >= 1024 * 1024 * 1024) {
                 size_str = std::format("{:.1f}G", static_cast<double>(size) /
                                                       (1024.0 * 1024 * 1024));
               } else if (size >= 1024 * 1024) {
                 size_str = std::format("{:.1f}M", static_cast<double>(size) /
                                                       (1024.0 * 1024));
               } else if (size >= 1024) {
                 size_str =
                     std::format("{:.1f}K", static_cast<double>(size) / 1024.0);
               } else {
                 size_str = std::format("{}B", size);
               }

               std::println("0x{:012x}-0x{:012x} {:>8} {:<16} {}",
                            region.begin(), region.begin() + region.size(),
                            size_str,
                            pp::permission_to_str(region.permissions()),
                            region.name().value_or("[anonymous]"));
             }
           }

           return {};
         } catch (const std::exception &e) {
           return std::unexpected{
               std::format("Error listing executable regions: {}", e.what())};
         }
       }});

  // Find region containing address
  parser.add_command(
      {.name = "region",
       .description = "find memory region containing address",
       .args = {"<pid>", "<address>"},
       .handler = [](std::span<const std::string_view> args)
           -> std::expected<void, std::string> {
         if (args.size() < 2) {
           return std::unexpected{"Usage: region <pid> <address>"};
         }

         const auto pid =
             static_cast<std::uint32_t>(std::stoul(std::string{args[0]}));
         const auto addr = std::stoull(std::string{args[1]}, nullptr, 16);

         try {

           pp::process proc{pid};
           const auto region = pp::addr_to_region(proc, addr);

           std::println("Memory region containing 0x{:x}:", addr);
           std::println("  Start: 0x{:x}", region.begin());
           std::println("  End: 0x{:x}", region.begin() + region.size());
           std::println("  Size: {} bytes", region.size());
           std::println("  Permissions: {}",
                        pp::permission_to_str(region.permissions()));
           if (auto name = region.name()) {
             std::println("  Name: {}", *name);
           }
           std::println("  Offset in region: 0x{:x}", addr - region.begin());

           return {};
         } catch (const std::invalid_argument &) {
           return std::unexpected{
               std::format("No region found containing address 0x{:x}", addr)};
         } catch (const std::exception &e) {
           return std::unexpected{
               std::format("Error finding region: {}", e.what())};
         }
       }});

  // List process functions command
  parser.add_command(
      {.name = "functions",
       .description =
           "list all functions in a process (with optional demangling)",
       .args = {"<pid>", "[--demangle]"},
       .handler = [](std::span<const std::string_view> args)
           -> std::expected<void, std::string> {
         if (args.empty()) {
           return std::unexpected{"Usage: functions <pid> [--demangle]"};
         }

         try {
           const auto pid =
               static_cast<std::uint32_t>(std::stoul(std::string{args[0]}));
           const bool should_demangle =
               args.size() > 1 && args[1] == "--demangle";

           pp::process proc{pid};
           const auto functions = proc.functions();

           std::println("Functions in process {} ({}):", pid, proc.name());
           std::println("ADDRESS          NAME");

           for (const auto &func : functions) {
             const auto name = should_demangle ? pp::demangle(func.name)
                                               : std::string{func.name};
             std::println("0x{:012x}  {}", func.address, name);
           }

           std::println("\nTotal functions found: {}", functions.size());

           return {};
         } catch (const std::exception &e) {
           return std::unexpected{
               std::format("Error listing functions: {}", e.what())};
         }
       }});

  // Search functions command
  parser.add_command(
      {.name = "find-fn",
       .description = "search for functions by name pattern",
       .args = {"<pid>", "<pattern>", "[--demangle]"},
       .handler = [](std::span<const std::string_view> args)
           -> std::expected<void, std::string> {
         if (args.size() < 2) {
           return std::unexpected{
               "Usage: find-fn <pid> <pattern> [--demangle]"};
         }

         try {
           const auto pid =
               static_cast<std::uint32_t>(std::stoul(std::string{args[0]}));
           const auto pattern = std::string{args[1]};
           const bool should_demangle =
               args.size() > 2 && args[2] == "--demangle";

           pp::process proc{pid};
           const auto functions = proc.functions();

           std::println(
               "Searching for functions matching '{}' in process {} ({}):",
               pattern, pid, proc.name());
           std::println("ADDRESS          NAME");

           size_t matches = 0;
           for (const auto &func : functions) {
             const auto name = should_demangle ? pp::demangle(func.name)
                                               : std::string{func.name};

             if (name.find(pattern) != std::string::npos) {
               std::println("0x{:012x}  {}", func.address, name);
               matches++;
             }
           }

           std::println("\nFound {} matching functions", matches);

           return {};
         } catch (const std::exception &e) {
           return std::unexpected{
               std::format("Error searching functions: {}", e.what())};
         }
       }});

  // Find function command
  parser.add_command(
      {.name = "find-func",
       .description = "find function address by name",
       .args = {"<pid>", "<function_name>"},
       .handler = [](std::span<const std::string_view> args)
           -> std::expected<void, std::string> {
         if (args.size() < 2) {
           return std::unexpected{"usage: find-func <pid> <function_name>"};
         }

         try {
           const auto pid =
               static_cast<std::uint32_t>(std::stoul(std::string{args[0]}));
           const auto func_name = std::string{args[1]};

           pp::process proc{pid};
           auto func_addr = proc.func_addr(func_name);

           if (!func_addr) {
             return std::unexpected{
                 std::format("function '{}' not found", func_name)};
           }

           std::println("found function '{}' at 0x{:x}", func_name, *func_addr);
           return {};
         } catch (const std::exception &e) {
           return std::unexpected{
               std::format("error finding function: {}", e.what())};
         }
       }});

  parser.add_command({.name = "name",
                      .description = "get process name from PID",
                      .args = {"<pid>"},
                      .handler = [](std::span<const std::string_view> args)
                          -> std::expected<void, std::string> {
                        if (args.empty()) {
                          return std::unexpected{"PID required"};
                        }

                        try {
                          const auto pid = static_cast<std::uint32_t>(
                              std::stoul(std::string{args[0]}));
                          pp::process proc{pid};
                          std::println("{}", proc.name());
                          return {};
                        } catch (const std::exception &e) {
                          return std::unexpected{std::format(
                              "Error getting process name: {}", e.what())};
                        }
                      }});

  parser.add_command(
      {.name = "hook",
       .description = "hook a function with source code",
       .args = {"<pid>", "<function_name>", "<source_file>"},
       .handler = [](std::span<const std::string_view> args)
           -> std::expected<void, std::string> {
         if (args.size() < 3) {
           return std::unexpected{
               "Usage: hook <pid> <function_name> <source_file>"};
         }

         try {
           const auto pid =
               static_cast<std::uint32_t>(std::stoul(std::string{args[0]}));
           const auto function_name = std::string{args[1]};
           const auto source_path = std::string{args[2]};

           pp::process proc{pid};
           auto func_addr = proc.func_addr(function_name);
           if (!func_addr) {
             return std::unexpected{
                 std::format("Function '{}' not found", function_name)};
           }

           pp::debugger dbg{proc};
           pp::function target{function_name, *func_addr};
           dbg.hook(target, source_path);

           std::println("Successfully hooked function '{}'", function_name);
           std::println("  at address: 0x{:x}", *func_addr);
           std::println("  with source: {}", source_path);

           return {};
         } catch (const std::exception &e) {
           return std::unexpected{
               std::format("Error hooking function: {}", e.what())};
         }
       }});

  // Show memory usage statistics
  parser.add_command(
      {.name = "memstat",
       .description = "show memory statistics of process",
       .args = {"<pid>"},
       .handler = [](std::span<const std::string_view> args)
           -> std::expected<void, std::string> {
         if (args.empty())
           return std::unexpected{"PID required"};
         try {
           const auto pid =
               static_cast<std::uint32_t>(std::stoul(std::string{args[0]}));
           pp::process proc{pid};

           size_t total_memory = 0;
           size_t executable_memory = 0;
           size_t writable_memory = 0;
           size_t anonymous_regions = 0;

           for (const auto &region : proc.memory_regions()) {
             total_memory += region.size();
             if (region.has_permissions(pp::permission::EXECUTE))
               executable_memory += region.size();
             if (region.has_permissions(pp::permission::WRITE))
               writable_memory += region.size();
             if (!region.name())
               anonymous_regions++;
           }

           std::println("Memory Statistics for {} ({}):", pid, proc.name());
           std::println("  Total Memory: {} bytes", total_memory);
           std::println("  Executable Memory: {} bytes", executable_memory);
           std::println("  Writable Memory: {} bytes", writable_memory);
           std::println("  Anonymous Regions: {}", anonymous_regions);

           return {};
         } catch (const std::exception &e) {
           return std::unexpected{
               std::format("Error getting memory stats: {}", e.what())};
         }
       }});

  parser.add_command(
      {.name = "thread-info",
       .description = "show detailed thread information",
       .args = {"<pid>", "<tid>"},
       .handler = [](std::span<const std::string_view> args)
           -> std::expected<void, std::string> {
         if (args.size() < 2)
           return std::unexpected{"Usage: thread-info <pid> <tid>"};
         try {
           const auto pid =
               static_cast<std::uint32_t>(std::stoul(std::string{args[0]}));
           const auto tid =
               static_cast<std::uint32_t>(std::stoul(std::string{args[1]}));

           pp::process proc{pid};
           pp::debugger dbg{proc};
           pp::thread thread{pid, tid};

           const auto regs = dbg.get_regs(thread);

           std::println("Thread {} Information:", tid);
           std::println("  Process: {} ({})", pid, proc.name());
           std::stringstream ss;
           ss << regs.regs;
           std::println("  Registers:\n{}", ss.str());

           return {};
         } catch (const std::exception &e) {
           return std::unexpected{
               std::format("Error getting thread info: {}", e.what())};
         }
       }});

  parser.add_command(
      {.name = "analyze-func",
       .description = "analyze function memory region",
       .args = {"<pid>", "<function_name>"},
       .handler = [](std::span<const std::string_view> args)
           -> std::expected<void, std::string> {
         if (args.size() < 2)
           return std::unexpected{"Usage: analyze-func <pid> <function_name>"};
         try {
           const auto pid =
               static_cast<std::uint32_t>(std::stoul(std::string{args[0]}));
           const auto func_name = std::string{args[1]};

           pp::process proc{pid};
           auto func_addr = proc.func_addr(func_name);
           if (!func_addr) {
             return std::unexpected{
                 std::format("Function '{}' not found", func_name)};
           }

           const auto region = pp::addr_to_region(proc, *func_addr);
           const auto bytes = pp::read_memory_region(proc, region);

           std::println("Function Analysis for '{}':", func_name);
           std::println("  Address: 0x{:x}", *func_addr);
           std::println("  Region: 0x{:x}-0x{:x}", region.begin(),
                        region.begin() + region.size());
           std::println("  Permissions: {}",
                        pp::permission_to_str(region.permissions()));
           if (auto name = region.name()) {
             std::println("  Module: {}", *name);
           }

           // Show first bytes of the function
           std::println("\nFirst 32 bytes:");
           const auto offset = *func_addr - region.begin();
           for (size_t i = 0; i < 32 && (offset + i) < bytes.size(); ++i) {
             std::print("{:02x} ",
                        static_cast<unsigned char>(bytes[offset + i]));
             if ((i + 1) % 16 == 0)
               std::println("");
           }
           std::println("");

           return {};
         } catch (const std::exception &e) {
           return std::unexpected{
               std::format("Error analyzing function: {}", e.what())};
         }
       }});

  parser.add_command(
      {.name = "check-access",
       .description = "check memory access at address",
       .args = {"<pid>", "<address>"},
       .handler = [](std::span<const std::string_view> args)
           -> std::expected<void, std::string> {
         if (args.size() < 2)
           return std::unexpected{"Usage: check-access <pid> <address>"};
         try {
           const auto pid =
               static_cast<std::uint32_t>(std::stoul(std::string{args[0]}));
           const auto addr = std::stoull(std::string{args[1]}, nullptr, 16);

           pp::process proc{pid};
           const auto region = pp::addr_to_region(proc, addr);

           std::println("Memory Access at 0x{:x}:", addr);
           std::println("  Readable: {}",
                        region.has_permissions(pp::permission::READ) ? "Yes"
                                                                     : "No");
           std::println("  Writable: {}",
                        region.has_permissions(pp::permission::WRITE) ? "Yes"
                                                                      : "No");
           std::println("  Executable: {}",
                        region.has_permissions(pp::permission::EXECUTE) ? "Yes"
                                                                        : "No");

           return {};
         } catch (const std::exception &e) {
           return std::unexpected{
               std::format("Error checking access: {}", e.what())};
         }
       }});
}

} // namespace pp
