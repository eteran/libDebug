// NOTE(eteran): Makes it possible for 32-bit builds to debug 64-bit processes
#define _POSIX_C_SOURCE   200809L
#define _FILE_OFFSET_BITS 64
#define _TIME_BITS        64

#include "Debug/Process.hpp"
#include "Debug/Breakpoint.hpp"
#include "Debug/DebuggerError.hpp"
#include "Debug/Event.hpp"
#include "Debug/Proc.hpp"
#include "Debug/Thread.hpp"

#include <algorithm>
#include <cassert>
#include <cerrno>
#include <chrono>
#include <cinttypes>
#include <climits>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <thread>
#include <utility>

#include <fcntl.h>
#include <sys/ptrace.h>
#include <sys/wait.h>
#include <unistd.h>

namespace {

/**
 * @brief Converts a `std::chrono::miliseconds` value into a `timespec` struct.
 *
 * @param msecs The value to convert.
 * @return The converted value.
 */
struct timespec duration_to_timespec(std::chrono::milliseconds msecs) {
	struct timespec ts;
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wuseless-cast"
	ts.tv_sec  = static_cast<time_t>(msecs.count() / 1000);
	ts.tv_nsec = static_cast<long>((msecs.count() % 1000) * 1000000);
#pragma GCC diagnostic pop
	return ts;
}

/**
 * @brief Waits for `timeout` milliseconds for a SIGCHLD signal.
 *
 * @param timeout How long to wait for the SIGCHLD signal in milliseconds.
 * @return true if a SIGCHLD was encountered, false if a timeout occurred.
 */
bool wait_for_sigchild(std::chrono::milliseconds timeout) {

	const struct timespec ts = duration_to_timespec(timeout);
	siginfo_t info;
	sigset_t mask;
	sigemptyset(&mask);
	sigaddset(&mask, SIGCHLD);
	return sigtimedwait(&mask, &info, &ts) == SIGCHLD;
}

/**
 * @brief Checks if the given status is a clone event.
 *
 * @param status The status to check.
 * @return true if the status is a clone event, false otherwise.
 */
constexpr bool is_clone_event(int status) {
	return (status >> 8 == (SIGTRAP | (PTRACE_EVENT_CLONE << 8)));
}

/**
 * @brief Checks if the given status is a trace event.
 *
 * @param status The status to check.
 * @return true if the status is a trace event, false otherwise.
 */
constexpr bool is_exit_trace_event(int status) {
	return (status >> 8 == (SIGTRAP | (PTRACE_EVENT_EXIT << 8)));
}

/**
 * @brief Checks if the given status is a trap event.
 *
 * @param status The status to check.
 * @return true if the status is a trap event, false otherwise.
 */
constexpr bool is_trap_event(int status) {
	return WIFSTOPPED(status) && WSTOPSIG(status) == SIGTRAP;
}

/**
 * @brief Opens the /proc/<pid>/mem file descriptor for the given pid.
 *
 * @param pid The pid to open the memory file descriptor for.
 * @return The file descriptor for the memory file, or -1 on error.
 */
int open_memfd(pid_t pid) {
	char path[PATH_MAX];
	std::snprintf(path, sizeof(path), "/proc/%d/mem", pid);
	return open(path, O_RDWR);
}

}

/**
 * @brief Construct a new Process object and attaches the debugger.
 * to the process identified by `pid`
 *
 * @param pid The process to attach to.
 * @param flags Controls the attach behavior of this constructor.
 */
Process::Process(pid_t pid, Flag flags)
	: pid_(pid) {

	if (flags & Process::Attach) {
		while (true) {

			const std::vector<pid_t> threads = enumerate_threads(pid);

			bool inserted = false;
			for (const pid_t tid : threads) {
				auto it = threads_.find(tid);
				if (it != threads_.end()) {
					continue;
				}

				auto new_thread = std::make_shared<Thread>(this, tid, Thread::Attach | Thread::KillOnTracerExit);
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
		auto threads = std::make_shared<Thread>(this, pid, Thread::NoAttach | Thread::KillOnTracerExit);
		threads_.emplace(pid, threads);
	}

	memfd_ = open_memfd(pid_);
	if (memfd_ == -1) {
		throw DebuggerError("Failed to open memory file descriptor for process %d: %s", pid_, strerror(errno));
	}

	report();
}

/**
 * @brief
 */
void Process::report() const {

	for (auto [tid, thread] : threads_) {
		(void)tid;

		if (thread->state_ == Thread::State::Running) {
			std::printf("Thread: %d [RUNNING]\n", thread->tid());
		} else {

			if (thread->is_exited()) {
				std::printf("Thread: %d [EXITED] [%d]\n", thread->tid(), thread->exit_status());
			}

			if (thread->is_signaled()) {
				std::printf("Thread: %d [SIGNALED] [%d]\n", thread->tid(), thread->signal_status());
			}

			if (thread->is_stopped()) {
				std::printf("Thread: %d [STOPPED] [%d]\n", thread->tid(), thread->stop_status());
			}

			if (thread->is_continued()) {
				std::printf("Thread: %d [CONTINUED]\n", thread->tid());
			}

			Context ctx;
			thread->get_context(&ctx);
			ctx.dump();
		}
	}
}

/**
 * @brief Destroy the Process object and detaches the debugger from the associated process.
 */
Process::~Process() {

	detach();

	if (memfd_ != -1) {
		close(memfd_);
	}
}

/**
 * @brief Given a buffer of memory, filters out any bytes that are part of a breakpoint.
 * and replaces them with the original bytes that were at that address before the breakpoint
 *
 * @param address The base address that was used to fill the buffer.
 * @param buffer The buffer to filter.
 * @param n The amount of bytes read into the buffer.
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
 * @brief Reads bytes from the attached process.
 *
 * @param address The address in the attached process to read from.
 * @param buffer Where to store the bytes, must be at least `n` bytes big.
 * @param n How many bytes to read.
 * @return how many bytes actually read.
 */
int64_t Process::read_memory(uint64_t address, void *buffer, size_t n) const {
#if 1
	const int64_t ret = pread(memfd_, buffer, n, static_cast<off_t>(address));
#else
	const int64_t ret = read_memory_ptrace(address, buffer, n);
#endif

	if (ret > 0) {
		filter_breakpoints(address, buffer, static_cast<size_t>(ret));
	}
	return ret;
}

/**
 * @brief Reads bytes from the attached process using the `ptrace` syscall.
 *
 * @param address The address in the attached process to read from.
 * @param buffer Where to store the bytes, must be at least `n` bytes big.
 * @param n How many bytes to read.
 * @return how many bytes actually read.
 */
int64_t Process::read_memory_ptrace(uint64_t address, void *buffer, size_t n) const {
	auto ptr      = static_cast<uint8_t *>(buffer);
	int64_t total = 0;

	while (n > 0) {
		errno          = 0;
		const long ret = ptrace(PTRACE_PEEKDATA, pid_, address, 0L);
		if (ret == -1 && errno != 0) {
			// we just ignore ESRCH because it means the process
			// was terminated while we were trying to read
			if (errno == ESRCH) {
				return 0;
			}
			break;
		}

		const size_t count = std::min(n, sizeof(ret));
		std::memcpy(ptr, &ret, count);

		address += count;
		ptr += count;
		total += static_cast<int64_t>(count);
		n -= count;
	}

	return total;
}

/**
 * @brief Writes bytes to the attached process.
 *
 * @param address The address in the attached process to write to.
 * @param buffer A buffer containing the bytes to write must be at least `n` bytes big.
 * @param n How many bytes to write.
 * @return how many bytes actually written.
 */
int64_t Process::write_memory(uint64_t address, const void *buffer, size_t n) const {
#if 1
	return pwrite(memfd_, buffer, n, static_cast<off_t>(address));
#else
	return write_memory_ptrace(address, buffer, n);
#endif
}

/**
 * @brief Writes bytes to the attached process using the `ptrace` syscall.
 *
 * @param address The address in the attached process to write to.
 * @param buffer A buffer containing the bytes to write must be at least `n` bytes big.
 * @param n How many bytes to write.
 * @return how many bytes actually written.
 */
int64_t Process::write_memory_ptrace(uint64_t address, const void *buffer, size_t n) const {
	auto ptr      = static_cast<const uint8_t *>(buffer);
	int64_t total = 0;

	while (n > 0) {
		const size_t count = std::min(n, sizeof(long));

		alignas(long) uint8_t data[sizeof(long)] = {};
		std::memcpy(data, ptr, count);

		if (count < sizeof(long)) {
			errno          = 0;
			const long ret = ptrace(PTRACE_PEEKDATA, pid_, address, 0L);
			if (ret == -1 && errno != 0) {
				// we just ignore ESRCH because it means the process
				// was terminated while we were trying to read
				if (errno == ESRCH) {
					return 0;
				}
				throw DebuggerError("Failed to read memory for process %d: %s", pid_, strerror(errno));
			}

			std::memcpy(
				data + count,
				reinterpret_cast<const uint8_t *>(&ret) + count,
				sizeof(long) - count);
		}

		long data_value;
		std::memcpy(&data_value, data, sizeof(long));

		if (ptrace(PTRACE_POKEDATA, pid_, address, data_value) == -1) {
			throw DebuggerError("Failed to write memory for process %d: %s", pid_, strerror(errno));
		}

		address += count;
		ptr += count;
		total += static_cast<int64_t>(count);
		n -= count;
	}

	return total;
}

/**
 * @brief Steps the current active thread (and ONLY the active thread) one instruction.
 * If there is no current active thread, one is selected at random to become the active thread
 * from the set of currently attached, stopped threads
 */
void Process::step() {

	// if no active thread, just pick one and step it
	// this should be a rare circumstance
	if (!active_thread_) {
		for (auto &[tid, thread] : threads_) {
			(void)tid;
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
 * @brief Resumes all attached, currently stopped threads.
 */
void Process::resume() {
	for (auto [tid, thread] : threads_) {
		(void)tid;
		if (thread->state_ == Thread::State::Stopped) {
			thread->resume();
		}
	}
}

/**
 * @brief Stops the current active thread. If there is no current active thread,
 * one is selected at random to become the active thread from the set of currently
 * attached, running threads.
 *
 * @note This is enough to stop the whole process if desired because the event
 * handler will stop all other threads if in "all-stop" mode.
 */
void Process::stop() {

	if (active_thread_) {
		active_thread_->stop();
	} else {
		for (auto &[tid, thread] : threads_) {
			(void)tid;
			if (thread->state_ == Thread::State::Running) {
				thread->stop();
				break;
			}
		}
	}
}

/**
 * @brief Terminates the attached process.
 */
void Process::kill() const {
	if (ptrace(PTRACE_KILL, pid_, 0L, 0L) == -1) {
		throw DebuggerError("Failed to kill process %d: %s", pid_, strerror(errno));
	}
}

/**
 * @brief Detaches the debugger from the attached process.
 */
void Process::detach() {
	breakpoints_.clear();
	threads_.clear();
}

/**
 * @brief A light wrapper around `waitpid` to wait for all threads to exit.
 * This is useful to call after calling `kill` to ensure the process is
 * fully cleaned up before returning from the function that called `kill`.
 */
int Process::wait() const {
	int status;
	waitpid(pid_, &status, 0);
	return status;
}

/**
 * @brief Searches for an active breakpoint which when executed will END at the given address.
 *
 * @param address The address to search for.
 * @return The breakpoint if found, otherwise nullptr.
 */
std::shared_ptr<Breakpoint> Process::search_breakpoint(uint64_t address) const {
	for (uint64_t i = Breakpoint::minBreakpointSize; i <= Breakpoint::maxBreakpointSize; ++i) {
		if (address < i) {
			continue;
		}
		const uint64_t candidate = address - i;
		if (auto bp = find_breakpoint(candidate)) {
			if (bp->size() == i) {
				return bp;
			}
		}
	}

	return nullptr;
}

/**
 * @brief Waits for `timeout` milliseconds for the next debug event to occur.
 * If there was a debug event, and we are in "all-stop" mode, then it will also
 * stop all other running threads. Events will be reported by calling `callback`.
 *
 * @param timeout The amount of milliseconds to wait for the next event.
 * @param callback The function to call when an event occurs.
 * @return true if a debug event occurred withing `timeout` milliseconds, otherwise false.
 *
 * @note This function can return true but not report any events if a debug event occurred that was filtered out
 * (e.g. a breakpoint event for a breakpoint that was just removed).
 * @note It is possible for a single call to this function can result in
 * multiple events to be reported.
 *
 */
bool Process::next_debug_event(std::chrono::milliseconds timeout, event_callback callback) {

	// TODO(eteran): handle ignored exceptions

	if (!wait_for_sigchild(timeout)) {
		return false;
	}

	bool first_stop = true;

	while (true) {
		int wstatus     = 0;
		const pid_t tid = waitpid(-1, &wstatus, __WALL | WNOHANG);

		if (tid == -1) {
			std::perror("waitpid");
			break;
		}

		if (tid == 0) {
			// no more events to process at the moment, we just return to the caller to wait for the next event
			break;
		}

		auto it = threads_.find(tid);
		if (it == threads_.end()) {
			std::printf("Event for untraced thread occurred...ignoring\n");
			continue;
		}

		auto &current_thread     = it->second;
		current_thread->wstatus_ = wstatus;
		current_thread->state_   = Thread::State::Stopped;

		if (WIFEXITED(wstatus)) {
			threads_.erase(it);

			// if the active thread just exited... pick any other thread to be the active thread
			if (active_thread_ && active_thread_->tid() == tid) {
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

		uint64_t ip = current_thread->get_instruction_pointer();

		std::printf("Stopped at: %016" PRIx64 "\n", ip);

		if (WIFSIGNALED(wstatus)) {
			// TODO(eteran): implement
			if (std::exchange(first_stop, false)) {
				active_thread_ = current_thread;
			}

			continue;
		}

		if (WIFSTOPPED(wstatus)) {
			if (std::exchange(first_stop, false)) {
				active_thread_ = current_thread;
			}

			Event e = {
				{},
				pid_,
				tid,
				wstatus,
				Event::Type::Stopped,
			};

			std::printf("Stopped Status: %d\n", current_thread->stop_status());

			if (is_trap_event(wstatus)) {

				if (ptrace(PTRACE_GETSIGINFO, tid, 0L, &current_thread->siginfo_) == -1) {
					std::perror("ptrace(PTRACE_GETSIGINFO)");
				} else {
					e.siginfo = current_thread->siginfo_;
				}

				if (is_exit_trace_event(wstatus)) {
					// Thread is about to exit, beyond that, this is a normal trap event
				} else if (is_clone_event(wstatus)) {
					unsigned long message;
					if (ptrace(PTRACE_GETEVENTMSG, tid, 0L, &message) != -1) {

						auto new_tid    = static_cast<pid_t>(message);
						auto new_thread = std::make_shared<Thread>(this, new_tid, Thread::NoAttach | Thread::KillOnTracerExit);
						threads_.emplace(new_tid, new_thread);

						new_thread->wstatus_ = 0;
						new_thread->state_   = Thread::State::Stopped;
						// TODO(eteran): start the new thread optionally
						new_thread->resume(0);
					}
				} else {

					// general trap event, likely one of:
					// * single step finished
					// * processes stopped
					// * a breakpoint
					if (auto bp = search_breakpoint(ip)) {
						std::printf("Breakpoint!\n");

						bp->hit();

						ip -= bp->size();
						current_thread->set_instruction_pointer(ip);

						// BREAKPOINT!
						// TODO(eteran): report as a trap event
					}
				}
			} else {
				if (auto bp = find_breakpoint(ip)) {
					std::printf("Alt-Breakpoint!\n");

					bp->hit();

					// NOTE(eteran): no need to rewind here because the instruction used for the BP
					// didn't advance the instruction pointer

					// ALT-BREAKPOINT!
					// TODO(eteran): report as a trap event
				}
			}

			const EventStatus status = callback(e);
			switch (status) {
			case EventStatus::Stop:
				// do nothing, the debugger will instigate the next event
				break;
			case EventStatus::Continue:
				current_thread->resume();
				break;
			case EventStatus::ContinueStep:
				current_thread->step();
				break;
			case EventStatus::ExceptionNotHandled:
				current_thread->resume(e.siginfo.si_signo);
				break;
			case EventStatus::NextHandler:
				// pass the event to the next handler
				// TODO(eteran): implement
				break;
			}

			continue;
		}

		__builtin_unreachable();
	}

	// assert(active_thread_);
	return true;
}

/**
 * @brief Adds a breakpoint to the process.
 *
 * @param address The address to add the breakpoint at.
 */
void Process::add_breakpoint(uint64_t address) {
	// Construct the breakpoint first
	// so we can use its actual size for precise overlap checks.
	auto bp = std::make_shared<Breakpoint>(this, address);
	assert(bp->size() > 0);

	const uint64_t new_start = bp->address();
	const uint64_t new_size  = bp->size();
	const uint64_t new_end   = new_start + (new_size - 1);

	for (auto const &entry : breakpoints_) {
		const uint64_t existing_addr  = entry.first;
		const auto existing_bp        = entry.second;
		const uint64_t existing_start = existing_addr;
		const uint64_t existing_size  = existing_bp->size();
		const uint64_t existing_end   = existing_start + (existing_size - 1);

		if (!(new_end < existing_start || new_start > existing_end)) {
			throw DebuggerError("Cannot add breakpoint at %" PRIx64 ": overlaps existing breakpoint at %" PRIx64, address, existing_addr);
		}
	}

	bp->enable();
	breakpoints_.emplace(address, bp);
}

/**
 * @brief Removes a breakpoint from the process.
 *
 * @param address The address to remove the breakpoint from.
 */
void Process::remove_breakpoint(uint64_t address) {
	breakpoints_.erase(address);
}

/**
 * @brief Finds a thread by its thread id.
 *
 * @param tid The thread id to find.
 * @return The thread if found, otherwise nullptr.
 */
std::shared_ptr<Thread> Process::find_thread(pid_t tid) const {
	auto it = threads_.find(tid);
	if (it != threads_.end()) {
		return it->second;
	}

	return nullptr;
}

/**
 * @brief Finds a breakpoint by its address.
 *
 * @param address The address to find the breakpoint at.
 * @return The breakpoint if found, otherwise nullptr.
 */
std::shared_ptr<Breakpoint> Process::find_breakpoint(uint64_t address) const {
	auto it = breakpoints_.find(address);
	if (it != breakpoints_.end()) {
		return it->second;
	}

	return nullptr;
}
