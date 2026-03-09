// NOTE(eteran): Makes it possible for 32-bit builds to debug 64-bit processes
#define _POSIX_C_SOURCE   200809L
#define _FILE_OFFSET_BITS 64
#define _TIME_BITS        64

#include "Debug/Process.hpp"
#include "Debug/Breakpoint.hpp"
#include "Debug/DebuggerError.hpp"
#include "Debug/Event.hpp"
#include "Debug/Memory.hpp"
#include "Debug/Proc.hpp"
#include "Debug/Ptrace.hpp"
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
#include <sys/uio.h>
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
 *
 * @note This function assumes that the signal is blocked in the current thread, so that it can be reliably received by `sigtimedwait`.
 * This is the case for us because the only way to create a Process object is through the Debugger interface, which blocks SIGCHLD in
 * the constructor before creating the Process object.
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
 * @brief Checks if the given status is a trap event.
 *
 * @param status The status to check.
 * @return true if the status is a trap event, false otherwise.
 */
bool is_trap_event(int status) {
	return WIFSTOPPED(status) && WSTOPSIG(status) == SIGTRAP;
}

/**
 * @brief Checks if the given status is a clone event.
 *
 * @param status The status to check.
 * @return true if the status is a clone event, false otherwise.
 */
bool is_ptrace_event(int status, int event) {
	// ptrace encodes the event in the high 16 bits of the wait status when a SIGTRAP stop is reported.
	return is_trap_event(status) && (((status >> 16) & 0xffff) == event);
}

/**
 * @brief Checks if the given status is a clone event.
 *
 * @param status The status to check.
 * @return true if the status is a clone event, false otherwise.
 */
bool is_clone_event(int status) {
	// Treat CLONE, FORK and VFORK ptrace events as equivalent "clone"-style events
	return is_ptrace_event(status, PTRACE_EVENT_CLONE) ||
		   is_ptrace_event(status, PTRACE_EVENT_FORK) ||
		   is_ptrace_event(status, PTRACE_EVENT_VFORK);
}

/**
 * @brief Checks if the given status is a trace event.
 *
 * @param status The status to check.
 * @return true if the status is a trace event, false otherwise.
 */
bool is_exit_trace_event(int status) {
	return is_ptrace_event(status, PTRACE_EVENT_EXIT);
}

}

/**
 * @brief Construct a new Process object and attaches the debugger.
 * to the process identified by `pid`
 *
 * @param pid The process to attach to.
 * @param flags Controls the attach behavior of this constructor.
 */
Process::Process(const internal_t &, pid_t pid, Flag flags)
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

				auto new_thread = std::make_shared<Thread>(Thread::internal_t(), this, tid, Thread::Attach | Thread::KillOnTracerExit);
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
		auto threads = std::make_shared<Thread>(Thread::internal_t(), this, pid, Thread::NoAttach | Thread::KillOnTracerExit);
		threads_.emplace(pid, threads);
	}

	try {
		memory_ = std::make_unique<ProcMemory>(pid);
	} catch (const DebuggerError &) {
		memory_ = std::make_unique<PtraceMemory>(pid);
	}

	report();
}

/**
 * @brief Prints a report about the current state of the process, including all threads and their contexts.
 * This is useful for debugging and testing purposes.
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

	int64_t ret = memory_->read(address, buffer, n);

	if (ret > 0) {
		filter_breakpoints(address, buffer, static_cast<size_t>(ret));
	}
	return ret;
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
	return memory_->write(address, buffer, n);
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
			const int signal = thread->pending_signal_;
			thread->resume(signal);
			thread->pending_signal_ = 0; // Clear after delivering
		}
	}
}

/**
 * @brief Stops all running threads except the specified one.
 *
 * @param except_tid The thread to exclude from stopping (0 to stop all threads).
 * @return The thread IDs of the threads that were asked to stop.
 */
std::vector<pid_t> Process::stop_all_threads(pid_t except_tid) {
	std::vector<pid_t> target_tids;
	target_tids.reserve(threads_.size());

	for (auto &[tid, thread] : threads_) {
		if (tid != except_tid && thread->state_ == Thread::State::Running) {
			target_tids.push_back(tid);
			thread->stop();
		}
	}

	return target_tids;
}

/**
 * @brief Waits until all target threads are no longer running.
 *
 * @param target_tids The thread IDs that were asked to stop.
 *
 * @note This is a synchronization barrier used in all-stop mode to guarantee no other thread is
 * running while breakpoint fixups (disable -> step -> re-enable) are performed.
 * Any exit or signal termination events encountered are queued for later dispatch.
 */
void Process::all_stop_barrier(const std::vector<pid_t> &target_tids) {
	for (const pid_t target_tid : target_tids) {
		auto it = threads_.find(target_tid);
		if (it == threads_.end()) {
			continue;
		}

		while (true) {
			int status      = 0;
			const pid_t tid = waitpid(target_tid, &status, __WALL);

			if (tid == -1) {
				if (errno == EINTR) {
					continue;
				}

				std::perror("waitpid");
				break;
			}

			auto thread_it = threads_.find(tid);
			if (thread_it == threads_.end()) {
				break;
			}

			auto &thread     = thread_it->second;
			thread->wstatus_ = status;
			thread->state_   = Thread::State::Stopped;

			if (WIFSTOPPED(status)) {
				// Normal stop (SIGSTOP from all-stop or other signal) - barrier complete for this thread
				break;
			} else if (WIFEXITED(status) || WIFSIGNALED(status)) {
				// Thread exited or was killed - queue for later event dispatch
				pending_events_.push_back({tid, status});
				break;
			}
		}
	}
}

/**
 * @brief Stops the current active thread. If there is no current active thread,
 * one is selected at random to become the active thread from the set of currently
 * attached, running threads.
 *
 * @note To stop all threads (all-stop mode), use stop_all_threads(0) instead.
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
	::kill(pid_, SIGKILL);
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
	for (uint64_t i = Breakpoint::MinBreakpointSize; i <= Breakpoint::MaxBreakpointSize; ++i) {
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
 * @brief Reports an exit event to the callback and removes the thread from tracking.
 *
 * @param ctx The context of the event.
 * @param callback The callback to report the event to.
 */
void Process::handle_exit_event(EventContext &ctx, event_callback callback) {

	Event e = {
		{},
		pid_,
		ctx.tid,
		ctx.wstatus,
		Event::Type::Exited,
	};

	// Report the event, but ignore the callback's return value because the thread is
	// already exiting and there's nothing the callback can do to change that.
	(void)callback(e);

	// Remove from tracking and select a new active thread if needed
	threads_.erase(ctx.tid);
	if (active_thread_ && active_thread_->tid() == ctx.tid) {
		if (!threads_.empty()) {
			active_thread_ = threads_.begin()->second;
		} else {
			active_thread_ = nullptr;
		}
	}
}

/**
 * @brief Reports a clone event to the callback and adds the new thread to tracking.
 *
 * @param ctx The context of the event.
 * @param callback The callback to report the event to.
 */
void Process::handle_clone_event(EventContext &ctx, event_callback callback) {

	unsigned long message;
	if (do_ptrace(PTRACE_GETEVENTMSG, ctx.tid, 0L, &message).ok()) {

		printf("Clone event: new thread tid=%lu\n", message);

		auto new_tid    = static_cast<pid_t>(message);
		auto new_thread = std::make_shared<Thread>(Thread::internal_t(), this, new_tid, Thread::NoAttach | Thread::KillOnTracerExit);
		threads_.emplace(new_tid, new_thread);

		new_thread->wstatus_ = 0;
		new_thread->state_   = Thread::State::Stopped;

		Event clone_event = {
			ctx.current_thread->siginfo_,
			pid_,
			ctx.tid,
			ctx.wstatus,
			Event::Type::Clone,
			new_tid,

		};

		// For now, we unconditionally resume the new thread, but we could potentially let the callback
		// decide whether to resume it or not by checking the return value of the callback and resuming
		// the thread only if the return value is `Continue` or `ContinueStep`.
		(void)callback(clone_event);

		// TODO(eteran): start the new thread optionally
		new_thread->resume(0);
	}
}

/**
 * @brief Reports an exit trace event to the callback and removes the thread from tracking.
 * An exit trace event is a special ptrace event that is triggered when a thread is about to exit,
 * but before it has fully exited.
 *
 * @param ctx The context of the event.
 * @param callback The callback to report the event to.
 */
void Process::handle_exit_trace_event(EventContext &ctx, event_callback callback) {

	// Thread is about to exit — report as a Terminated event.

	Event exit_event = {
		ctx.current_thread->siginfo_,
		pid_,
		ctx.tid,
		ctx.wstatus,
		Event::Type::Terminated,
	};

	(void)callback(exit_event);

	// Remove the thread from tracking.
	threads_.erase(ctx.tid);
	if (active_thread_ && active_thread_->tid() == ctx.tid) {
		if (!threads_.empty()) {
			active_thread_ = threads_.begin()->second;
		} else {
			active_thread_ = nullptr;
		}
	}
}

/**
 * @brief Reports a signal event to the callback and removes the thread from tracking.
 *
 * @param ctx The context of the event.
 * @param callback The callback to report the event to.
 *
 * @note A signal event is triggered when a thread is terminated by a signal.
 * This is different from a normal exit event because the thread was not able to clean up and exit normally.
 */
void Process::handle_signal_event(EventContext &ctx, event_callback callback) {
#if 0
	if (std::exchange(ctx.first_stop, false)) {
		active_thread_ = ctx.current_thread;
	}
#endif
	std::printf("Thread %d was terminated by signal %d\n", ctx.tid, WTERMSIG(ctx.wstatus));

	// Report a Terminated event so callers can observe the terminating signal.
	Event e = {
		{},
		pid_,
		ctx.tid,
		ctx.wstatus,
		Event::Type::Terminated,
	};
	// Populate minimal siginfo so handlers can inspect the signal.
	e.siginfo.si_signo = WTERMSIG(ctx.wstatus);
	(void)callback(e);

	// The thread was terminated by a signal; remove it from tracking.
	threads_.erase(ctx.tid);

	// If the active thread exited, pick another one.
	if (active_thread_ && active_thread_->tid() == ctx.tid) {
		if (!threads_.empty()) {
			active_thread_ = threads_.begin()->second;
		} else {
			active_thread_ = nullptr;
		}
	}
}

/**
 * @brief Reports a continue event to the callback. A continue event is triggered when a thread is continued after being stopped.
 *
 * @param ctx The context of the event.
 * @param callback The callback to report the event to.
 */
void Process::handle_continue_event(EventContext &ctx, event_callback callback) {
	(void)ctx;
	(void)callback;
	// NOTE(eteran): Not sure under what circumstances we would get a continue event, but we handle it just in case.
}

/**
 * @brief Reports a trap event to the callback. A trap event is triggered when a thread hits a breakpoint or finishes a single step.
 *
 * @param ctx The context of the event.
 * @param callback The callback to report the event to.
 */
void Process::handle_trap_event(EventContext &ctx, event_callback callback) {

	(void)callback;

	// general trap event, likely one of:
	// * single step finished
	// * processes stopped
	// * a breakpoint
	if (auto bp = search_breakpoint(ctx.address)) {
		bp->hit();

		ctx.address -= bp->size();
		ctx.current_thread->set_instruction_pointer(ctx.address);

		// BREAKPOINT!
	}
}

/**
 * @brief Reports an unknown event to the callback. This is a catch-all for events that we don't have specific handlers for.
 *
 * @param ctx The context of the event.
 * @param callback The callback to report the event to.
 *
 * @note This is used to catch things like our "alt-breakpoints" which are breakpoints that don't use an actual breakpoint instruction
 * and thus don't trigger a trap event, but we can still detect them by checking if the instruction at the current RIP matches a known breakpoint pattern.
 */
bool Process::handle_unknown_event(EventContext &ctx, event_callback callback) {
	(void)callback;

	if (auto bp = find_breakpoint(ctx.address)) {
		bp->hit();

		// NOTE(eteran): no need to rewind here because the instruction used for the BP
		// didn't advance the instruction pointer

		// ALT-BREAKPOINT!
		return true;
	}

	return false;
}

/**
 * @brief Reports a stop event to the callback. A stop event is triggered when a thread is stopped by a signal or by the user.
 *
 * @param ctx The context of the event.
 * @param callback The callback to report the event to.
 */
void Process::handle_stop_event(EventContext &ctx, event_callback callback) {
	if (std::exchange(ctx.first_stop, false)) {
		active_thread_ = ctx.current_thread;
	}

	if (ctx.current_thread->pending_step_breakpoint_.has_value()) {
		if (auto bp = find_breakpoint(*ctx.current_thread->pending_step_breakpoint_); bp) {
			bp->enable();
		}

		ctx.current_thread->pending_step_breakpoint_.reset();
	}

	/*
	 * If this is a signal-stop (i.e. not a SIGTRAP), and the signal
	 * is configured to be ignored for this Process, resume the thread
	 * and do not report an event to the callback.
	 *
	 * For non-ignored stops we load the `siginfo` so event handlers
	 * always receive valid signal information.
	 */
	const int stop_sig = WSTOPSIG(ctx.wstatus);
	if (stop_sig != SIGTRAP && is_ignoring_signal(stop_sig)) {
		std::printf("Signal %d is being ignored, resuming thread %d\n", stop_sig, ctx.tid);
		ctx.current_thread->resume(stop_sig);
		return;
	}

	/* Load siginfo for stopped events so callbacks can inspect it. */
	if (!ctx.current_thread->load_signal_info()) {
		// Can't proceed without valid signal info—resume and continue to next event
		ctx.current_thread->resume(stop_sig);
		return;
	}

	switch (ctx.current_thread->siginfo_.si_signo) {
	case SIGTRAP:

		switch (ctx.current_thread->siginfo_.si_code) {
		case SI_KERNEL:
		case TRAP_BRKPT:
			handle_trap_event(ctx, callback);
			break;
		case TRAP_TRACE:
			std::printf("Hit a trace trap\n");
			break;
		case TRAP_BRANCH:
			std::printf("Hit a branch trap\n");
			break;
		case TRAP_HWBKPT:
			std::printf("Hit a hardware breakpoint/watchpoint trap\n");
			break;
		case TRAP_UNK:
		default:
			std::printf("Hit an unknown trap: si_code=%d\n", ctx.current_thread->siginfo_.si_code);
			break;
		}

		if (is_exit_trace_event(ctx.wstatus)) {
			handle_exit_trace_event(ctx, callback);
			return;
		} else if (is_clone_event(ctx.wstatus)) {
			handle_clone_event(ctx, callback);
			return;
		}

		break;
	case SIGSEGV:
	case SIGFPE:
	case SIGILL:
	case SIGBUS:

		// TODO(eteran): first check if this is due to a breakpoint (e.g. an "alt-breakpoint" that doesn't use an actual breakpoint instruction
		// and thus doesn't trigger a trap event), and if so handle it as a breakpoint event instead of a signal event
		if (handle_unknown_event(ctx, callback)) {
			break;
		}

		process_stop_event(ctx, callback, Event::Type::Fault);
		return;
	default:
		handle_unknown_event(ctx, callback);
		break;
	}

	process_stop_event(ctx, callback, Event::Type::Stopped);
}

/**
 * @brief Reports a stop event to the callback and takes action based on the callback's return value.
 *
 * @param ctx The context of the event.
 * @param callback The callback to report the event to.
 * @param stop_type The type of stop event to report (e.g. normal stop, signal stop, etc.)
 */
void Process::process_stop_event(EventContext &ctx, event_callback callback, Event::Type stop_type) {
	Event e = {
		ctx.current_thread->siginfo_,
		pid_,
		ctx.tid,
		ctx.wstatus,
		stop_type,
	};

	const EventStatus status = callback(e);
	switch (status) {
	case EventStatus::Stop:
		// Thread remains stopped - save pending signal for later resume
		if (stop_type == Event::Type::Fault || stop_type == Event::Type::Stopped) {
			ctx.current_thread->pending_signal_ = e.siginfo.si_signo;
		}
		break;
	case EventStatus::Continue:
		ctx.current_thread->pending_signal_ = 0;
		ctx.current_thread->resume();
		break;
	case EventStatus::ContinueStep:
		ctx.current_thread->pending_signal_ = 0;
		ctx.current_thread->step();
		break;
	case EventStatus::ExceptionNotHandled:
		// User chose to deliver the signal - set pending and resume with it
		ctx.current_thread->pending_signal_ = e.siginfo.si_signo;
		ctx.current_thread->resume(e.siginfo.si_signo);
		ctx.current_thread->pending_signal_ = 0; // Delivered, clear it
		break;
	case EventStatus::NextHandler:
		// pass the event to the next handler
		// TODO(eteran): implement
		break;
	}
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

	if (!wait_for_sigchild(timeout)) {
		return false;
	}

	EventContext ctx;
	// First stop should be true so the first stopped thread becomes the active thread.
	ctx.first_stop = true;

	// Drain any pending events from the all-stop barrier before checking for new events
	while (!pending_events_.empty()) {
		const PendingEvent pending = pending_events_.front();
		pending_events_.pop_front();

		auto it = threads_.find(pending.tid);
		if (it == threads_.end()) {
			continue;
		}

		ctx.tid                      = pending.tid;
		ctx.current_thread           = it->second;
		ctx.current_thread->wstatus_ = pending.wstatus;
		ctx.current_thread->state_   = Thread::State::Stopped;
		ctx.wstatus                  = pending.wstatus;
		ctx.address                  = 0;

		if (WIFEXITED(ctx.wstatus)) {
			handle_exit_event(ctx, callback);
			continue;
		}

		if (WIFSIGNALED(ctx.wstatus)) {
			handle_signal_event(ctx, callback);
			continue;
		}
	}

	while (true) {
		const pid_t tid = waitpid(-1, &ctx.wstatus, __WALL | WNOHANG);

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

		// Populate the ctx with per-event information for handlers.
		ctx.tid                      = tid;
		ctx.current_thread           = it->second;
		ctx.current_thread->wstatus_ = ctx.wstatus;
		ctx.current_thread->state_   = Thread::State::Stopped;
		ctx.address                  = 0;

		// In all-stop mode, when we encounter the first significant stop event,
		// immediately stop all other running threads before processing.
		// This ensures breakpoint handling (disable → step → re-enable) is safe.
		if (all_stop_ && ctx.first_stop && WIFSTOPPED(ctx.wstatus)) {
			const auto target_tids = stop_all_threads(tid);
			all_stop_barrier(target_tids);
		}

		if (WIFEXITED(ctx.wstatus)) {
			handle_exit_event(ctx, callback);
			continue;
		}

		if (WIFCONTINUED(ctx.wstatus)) {
			handle_continue_event(ctx, callback);
			continue;
		}

		ctx.address = ctx.current_thread->get_instruction_pointer();

		if (WIFSIGNALED(ctx.wstatus)) {
			handle_signal_event(ctx, callback);
			continue;
		}

		if (WIFSTOPPED(ctx.wstatus)) {
			handle_stop_event(ctx, callback);
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
void Process::add_breakpoint(uint64_t address, Breakpoint::TypeId type) {
	// Construct the breakpoint first
	// so we can use its actual size for precise overlap checks.
	auto bp = std::make_shared<Breakpoint>(this, address, type);
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

/**
 * @brief Ignores a signal for the attached process.
 * Ignored signals will not cause the process to stop and will not be reported to the event handler.
 * However, they will still be delivered to the process as normal.
 */
void Process::ignore_signal(int signal) {
	ignored_signals_.insert(signal);
}

/**
 * @brief Unignores a signal for the attached process.
 *
 * @param signal The signal to unignore.
 */
void Process::unignore_signal(int signal) {
	ignored_signals_.erase(signal);
}

/**
 * @brief Checks if a signal is being ignored for the attached process.
 *
 * @param signal The signal to check.
 * @return true if the signal is being ignored, false otherwise.
 */
[[nodiscard]] bool Process::is_ignoring_signal(int signal) const {
	return ignored_signals_.count(signal) > 0;
}
