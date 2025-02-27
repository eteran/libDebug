
#include "Process.hpp"
#include "Proc.hpp"
#include "Thread.hpp"

#include <cassert>
#include <cerrno>
#include <chrono>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <thread>

#include <dirent.h>
#include <fcntl.h>
#include <sys/ptrace.h>
#include <sys/uio.h>
#include <sys/wait.h>

namespace {

/**
 * @brief converts a `std::chrono::miliseconds` value into a `timespec` struct
 *
 * @param msecs
 * @return struct timespec
 */
struct timespec duration_to_timespec(std::chrono::milliseconds msecs) {
	struct timespec ts;
	ts.tv_sec  = (msecs.count() / 1000);
	ts.tv_nsec = (msecs.count() % 1000) * 1000000;
	return ts;
}

/**
 * @brief waits for `timeout` milliseconds for a SIGCHLD signal.
 *
 * @param timeout how long to wait for the SIGCHLD signal in milliseconds
 * @return bool true if a SIGCHLD was encountered, false if a timeout occurred.
 */
bool wait_for_sigchild(std::chrono::milliseconds timeout) {

	const struct timespec ts = duration_to_timespec(timeout);
	siginfo_t info;
	sigset_t mask;
	sigemptyset(&mask);
	sigaddset(&mask, SIGCHLD);
	sigprocmask(SIG_BLOCK, &mask, nullptr);
	const int ret = ::sigtimedwait(&mask, &info, &ts);
	// sigprocmask(SIG_UNBLOCK, &mask, nullptr);
	return ret == SIGCHLD;
}

/**
 * @brief
 *
 * @param status
 * @return constexpr bool
 */
constexpr bool is_clone_event(int status) {
	return (status >> 8 == (SIGTRAP | (PTRACE_EVENT_CLONE << 8)));
}

/**
 * @brief
 *
 * @param status
 * @return constexpr bool
 */
constexpr bool is_exit_trace_event(int status) {
	return (status >> 8 == (SIGTRAP | (PTRACE_EVENT_EXIT << 8)));
}

}

/**
 * @brief Construct a new Process object and attaches the debugger
 * to the process identified by `pid`
 *
 * @param pid The process to attach to
 */
Process::Process(pid_t pid)
	: pid_(pid) {

	while (true) {

		const std::vector<pid_t> threads = enumerate_threads(pid);

		bool inserted = false;
		for (pid_t tid : threads) {
			auto it = threads_.find(tid);
			if (it != threads_.end()) {
				continue;
			}

			auto new_thread = std::make_shared<Thread>(pid, tid, Thread::Flags::Attach);
			threads_.emplace(tid, new_thread);

			if (!active_thread_) {
				active_thread_ = new_thread;
			}

			inserted = true;
		}

		if (!inserted) {
			break;
		}
	}

	char path[PATH_MAX];
	::snprintf(path, sizeof(path), "/proc/%d/mem", pid_);
	memfd_ = ::open(path, O_RDWR);

	report();
}

void Process::report() const {
	for (auto [tid, thread] : threads_) {

		if (thread->state_ == Thread::State::Running) {
			printf("Thread: %d [RUNNING]\n", thread->tid());
		} else {

			if (thread->is_exited()) {
				printf("Thread: %d [EXITED] [%d]\n", thread->tid(), thread->exit_status());
			}

			if (thread->is_signaled()) {
				printf("Thread: %d [SIGNALED] [%d]\n", thread->tid(), thread->signal_status());
			}

			if (thread->is_stopped()) {
				printf("Thread: %d [STOPPED] [%d]\n", thread->tid(), thread->stop_status());
			}

			if (thread->is_continued()) {
				printf("Thread: %d [CONTINUED]\n", thread->tid());
			}
		}
	}
}

/**
 * @brief Destroy the Process object and detaches the debugger from the associated process
 */
Process::~Process() {

	detach();

	if (memfd_ != -1) {
		::close(memfd_);
	}
}

/**
 * @brief reads bytes from the attached process
 *
 * @param address the address in the attached process to read from
 * @param buffer where to store the bytes, must be at least `n` bytes big
 * @param n how many bytes to read
 * @return int64_t how many bytes actually read
 */
int64_t Process::read_memory(uint64_t address, void *buffer, size_t n) {
#if 1
	return ::pread(memfd_, buffer, n, static_cast<off_t>(address));
#else
	// NOTE(eteran): perhaps not viable for a ptrace API?
	struct iovec local[1];
	struct iovec remote[1];

	local[0].iov_base = buffer;
	local[0].iov_len  = n;

	remote[0].iov_base = (void *)address;
	remote[0].iov_len  = n;

	return process_vm_readv(pid_, local, 1, remote, 1, 0);
#endif
}

/**
 * @brief writes bytes to the attached process
 *
 * @param address the address in the attached process to write to
 * @param buffer a buffer containing the bytes to write must be at least `n` bytes big
 * @param n how many bytes to write
 * @return int64_t how many bytes actually written
 */
int64_t Process::write_memory(uint64_t address, const void *buffer, size_t n) {
#if 1
	return ::pwrite(memfd_, buffer, n, static_cast<off_t>(address));
#else
	// NOTE(eteran): perhaps not viable for a ptrace API?
	struct iovec local[1];
	struct iovec remote[1];

	local[0].iov_base = const_cast<void *>(buffer);
	local[0].iov_len  = n;

	remote[0].iov_base = (void *)address;
	remote[0].iov_len  = n;

	return process_vm_writev(pid_, local, 1, remote, 1, 0);
#endif
}

/**
 * @brief steps the active thread (and ONLY the active thread) one instruction.
 * If there is no current active thread, one is selected at random to become the active thread
 * from the set of currently attached, stopped threads
 */
void Process::step() {

	// if no active thread, just pick one and step it
	// this should be a rare circumstance
	if (!active_thread_) {
		for (auto &[tid, thread] : threads_) {
			if (thread->state_ == Thread::State::Stopped) {
				active_thread_ = thread;
				break;
			}
		}
	}

	assert(active_thread_);
	active_thread_->step();
}

/**
 * @brief resumes all attached, currently stopped threads
 */
void Process::resume() {
	for (auto [tid, thread] : threads_) {
		if (thread->state_ == Thread::State::Stopped) {
			thread->resume();
		}
	}
}

/**
 * @brief stops the current active thread. If there is no current active thread,
 * one is selected at random to become the active thread from the set of currently
 * attached, running threads. NOTE: this is enough to stop the whole process if
 * desired because the event handler will stop all other threads if in "all-stop"
 * mode
 */
void Process::stop() {

	printf("Stopping Process [%d]\n", pid_);

	if (active_thread_) {
		active_thread_->stop();
	} else {
		for (auto &[tid, thread] : threads_) {
			if (thread->state_ == Thread::State::Running) {
				thread->stop();
				break;
			}
		}
	}
}

/**
 * @brief Terminates the attached process
 */
void Process::kill() {
	long ret = ::ptrace(PTRACE_KILL, pid_, 0L, 0L);
	if (ret == -1) {
		perror("ptrace (kill)");
		exit(0);
	}
}

/**
 * @brief detaches the debugger from the attached process
 */
void Process::detach() {
	threads_.clear();
}

/**
 * @brief waits for `timeout` milliseconds for the next debug event to occur.
 * If there was a debug event, and we are in "all-stop" mode, then it will also
 * stop all other running threads
 *
 * @param timeout the amount of milliseconds to wait for the next event
 * @return bool true if a debug event occurred withing `timeout` milliseconds, otherwise false
 */
bool Process::next_debug_event(std::chrono::milliseconds timeout) {

#if 0
	if (!wait_for_sigchild(timeout)) {
		printf("No SIGCHLD\n");
		return false;
	}
#endif

	active_thread_ = nullptr;

	while (true) {
		int wstatus     = 0;
		const pid_t tid = ::waitpid(-1, &wstatus, __WALL);

		if (tid == -1) {
			perror("waitpid");
			exit(0);
		}

		if (tid == 0) {
			printf("No Event\n");
			break;
		}

		printf("Wait Event Success! [%d] [%d]\n", tid, wstatus);

		if (is_exit_trace_event(wstatus)) {
			printf("EXIT EVENT!\n");
		}

		if (is_clone_event(wstatus)) {
			printf("CLONE EVENT!\n");

			unsigned long message;
			if (ptrace(PTRACE_GETEVENTMSG, tid, 0, &message) != -1) {

				auto new_tid = static_cast<pid_t>(message);
				printf("Cloned Thread ID [%d]\n", new_tid);
				auto new_thread = std::make_shared<Thread>(pid_, new_tid, Thread::Flags::NoAttach);
				threads_.emplace(new_tid, new_thread);

				new_thread->wstatus_ = wstatus;
				new_thread->state_   = Thread::State::Stopped;
				new_thread->resume();
			}
		}

		auto it = threads_.find(tid);
		if (it != threads_.end()) {
			it->second->wstatus_ = wstatus;
			it->second->state_   = Thread::State::Stopped;
			it->second->resume();
			if (!active_thread_) {
				active_thread_ = it->second;
			}
		} else {
			printf("Unknown Thread?! [%d]\n", tid);
		}
	}

	assert(active_thread_);

	return true;
}
