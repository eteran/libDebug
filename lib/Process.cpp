// NOTE(eteran): Makes it possible for 32-bit builds to debug 64-bit processes
#define _POSIX_C_SOURCE   200809L
#define _FILE_OFFSET_BITS 64
#define _TIME_BITS        64

#include "Debug/Process.hpp"
#include "Debug/Breakpoint.hpp"
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

#include <fcntl.h>
#include <sys/ptrace.h>
#include <sys/wait.h>

namespace {

/**
 * @brief Converts a `std::chrono::miliseconds` value into a `timespec` struct.
 *
 * @param msecs The value to convert.
 * @return The converted value.
 */
struct timespec duration_to_timespec(std::chrono::milliseconds msecs) {
	struct timespec ts;
	ts.tv_sec  = msecs.count() / 1000;
	ts.tv_nsec = static_cast<long>((msecs.count() % 1000) * 1000000);
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
	sigprocmask(SIG_BLOCK, &mask, nullptr);
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

void dump_context(Context *ctx) {

	/*
	RIP: 0033:0x7ffff7fe31e7
	Code: c0 75 e0 e9 6e fe ff ff 66 2e 0f 1f 84 00 00 00 00 00 90 55 48 83 c7 08 49 89 f0 48 89 e5 41 55 41 54 53 48 81 ec 30 01 00 00 <48> 8b 47 f8 48 89 3d 86 98 01 00 89 05 88 98 01 00 48 98 48 8d 74
	RSP: 002b:00007fffffffd728 EFLAGS: 00010206
	RAX: 00007ffff7ffdab0 RBX: 00007ffff7fe5af0 RCX: 0000000000000024
	RDX: 00007ffff7fc54f0 RSI: 00007fffffffd880 RDI: 00000000ffffd988
	RBP: 00007fffffffd870 R08: 00007fffffffd880 R09: 00007ffff7ffcec8
	R10: 0000000000036360 R11: 0000000000000000 R12: 00007fffffffd940
	R13: 0000000000000000 R14: 00007ffff7fc5000 R15: 00007ffff7fc5590
	FS:  0000000000000000 GS:  0000000000000000
	*/

	if (ctx->is_64_bit()) {
		std::printf("RIP: %016" PRIx64 " RFL: %016" PRIx64 "\n", ctx->get(RegisterId::RIP).as<uint64_t>(), ctx->get(RegisterId::RFLAGS).as<uint64_t>());
		std::printf("RSP: %016" PRIx64 " R8 : %016" PRIx64 "\n", ctx->get(RegisterId::RSP).as<uint64_t>(), ctx->get(RegisterId::R8).as<uint64_t>());
		std::printf("RBP: %016" PRIx64 " R9 : %016" PRIx64 "\n", ctx->get(RegisterId::RBP).as<uint64_t>(), ctx->get(RegisterId::R9).as<uint64_t>());
		std::printf("RAX: %016" PRIx64 " R10: %016" PRIx64 "\n", ctx->get(RegisterId::RAX).as<uint64_t>(), ctx->get(RegisterId::R10).as<uint64_t>());
		std::printf("RBX: %016" PRIx64 " R11: %016" PRIx64 "\n", ctx->get(RegisterId::RBX).as<uint64_t>(), ctx->get(RegisterId::R11).as<uint64_t>());
		std::printf("RCX: %016" PRIx64 " R12: %016" PRIx64 "\n", ctx->get(RegisterId::RCX).as<uint64_t>(), ctx->get(RegisterId::R12).as<uint64_t>());
		std::printf("RDX: %016" PRIx64 " R13: %016" PRIx64 "\n", ctx->get(RegisterId::RDX).as<uint64_t>(), ctx->get(RegisterId::R13).as<uint64_t>());
		std::printf("RSI: %016" PRIx64 " R14: %016" PRIx64 "\n", ctx->get(RegisterId::RSI).as<uint64_t>(), ctx->get(RegisterId::R14).as<uint64_t>());
		std::printf("RDI: %016" PRIx64 " R15: %016" PRIx64 "\n", ctx->get(RegisterId::RDI).as<uint64_t>(), ctx->get(RegisterId::R15).as<uint64_t>());
		std::printf("CS: %04x SS : %04x FS_BASE:  %016" PRIx64 "\n", ctx->get(RegisterId::CS).as<uint16_t>(), ctx->get(RegisterId::SS).as<uint16_t>(), ctx->get(RegisterId::FS_BASE).as<uint64_t>());
		std::printf("DS: %04x ES : %04x GS_BASE:  %016" PRIx64 "\n", ctx->get(RegisterId::DS).as<uint16_t>(), ctx->get(RegisterId::ES).as<uint16_t>(), ctx->get(RegisterId::GS_BASE).as<uint64_t>());
		std::printf("FS: %04x GS : %04x\n", ctx->get(RegisterId::FS).as<uint16_t>(), ctx->get(RegisterId::GS).as<uint16_t>());

		std::printf("EBX: %08x\n", ctx->get(RegisterId::EBX).as<uint32_t>());
		std::printf("BX: %04x\n", ctx->get(RegisterId::BX).as<uint16_t>());
		std::printf("BL: %02x\n", ctx->get(RegisterId::BL).as<uint8_t>());
		std::printf("BH: %02x\n", ctx->get(RegisterId::BH).as<uint8_t>());
	} else {
		std::printf("EIP: %08x EFL: %08x\n", ctx->get(RegisterId::EIP).as<uint32_t>(), ctx->get(RegisterId::EFLAGS).as<uint32_t>());
		std::printf("ESP: %08x EBP: %08x\n", ctx->get(RegisterId::ESP).as<uint32_t>(), ctx->get(RegisterId::EBP).as<uint32_t>());
		std::printf("EAX: %08x EBX: %08x\n", ctx->get(RegisterId::EAX).as<uint32_t>(), ctx->get(RegisterId::EBX).as<uint32_t>());
		std::printf("ECX: %08x EDX: %08x\n", ctx->get(RegisterId::ECX).as<uint32_t>(), ctx->get(RegisterId::EDX).as<uint32_t>());
		std::printf("ESI: %08x EDI: %08x\n", ctx->get(RegisterId::ESI).as<uint32_t>(), ctx->get(RegisterId::EDI).as<uint32_t>());

		std::printf("CS: %04x SS : %04x FS_BASE:  %016" PRIx64 "\n", ctx->get(RegisterId::CS).as<uint16_t>(), ctx->get(RegisterId::SS).as<uint16_t>(), ctx->get(RegisterId::FS_BASE).as<uint64_t>());
		std::printf("DS: %04x ES : %04x GS_BASE:  %016" PRIx64 "\n", ctx->get(RegisterId::DS).as<uint16_t>(), ctx->get(RegisterId::ES).as<uint16_t>(), ctx->get(RegisterId::GS_BASE).as<uint64_t>());
		std::printf("FS: %04x GS : %04x\n", ctx->get(RegisterId::FS).as<uint16_t>(), ctx->get(RegisterId::GS).as<uint16_t>());
	}
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
	std::snprintf(path, sizeof(path), "/proc/%d/mem", pid_);
	memfd_ = open(path, O_RDWR);

	assert(memfd_ != -1);

	report();
}

/**
 * @brief
 */
void Process::report() const {

	for (auto [tid, thread] : threads_) {

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

			dump_context(&ctx);
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
				std::perror("ptrace(PTRACE_PEEKDATA)");
				std::abort();
			}

			std::memcpy(
				data + count,
				reinterpret_cast<const uint8_t *>(&ret) + count,
				sizeof(long) - count);
		}

		long data_value;
		std::memcpy(&data_value, data, sizeof(long));

		if (ptrace(PTRACE_POKEDATA, pid_, address, data_value) == -1) {
			std::perror("ptrace(PTRACE_POKEDATA)");
			std::abort();
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
		std::perror("ptrace(PTRACE_KILL)");
		std::exit(0);
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
 * @brief Waits for `timeout` milliseconds for the next debug event to occur.
 * If there was a debug event, and we are in "all-stop" mode, then it will also
 * stop all other running threads. Events will be reported by calling `callback`.
 * Note that it is possible for a single call to this function can result in
 * multiple events to be reported.
 *
 * @param timeout The amount of milliseconds to wait for the next event.
 * @param callback The function to call when an event occurs.
 * @return true if a debug event occurred withing `timeout` milliseconds, otherwise false.
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
		const pid_t tid = waitpid(-1, &wstatus, __WALL | WNOHANG);

		if (tid == -1) {
			std::perror("waitpid");
			break;
		}

		if (tid == 0) {
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

		Context ctx;
		current_thread->get_context(&ctx);

		RegisterRef ip_ref = ctx.is_64_bit() ? ctx.get(RegisterId::RIP) : ctx.get(RegisterId::EIP);
		uint64_t ip        = ip_ref.as<uint64_t>();

		std::printf("Stopped at: %016" PRIx64 "\n", ip);

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

			std::printf("Stopped Status: %d\n", current_thread->stop_status());

			if (is_trap_event(wstatus)) {

				if (ptrace(PTRACE_GETSIGINFO, tid, 0L, &e.siginfo) == -1) {
					std::perror("ptrace(PTRACE_GETSIGINFO)");
				}

				if (is_exit_trace_event(wstatus)) {
					// Thread is about to exit, beyond that, this is a normal trap event
				} else if (is_clone_event(wstatus)) {
					unsigned long message;
					if (ptrace(PTRACE_GETEVENTMSG, tid, 0L, &message) != -1) {

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
					for (uint64_t i = 1; i <= 2; ++i) {
						if (auto bp = find_breakpoint(ip - i)) {

							if (bp->size() == i) {
								std::printf("Breakpoint!\n");
								ip_ref = ip - i;
								current_thread->set_context(&ctx);

								// BREAKPOINT!
								// TODO(eteran): report as a trap event
								break;
							}
						}
					}
				}
			} else {
				if (auto bp = find_breakpoint(ip)) {
					std::printf("Alt-Breakpoint!\n");
					// NOTE(eteran): no need to rewind here because the instruction used for the BP
					// didn't advance the instruction pointer

					// TODO(eteran): report as a trap event
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
 * @brief Adds a breakpoint to the process.
 *
 * @param address The address to add the breakpoint at.
 */
void Process::add_breakpoint(uint64_t address) {
	auto bp = std::make_shared<Breakpoint>(this, address);
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
