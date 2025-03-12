
#include "Debugger.hpp"
#include "Process.hpp"
#include "Thread.hpp"

#include <cerrno>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <sys/mman.h>
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
 * @param cwd the working directory to change to before executing the process
 * @param argv the arguments to pass to the process
 * @note the first argument in `argv` must be the path to the executable
 * @return std::shared_ptr<Process>
 */
std::shared_ptr<Process> Debugger::spawn(const char *cwd, const char *argv[]) {
	return spawn(cwd, argv, nullptr);
}

/**
 * @brief spawns a process and attaches to it
 *
 * @param cwd the working directory to change to before executing the process
 * @param argv the arguments to pass to the process
 * @param envp the environment variables to pass to the process
 * @note the first argument in `argv` must be the path to the executable
 * @return std::shared_ptr<Process>
 */
std::shared_ptr<Process> Debugger::spawn(const char *cwd, const char *argv[], const char *envp[]) {

	constexpr std::size_t SharedMemSize = 4096;

	void *ptr       = ::mmap(nullptr, SharedMemSize, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
	auto shared_mem = static_cast<char *>(ptr);
	::memset(ptr, 0, SharedMemSize);

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

		if (::ptrace(PTRACE_TRACEME, 0L, 0L, 0L) < 0) {
			::perror("ptrace(PTRACE_TRACEME)");
			::abort();
		}

		if (cwd) {
			::chdir(cwd);
		}

		errno = 0;
		if (envp) {
			// TODO(eteran): work out the interaction between disableLazyBinding and envp
			::execve(argv[0], const_cast<char **>(argv), const_cast<char **>(envp));
		} else {
			::execv(argv[0], const_cast<char **>(argv));
		}

		// NOTE(eteran): we only get here if execve failed, so no need to explicitly check the return value
		::snprintf(shared_mem, SharedMemSize, "Failed to execv: %s", strerror(errno));
		::abort();
	case -1:
		return nullptr;
	default:
		printf("Debugging New Process: %d\n", cpid);

		process_ = std::make_unique<Process>(cpid, Process::NoAttach);

		auto thread = process_->find_thread(cpid);
		if (!thread) {
			printf("Failed to find thread for process %d\n", cpid);
			::munmap(ptr, SharedMemSize);
			return nullptr;
		}

		if (thread->is_exited()) {
			printf("The child unexpectedly exited with code %d\n", thread->exit_status());
			::munmap(ptr, SharedMemSize);
			return nullptr;
		}

		if (thread->is_signaled()) {
			printf("The child was unexpectedly killed by signal %d\n", thread->signal_status());
			::munmap(ptr, SharedMemSize);
			return nullptr;
		}

		if (thread->is_stopped() && thread->stop_status() == SIGABRT) {
			printf("The child unexpectedly aborted: %s\n", shared_mem);
			::munmap(ptr, SharedMemSize);
			return nullptr;
		}

		if (!thread->is_stopped() || thread->stop_status() != SIGTRAP) {
			printf("The child was not stopped by SIGTRAP, but by %d\n", thread->stop_status());
			::munmap(ptr, SharedMemSize);
			return nullptr;
		}

		::munmap(ptr, SharedMemSize);
		return process_;
	}
}
