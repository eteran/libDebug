
#include "Debugger.hpp"
#include "Process.hpp"

/**
 * @brief attaches to the process identified by <pid>
 *
 * @param pid
 */
std::shared_ptr<Process> Debugger::attach(pid_t pid) {
	process_ = std::make_shared<Process>(pid);
	return process_;
}

/**
 * @brief spawns a process and attaches to it
 *
 * @param argc
 * @param argv
 */
std::shared_ptr<Process> Debugger::spawn(int argc, char *argv[]) {
	// TODO(eteran): implement
	(void)argc;
	(void)argv;
	return process_;
}
