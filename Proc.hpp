
#ifndef PROC_HPP_
#define PROC_HPP_

#include <cstdint>
#include <vector>

#include <sys/types.h>

class Region;

std::vector<pid_t> enumerate_processes();
std::vector<pid_t> enumerate_threads(pid_t pid);
std::vector<Region> enumerate_regions(pid_t pid);
uint64_t hash_regions(pid_t pid);

#endif
