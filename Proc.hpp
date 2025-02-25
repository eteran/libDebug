
#ifndef PROC_HPP
#define PROC_HPP

#include <vector>

#include <sys/types.h>

std::vector<pid_t> enumerate_processes();
std::vector<pid_t> enumerate_threads(pid_t pid);

#endif
