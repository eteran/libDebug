
#include "Debug/Debugger.hpp"
#include "Debug/DebuggerError.hpp"
#include "Debug/Defer.hpp"
#include "Debug/Process.hpp"
#include "Debug/Ptrace.hpp"
#include "Debug/Thread.hpp"

#include <cerrno>
#include <climits>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <sys/mman.h>
#include <sys/personality.h>
#include <unistd.h>
#include <vector>

// Per-thread counters and saved mask so we only modify the signal mask
// once per thread regardless of how many Debugger instances exist.
namespace {
thread_local int DebuggerCount = 0;
thread_local sigset_t PrevSigMask;
}

/**
 * @brief Construct a new Debugger object.
 * This constructor blocks SIGCHLD for the current thread if this is the first Debugger
 * instance being created. This ensures that waitpid usage in Process::wait() is deterministic,
 * and that we can use sigtimedwait to implement timeouts in Process::next_debug_event().
 */
Debugger::Debugger() {

	ctor_thread_id_ = std::this_thread::get_id();

	// Block SIGCHLD once per thread so waitpid usage is deterministic.
	if (DebuggerCount == 0) {
		sigset_t mask;
		sigemptyset(&mask);
		sigaddset(&mask, SIGCHLD);

		if (sigprocmask(SIG_BLOCK, &mask, &PrevSigMask) != 0) {
			throw DebuggerError("Failed to block SIGCHLD: %s", strerror(errno));
		}
	}
	++DebuggerCount;
}

/**
 * @brief Destroy the Debugger object.
 * This destructor unblocks SIGCHLD for the current thread if this is the last Debugger
 * instance being destroyed.
 *
 * @note The destructor must be called on the same thread that the constructor was called on, since
 * the signal mask is modified on that thread. If the destructor is called on a different thread,
 * we print an error message and abort the process.
 */
Debugger::~Debugger() {
	// NOTE(eteran): Enforce that the Debugger is destroyed on the same thread it was
	// constructed on. Restoring the signal mask must occur on that thread.
	if (std::this_thread::get_id() != ctor_thread_id_) {
		std::fprintf(stderr, "Debugger destroyed on a different thread than constructed\n");
		std::abort();
	}

	if (DebuggerCount > 0) {
		if (--DebuggerCount == 0) {
			sigprocmask(SIG_SETMASK, &PrevSigMask, nullptr);
		}
	}
}

/**
 * @brief Enables or disables lazy binding for newly spawned processes.
 *
 * @param value true to disable lazy binding, false to enable it.
 */
void Debugger::set_disable_lazy_binding(bool value) noexcept {
	disableLazyBinding_ = value;
}

/**
 * @brief Enables or disables address space layout randomization for newly spawned processes.
 *
 * @param value true to disable ASLR, false to enable it.
 */
void Debugger::set_disable_aslr(bool value) noexcept {
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
	process_ = std::make_shared<Process>(Process::internal_t(), pid, Process::Attach);
	return process_;
}

/**
 * @brief Spawns a process and attaches to it.
 *
 * @param cwd The working directory to change to before executing the process.
 * @param argv The arguments to pass to the process.
 * @param envp The environment variables to pass to the process.
 *
 * @return The process object or `nullptr` if the process could not be spawned or attached to.
 * @note The first argument in `argv` must be the path to the executable.
 * @note The last argument in `argv` must be nullptr.
 * @note If `envp` is `nullptr`, the current environment is used.
 * @note The environment variables are passed as a null-terminated array of strings, where each string is of the form "KEY=VALUE".
 * @note The last argument in `envp` must be `nullptr`.
 * @throws DebuggerError if any errors occur during the process creation.
 */
std::shared_ptr<Process> Debugger::spawn(const char *cwd, const char *argv[], const char *envp[]) {

	constexpr std::size_t SharedMemSize = 4096;

	void *ptr = mmap(nullptr, SharedMemSize, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
	if (ptr == MAP_FAILED) {
		throw DebuggerError("Failed to create shared memory: %s", strerror(errno));
	}

	auto shared_mem = static_cast<char *>(ptr);
	std::memset(ptr, 0, SharedMemSize);

	SCOPE_EXIT({
		munmap(ptr, SharedMemSize);
	});

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
			if (current == -1) {
				std::snprintf(shared_mem, SharedMemSize, "Failed to get current personality: %s", strerror(errno));
				std::abort();
			} else if (personality(static_cast<uint32_t>(current | ADDR_NO_RANDOMIZE)) == -1) {
				std::snprintf(shared_mem, SharedMemSize, "Failed to disable ASLR: %s", strerror(errno));
				std::abort();
			}
		}

		if (auto ret = do_ptrace(PTRACE_TRACEME, 0L, 0L, 0L); ret.is_err()) {
			std::snprintf(shared_mem, SharedMemSize, "Failed to enable tracing: %s", strerror(ret.error()));
			std::abort();
		}

		if (cwd) {
			if (chdir(cwd) == -1) {
				std::snprintf(shared_mem, SharedMemSize, "Failed to change working directory: %s", strerror(errno));
				std::abort();
			}
		}

		if (envp) {
			// If the caller provided an explicit envp, setting an env var with
			// setenv() above won't affect the environment passed to execve().
			// If lazy binding is disabled, inject LD_BIND_NOW=1 into the envp
			// we pass to execve unless the caller already set it.
			if (disableLazyBinding_) {
				bool has_ld = false;
				for (const char **e = envp; *e; ++e) {
					if (std::strncmp(*e, "LD_BIND_NOW=", 12) == 0) {
						has_ld = true;
						break;
					}
				}

				if (!has_ld) {
					std::vector<char *> new_env;
					for (const char **e = envp; *e; ++e) {
						new_env.emplace_back(const_cast<char *>(*e));
					}
					new_env.emplace_back(const_cast<char *>("LD_BIND_NOW=1"));
					new_env.push_back(nullptr);
					execve(argv[0], const_cast<char **>(argv), new_env.data());
					// execve only returns on error; fallthrough records the error below
				}
			}

			// If lazy binding isn't disabled or execve above failed, fall back
			// to calling execve with the original envp (this will also run
			// if disableLazyBinding_ was false).
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
		process_ = std::make_shared<Process>(Process::internal_t(), cpid, Process::NoAttach);

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
