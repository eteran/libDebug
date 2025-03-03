
#include "Debugger.hpp"
#include "Process.hpp"
#include "Thread.hpp"

#include <csignal>
#include <sys/ptrace.h>

/**
 * @brief Construct a new Debugger:: Debugger object
 *
 */
Debugger::Debugger() {

	sigset_t mask;
	sigemptyset(&mask);
	sigaddset(&mask, SIGCHLD);
	sigprocmask(SIG_BLOCK, &mask, &prev_mask_);
}

/**
 * @brief Destroy the Debugger:: Debugger object
 *
 */
Debugger::~Debugger() {
	sigprocmask(SIG_SETMASK, &prev_mask_, nullptr);
}

/**
 * @brief attaches to the process identified by <pid>
 *
 * @param pid
 */
std::shared_ptr<Process> Debugger::attach(pid_t pid) {
	process_ = std::make_shared<Process>(pid, Process::Flags::Attach);
	return process_;
}

/**
 * @brief spawns a process and attaches to it
 *
 * @param cwd
 * @param tty
 * @param argv
 * @return std::shared_ptr<Process>
 */
std::shared_ptr<Process> Debugger::spawn(const char *cwd, const char *tty, const char *argv[]) {

	switch (pid_t cpid = fork()) {
	case 0:
		ptrace(PTRACE_TRACEME, 0L, 0L, 0L);

		if (tty) {
			FILE *const std_out = ::freopen(tty, "r+b", stdout);
			FILE *const std_in  = ::freopen(tty, "r+b", stdin);
			FILE *const std_err = ::freopen(tty, "r+b", stderr);
		}

		if (cwd) {
			::chdir(cwd);
		}

		::execv(argv[0], const_cast<char **>(argv));
		::abort();
	case -1:
		return nullptr;
	default:
		auto process = std::make_shared<Process>(cpid, Process::Flags::NoAttach);
		auto threads = std::make_shared<Thread>(cpid, cpid, Thread::Flags::NoAttach);
		process->threads_.emplace(cpid, threads);
		return process;
	}
}
