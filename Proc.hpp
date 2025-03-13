
#ifndef PROC_HPP_
#define PROC_HPP_

#include <cstdint>
#include <vector>

#include <sys/types.h>

class Region;

[[nodiscard]] std::vector<pid_t> enumerate_processes();
[[nodiscard]] std::vector<pid_t> enumerate_threads(pid_t pid);
[[nodiscard]] std::vector<Region> enumerate_regions(pid_t pid);
[[nodiscard]] uint64_t hash_regions(pid_t pid);

#endif
