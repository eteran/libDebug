
#include "Debugger.hpp"
#include "Process.hpp"
#include "Thread.hpp"

#include <csignal>
#include <sys/personality.h>
#include <sys/ptrace.h>

const bool DisableLazyBinding = true;
const bool DisableASLR        = true;

/**
 * @brief Construct a new Debugger:: Debugger object
 *
 */
Debugger::Debugger() {

	sigset_t mask;
	::sigemptyset(&mask);
	::sigaddset(&mask, SIGCHLD);
	::sigprocmask(SIG_BLOCK, &mask, &prev_mask_);
}

/**
 * @brief Destroy the Debugger:: Debugger object
 *
 */
Debugger::~Debugger() {
	::sigprocmask(SIG_SETMASK, &prev_mask_, nullptr);
}

/**
 * @brief attaches to the process identified by <pid>
 *
 * @param pid
 */
std::shared_ptr<Process> Debugger::attach(pid_t pid) {
	process_ = std::make_shared<Process>(pid, Process::Attach);
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

	switch (pid_t cpid = fork()) {
	case 0:

		if (DisableLazyBinding) {
			if (setenv("LD_BIND_NOW", "1", true) == -1) {
				perror("Failed to disable lazy binding");
			}
		}

		if (DisableASLR) {
			const int current = ::personality(UINT32_MAX);
			// This shouldn't fail, but let's at least perror if it does anyway
			if (current == -1) {
				perror("Failed to get current personality");
			} else if (::personality(static_cast<uint32_t>(current | ADDR_NO_RANDOMIZE)) == -1) {
				perror("Failed to disable ASLR");
			}
		}

		ptrace(PTRACE_TRACEME, 0L, 0L, 0L);

		if (cwd) {
			::chdir(cwd);
		}

		::execv(argv[0], const_cast<char **>(argv));
		::abort();
	case -1:
		return nullptr;
	default:
		printf("Debugging New Process: %d\n", cpid);
		return std::make_shared<Process>(cpid, Process::NoAttach);
	}
}
