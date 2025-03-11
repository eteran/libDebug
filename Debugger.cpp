
#include "Debugger.hpp"
#include "Process.hpp"
#include "Thread.hpp"

#include <csignal>
#include <cstdio>
#include <cstdlib>

#include <sys/personality.h>
#include <sys/ptrace.h>

/**
 * @brief Construct a new Debugger object
 *
 */
Debugger::Debugger() {

	sigset_t mask;
	::sigemptyset(&mask);
	::sigaddset(&mask, SIGCHLD);
	::sigprocmask(SIG_BLOCK, &mask, &prev_mask_);
}

/**
 * @brief Destroy the Debugger object
 *
 */
Debugger::~Debugger() {
	::sigprocmask(SIG_SETMASK, &prev_mask_, nullptr);
}

/**
 * @brief enables or disables lazy binding for newly spawned processes
 *
 * @param value
 */
void Debugger::set_disable_lazy_binding(bool value) {
	disableLazyBinding_ = value;
}

/**
 * @brief enables or disables address space layout randomization for newly spawned processes
 *
 * @param value
 */
void Debugger::set_disable_aslr(bool value) {
	disableASLR_ = value;
}

/**
 * @brief attaches to the process identified by <pid>
 *
 * @param pid
 * @return std::shared_ptr<Process>
 */
std::shared_ptr<Process> Debugger::attach(pid_t pid) {
	process_ = std::make_unique<Process>(pid, Process::Attach);
	return process_;
}

/**
 * @brief spawns a process and attaches to it
 *
 * @param cwd
 * @param argv
 * @return std::shared_ptr<Process>
 */
std::shared_ptr<Process> Debugger::spawn(const char *cwd, const char *argv[]) {

	switch (pid_t cpid = ::fork()) {
	case 0:
		if (disableLazyBinding_) {
			if (::setenv("LD_BIND_NOW", "1", true) == -1) {
				::perror("Failed to disable lazy binding");
			}
		}

		if (disableASLR_) {
			const int current = ::personality(UINT32_MAX);
			// This shouldn't fail, but let's at least perror if it does anyway
			if (current == -1) {
				::perror("Failed to get current personality");
			} else if (::personality(static_cast<uint32_t>(current | ADDR_NO_RANDOMIZE)) == -1) {
				::perror("Failed to disable ASLR");
			}
		}

		::ptrace(PTRACE_TRACEME, 0L, 0L, 0L);

		if (cwd) {
			::chdir(cwd);
		}

		::execv(argv[0], const_cast<char **>(argv));
		::abort();
	case -1:
		return nullptr;
	default:
		printf("Debugging New Process: %d\n", cpid);

		process_ = std::make_unique<Process>(cpid, Process::NoAttach);
		return process_;
	}
}
