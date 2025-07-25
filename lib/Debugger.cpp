
#include "Debug/Debugger.hpp"
#include "Debug/DebuggerError.hpp"
#include "Debug/Defer.hpp"
#include "Debug/Process.hpp"
#include "Debug/Thread.hpp"

#include <cerrno>
#include <climits>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <sys/mman.h>
#include <sys/personality.h>
#include <sys/ptrace.h>
#include <unistd.h>

/**
 * @brief Construct a new Debugger object.
 */
Debugger::Debugger() {

	// we need to block SIGCHLD to make sure that we can control
	// the waitpid calls.
	sigset_t mask;
	sigemptyset(&mask);
	sigaddset(&mask, SIGCHLD);
	sigprocmask(SIG_BLOCK, &mask, &prev_mask_);
}

/**
 * @brief Destroy the Debugger object.
 */
Debugger::~Debugger() {
	sigprocmask(SIG_SETMASK, &prev_mask_, nullptr);
}

/**
 * @brief Enables or disables lazy binding for newly spawned processes.
 *
 * @param value true to disable lazy binding, false to enable it.
 */
void Debugger::set_disable_lazy_binding(bool value) {
	disableLazyBinding_ = value;
}

/**
 * @brief Enables or disables address space layout randomization for newly spawned processes.
 *
 * @param value true to disable ASLR, false to enable it.
 */
void Debugger::set_disable_aslr(bool value) {
	disableASLR_ = value;
}

/**
 * @brief Attaches to the process identified by <pid>.
 *
 * @param pid The process to attach to.
 * @return The process object.
 * @throws DebuggerError if the process could not be attached.
 */
std::shared_ptr<Process> Debugger::attach(pid_t pid) {
	process_ = std::make_unique<Process>(pid, Process::Attach);
	return process_;
}

/**
 * @brief Spawns a process and attaches to it.
 *
 * @param cwd The working directory to change to before executing the process.
 * @param argv The arguments to pass to the process.
 * @param envp The environment variables to pass to the process.
 *
 * @return The process object.
 * @note The first argument in `argv` must be the path to the executable.
 * @note The last argument in `argv` must be nullptr.
 * @note If envp is nullptr, the current environment is used.
 * @note The environment variables are passed as a null-terminated array of strings, where each string is of the form "KEY=VALUE".
 * @note The last argument in `envp` must be nullptr.
 * @throws DebuggerError if any errors occur during the process creation.
 */
std::shared_ptr<Process> Debugger::spawn(const char *cwd, const char *argv[], const char *envp[]) {

	constexpr std::size_t SharedMemSize = 4096;

	void *ptr       = mmap(nullptr, SharedMemSize, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
	auto shared_mem = static_cast<char *>(ptr);
	std::memset(ptr, 0, SharedMemSize);

	// NOTE(eteran): the use of std::abort() in the child is intentional, as it
	// generates a SIGABRT that the parent can detect and report the error message.
	switch (const pid_t cpid = fork()) {
	case 0:

		if (disableLazyBinding_) {
			if (setenv("LD_BIND_NOW", "1", true) == -1) {
				std::snprintf(shared_mem, SharedMemSize, "Failed to disable lazy binding: %s", strerror(errno));
				std::abort();
			}
		}

		if (disableASLR_) {
			const int current = personality(UINT32_MAX);
			// This shouldn't fail, but let's at least perror if it does anyway
			if (current == -1) {
				std::snprintf(shared_mem, SharedMemSize, "Failed to get current personality: %s", strerror(errno));
				std::abort();
			} else if (personality(static_cast<uint32_t>(current | ADDR_NO_RANDOMIZE)) == -1) {
				std::snprintf(shared_mem, SharedMemSize, "Failed to disable ASLR: %s", strerror(errno));
				std::abort();
			}
		}

		if (ptrace(PTRACE_TRACEME, 0L, 0L, 0L) == -1) {
			std::snprintf(shared_mem, SharedMemSize, "Failed to enable tracing: %s", strerror(errno));
			std::abort();
		}

		if (cwd) {
			if (chdir(cwd) == -1) {
				std::snprintf(shared_mem, SharedMemSize, "Failed to change working directory: %s", strerror(errno));
				std::abort();
			}
		}

		if (envp) {
			// TODO(eteran): work out the interaction between disableLazyBinding and envp
			execve(argv[0], const_cast<char **>(argv), const_cast<char **>(envp));
		} else {
			execv(argv[0], const_cast<char **>(argv));
		}

		// NOTE(eteran): we only get here if execve failed, so no need to explicitly check the return value
		std::snprintf(shared_mem, SharedMemSize, "Failed to execv: %s", strerror(errno));
		std::abort();
	case -1:
		return nullptr;
	default:
		std::printf("Debugging New Process: %d\n", cpid);

		process_ = std::make_unique<Process>(cpid, Process::NoAttach);

		SCOPE_EXIT({
			munmap(ptr, SharedMemSize);
		});

		auto thread = process_->find_thread(cpid);
		if (!thread) {
			throw DebuggerError("Failed to find thread for process %d", cpid);
		}

		if (thread->is_exited()) {
			throw DebuggerError("The child unexpectedly exited with code %d", thread->exit_status());
		}

		if (thread->is_signaled()) {
			throw DebuggerError("The child was unexpectedly killed by signal %d", thread->signal_status());
		}

		if (thread->is_stopped() && thread->stop_status() == SIGABRT) {
			throw DebuggerError("The child unexpectedly aborted: %s", shared_mem);
		}

		if (!thread->is_stopped() || thread->stop_status() != SIGTRAP) {
			throw DebuggerError("The child was not stopped by SIGTRAP, but by %d", thread->stop_status());
		}

		return process_;
	}
}
