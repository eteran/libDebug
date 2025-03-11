
#include "Process.hpp"
#include "Breakpoint.hpp"
#include "Proc.hpp"
#include "Thread.hpp"

#include <cassert>
#include <cerrno>
#include <chrono>
#include <climits>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
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
	::sigemptyset(&mask);
	::sigaddset(&mask, SIGCHLD);
	::sigprocmask(SIG_BLOCK, &mask, nullptr);
	return ::sigtimedwait(&mask, &info, &ts) == SIGCHLD;
}

/**
 * @brief
 *
 * @param status
 * @return bool
 */
constexpr bool is_clone_event(int status) {
	return (status >> 8 == (SIGTRAP | (PTRACE_EVENT_CLONE << 8)));
}

/**
 * @brief
 *
 * @param status
 * @return bool
 */
constexpr bool is_exit_trace_event(int status) {
	return (status >> 8 == (SIGTRAP | (PTRACE_EVENT_EXIT << 8)));
}

/**
 * @brief
 *
 * @param status
 * @return bool
 */
constexpr bool is_trap_event(int status) {
	return WIFSTOPPED(status) && WSTOPSIG(status) == SIGTRAP;
}

}

/**
 * @brief Construct a new Process object and attaches the debugger
 * to the process identified by `pid`
 *
 * @param pid The process to attach to
 * @param flags Controls the attach behavior of this constructor
 */
Process::Process(pid_t pid, Flag flags)
	: pid_(pid) {

	if (flags & Process::Attach) {
		while (true) {

			const std::vector<pid_t> threads = enumerate_threads(pid);

			bool inserted = false;
			for (pid_t tid : threads) {
				auto it = threads_.find(tid);
				if (it != threads_.end()) {
					continue;
				}

				auto new_thread = std::make_shared<Thread>(pid, tid, Thread::Attach | Thread::KillOnTracerExit);
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
	} else {
		auto threads = std::make_shared<Thread>(pid, pid, Thread::NoAttach | Thread::KillOnTracerExit);
		threads_.emplace(pid, threads);
	}

	char path[PATH_MAX];
	::snprintf(path, sizeof(path), "/proc/%d/mem", pid_);
	memfd_ = ::open(path, O_RDWR);

	report();
}

/**
 * @brief
 *
 */
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

			Context_x86_64 ctx;
			thread->get_state(&ctx, sizeof(ctx));

			printf("RIP: %016lx RFL: %016lx\n", ctx.rip, ctx.eflags);
			printf("RSP: %016lx R8 : %016lx\n", ctx.rsp, ctx.r8);
			printf("RBP: %016lx R9 : %016lx\n", ctx.rbp, ctx.r9);
			printf("RAX: %016lx R10: %016lx\n", ctx.rax, ctx.r10);
			printf("RBX: %016lx R11: %016lx\n", ctx.rbx, ctx.r11);
			printf("RCX: %016lx R12: %016lx\n", ctx.rcx, ctx.r12);
			printf("RDX: %016lx R13: %016lx\n", ctx.rdx, ctx.r13);
			printf("RSI: %016lx R14: %016lx\n", ctx.rsi, ctx.r14);
			printf("RDI: %016lx R15: %016lx\n", ctx.rdi, ctx.r15);
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
 * @brief given a buffer of memory, filters out any bytes that are part of a breakpoint
 * and replaces them with the original bytes that were at that address before the breakpoint
 *
 * @param address the base address that was used to fill the buffer
 * @param buffer the buffer to filter
 * @param n the amount of bytes read into the buffer
 */
void Process::filter_breakpoints(uint64_t address, void *buffer, size_t n) const {
	for (auto &&[bp_address, bp] : breakpoints_) {
		if (bp_address >= address && bp_address < address + n) {
			const uint64_t offset    = bp_address - address;
			const uint8_t *old_bytes = bp->old_bytes();

			auto ptr = static_cast<uint8_t *>(buffer);

			for (size_t i = 0; i < bp->size(); ++i) {
				if (offset + i < n) {
					ptr[offset + i] = old_bytes[i];
				}
			}
		}
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
int64_t Process::read_memory(uint64_t address, void *buffer, size_t n) const {
#if 1
	int64_t ret = ::pread(memfd_, buffer, n, static_cast<off_t>(address));
#else
	int64_t ret = read_memory_ptrace(address, buffer, n);
#endif

	if (ret > 0) {
		filter_breakpoints(address, buffer, static_cast<size_t>(ret));
	}
	return ret;
}

/**
 * @brief reads bytes from the attached process using the `ptrace` syscall
 *
 * @param address the address in the attached process to read from
 * @param buffer where to store the bytes, must be at least `n` bytes big
 * @param n how many bytes to read
 * @return int64_t how many bytes actually read
 */
int64_t Process::read_memory_ptrace(uint64_t address, void *buffer, size_t n) const {
	auto ptr      = static_cast<uint8_t *>(buffer);
	int64_t total = 0;

	while (n > 0) {
		const ssize_t ret = ::ptrace(PTRACE_PEEKDATA, pid_, address, 0);
		if (ret == -1) {
			break;
		}

		const size_t count = std::min(n, sizeof(ret));
		::memcpy(ptr, &ret, count);

		address += count;
		ptr += count;
		total += count;
		n -= count;
	}

	return total;
}

/**
 * @brief writes bytes to the attached process
 *
 * @param address the address in the attached process to write to
 * @param buffer a buffer containing the bytes to write must be at least `n` bytes big
 * @param n how many bytes to write
 * @return int64_t how many bytes actually written
 */
int64_t Process::write_memory(uint64_t address, const void *buffer, size_t n) const {
#if 0
	return ::pwrite(memfd_, buffer, n, static_cast<off_t>(address));
#else
	return write_memory_ptrace(address, buffer, n);
#endif
}

/**
 * @brief writes bytes to the attached process using the `ptrace` syscall
 *
 * @param address the address in the attached process to write to
 * @param buffer a buffer containing the bytes to write must be at least `n` bytes big
 * @param n how many bytes to write
 * @return int64_t how many bytes actually written
 */
int64_t Process::write_memory_ptrace(uint64_t address, const void *buffer, size_t n) const {
	auto ptr      = static_cast<const uint8_t *>(buffer);
	int64_t total = 0;

	while (n > 0) {
		const size_t count = std::min(n, sizeof(long));

		alignas(long) uint8_t data[sizeof(long)] = {};
		::memcpy(data, ptr, count);

		if (count < sizeof(long)) {
			const long ret = ::ptrace(PTRACE_PEEKDATA, pid_, address, 0);
			if (ret == -1) {
				perror("ptrace(PTRACE_PEEKDATA)");
				abort();
			}

			::memcpy(
				data + count,
				reinterpret_cast<const uint8_t *>(&ret) + count,
				sizeof(long) - count);
		}

		long data_value;
		::memcpy(&data_value, data, sizeof(long));

		if (::ptrace(PTRACE_POKEDATA, pid_, address, data_value) == -1) {
			perror("ptrace(PTRACE_POKEDATA)");
			abort();
		}

		address += count;
		ptr += count;
		total += count;
		n -= count;
	}

	return total;
}

/**
 * @brief steps the current active thread (and ONLY the active thread) one instruction.
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
		::perror("ptrace(PTRACE_KILL)");
		::exit(0);
	}
}

/**
 * @brief detaches the debugger from the attached process
 */
void Process::detach() {
	breakpoints_.clear();
	threads_.clear();
}

/**
 * @brief waits for `timeout` milliseconds for the next debug event to occur.
 * If there was a debug event, and we are in "all-stop" mode, then it will also
 * stop all other running threads. Events will be reported by calling `callback`.
 * Note that it is possible for a single call to this function can result in
 * multiple events to be reported.
 *
 * @param timeout the amount of milliseconds to wait for the next event
 * @param callback the function to call when an event occurs
 * @return bool true if a debug event occurred withing `timeout` milliseconds, otherwise false
 */
bool Process::next_debug_event(std::chrono::milliseconds timeout, event_callback callback) {

	// TODO(eteran): handle breakpoints
	// TODO(eteran): handle ignored exceptions

	if (!wait_for_sigchild(timeout)) {
		return false;
	}

	bool first_stop = true;

	while (true) {
		int wstatus     = 0;
		const pid_t tid = ::waitpid(-1, &wstatus, __WALL | WNOHANG);

		if (tid == -1) {
			::perror("waitpid");
			break;
		}

		if (tid == 0) {
			break;
		}

		auto it = threads_.find(tid);
		if (it == threads_.end()) {
			printf("Event for untraced thread occurred...ignoring\n");
			continue;
		}

		auto &current_thread     = it->second;
		current_thread->wstatus_ = wstatus;
		current_thread->state_   = Thread::State::Stopped;

		if (WIFEXITED(wstatus)) {
			threads_.erase(it);

			// if the active thread just exited... pick any other thread to be the active thread
			if (active_thread_->tid() == tid) {
				if (!threads_.empty()) {
					active_thread_ = threads_.begin()->second;
				} else {
					active_thread_ = nullptr;
				}
			}
			continue;
		}

		if (WIFCONTINUED(wstatus)) {
			// NOTE(eteran): not 100% when this can actually happen...
			continue;
		}

		if (WIFSIGNALED(wstatus)) {
			// TODO(eteran): implement
			if (first_stop) {
				active_thread_ = current_thread;
				first_stop     = false;
			}

			continue;
		}

		if (WIFSTOPPED(wstatus)) {

			if (first_stop) {
				active_thread_ = current_thread;
				first_stop     = false;
			}

			Event e  = {};
			e.pid    = pid_;
			e.tid    = tid;
			e.status = wstatus;
			e.type   = Event::Type::Stopped;

			printf("Stopped Status: %d\n", current_thread->stop_status());

			if (is_trap_event(wstatus)) {

				if (::ptrace(PTRACE_GETSIGINFO, tid, 0, &e.siginfo) == -1) {
					::perror("ptrace(PTRACE_GETSIGINFO)");
				}

				if (is_exit_trace_event(wstatus)) {
					// Thread is about to exit, beyond that, this is a normal trap event
				} else if (is_clone_event(wstatus)) {
					unsigned long message;
					if (::ptrace(PTRACE_GETEVENTMSG, tid, 0, &message) != -1) {

						auto new_tid    = static_cast<pid_t>(message);
						auto new_thread = std::make_shared<Thread>(pid_, new_tid, Thread::NoAttach | Thread::KillOnTracerExit);
						threads_.emplace(new_tid, new_thread);

						new_thread->wstatus_ = 0;
						new_thread->state_   = Thread::State::Stopped;
						// TODO(eteran): start the new thread optionally
						new_thread->resume();
					}
				} else {
					// general trap event, likely one of: single step finished, processes stopped, or a breakpoint

					Context_x86_64 ctx;
					current_thread->get_state(&ctx, sizeof(ctx));

					if (auto bp = find_breakpoint(ctx.rip - 1)) {
						printf("Breakpoint!\n");

						ctx.rip--;
						current_thread->set_state(&ctx, sizeof(ctx));

						// BREAKPOINT!
						// TODO(eteran): report as a trap event
						// TODO(eteran): rewind RIP by appropriate amount
					}
				}
			}

			// TODO(eteran): the callback should dictate what to do next
			callback(e);

			// TODO(eteran_): conditionally resume
			current_thread->resume();
			continue;
		}

		// NOTE(eteran): should never get here
		assert(false && "internal error");
	}

	// assert(active_thread_);
	return true;
}

/**
 * @brief
 *
 * @param address
 */
void Process::add_breakpoint(uint64_t address) {
	auto bp = std::make_shared<Breakpoint>(this, address);
	breakpoints_.emplace(address, bp);
}

/**
 * @brief
 *
 * @param address
 */
void Process::remove_breakpoint(uint64_t address) {
	breakpoints_.erase(address);
}

/**
 * @brief
 *
 * @param tid
 * @return std::shared_ptr<Thread>
 */
std::shared_ptr<Thread> Process::find_thread(pid_t tid) const {
	auto it = threads_.find(tid);
	if (it != threads_.end()) {
		return it->second;
	}

	return nullptr;
}

/**
 * @brief
 *
 * @param address
 * @return std::shared_ptr<Breakpoint>
 */
std::shared_ptr<Breakpoint> Process::find_breakpoint(uint64_t address) const {
	auto it = breakpoints_.find(address);
	if (it != breakpoints_.end()) {
		return it->second;
	}

	return nullptr;
}
