#ifndef TEST_HELPERS_HPP_
#define TEST_HELPERS_HPP_

#include "Debug/Debugger.hpp"
#include "Debug/DebuggerError.hpp"
#include "Debug/Process.hpp"
#include "Test.hpp"

#include <chrono>
#include <cstdint>
#include <memory>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>

struct AttachedContext {
	uint64_t address  = 0;
	pid_t child_pid   = -1;
	int sync_write_fd = -1;
	std::shared_ptr<Process> process;
};

/**
 * @brief Notifies the child process to start by writing a byte to the provided file descriptor.
 */
inline void notify_child_start(int sync_write_fd) {
	char one            = 1;
	const ssize_t wrote = write(sync_write_fd, &one, 1);
	CHECK_MSG(wrote == 1, "failed to notify child to start");
}

inline void child_wait_ready(int sync_read_fd) {
	char ready;
	const ssize_t rr = read(sync_read_fd, &ready, 1);
	CHECK_MSG(rr == 1, "failed to read ready byte from child");
}

template <class Ptr>
void send_address(int addr_write_fd, Ptr address) {

    static_assert(sizeof(Ptr) <= sizeof(uint64_t), "Pointer size is larger than uint64_t, cannot send address through pipe");

	// NOTE(eteran): This cast is only circumstantially "useless" depending
	// on if we are building with -m32 or -m64, so we need to silence the warning here.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wuseless-cast"
	auto addr = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(address));
#pragma GCC diagnostic pop

	ssize_t wrote = write(addr_write_fd, &addr, sizeof(addr));
	CHECK_MSG(wrote == static_cast<ssize_t>(sizeof(addr)), "failed to write code address to pipe");
}

inline void send_dummy_address(int addr_write_fd) {
	uint64_t dummy_addr = 0xdeadbeeffeedface;
	ssize_t wrote       = write(addr_write_fd, &dummy_addr, sizeof(dummy_addr));
	CHECK_MSG(wrote == static_cast<ssize_t>(sizeof(dummy_addr)), "failed to write code address to pipe");
}



/**
 * @brief Runs a child function in a separate process, attaches to it with the debugger,
 * and then runs a parent function with the attached process context.
 *
 * @param child_fn The function to run in the child process.
 * It will be passed a file descriptor to write an address to and a file descriptor for synchronization.
 * @param parent_fn The function to run in the parent process.
 * It will be passed an AttachedContext containing the child process id, synchronization file descriptor, attached process, and the address read from the child.
 * @param attach_required If true, the function will check that attaching to the child process succeeds.
 * If false, it will not check and will simply return if attaching fails.
 */
template <class ChildFn, class ParentFn>
void with_attached_child(const ChildFn &child_fn, const ParentFn &parent_fn, bool attach_required = false) {
	int addr_pipe[2];
	int sync_pipe[2];
	CHECK_MSG(pipe(addr_pipe) == 0, "pipe(addr_pipe) failed");
	CHECK_MSG(pipe(sync_pipe) == 0, "pipe(sync_pipe) failed");

	const pid_t cpid = fork();
	CHECK_MSG(cpid >= 0, "fork() failed");

	if (cpid == 0) {
		close(addr_pipe[0]);
		close(sync_pipe[1]);
		child_fn(addr_pipe[1], sync_pipe[0]);
		_exit(0);
	}

	close(addr_pipe[1]);
	close(sync_pipe[0]);

	uint64_t address = 0;
	const ssize_t rr = read(addr_pipe[0], &address, sizeof(address));
	CHECK_MSG(rr == static_cast<ssize_t>(sizeof(address)), "failed to read address from pipe");

	Debugger dbg;
	std::shared_ptr<Process> proc;
	try {
		proc = dbg.attach(cpid);
	} catch (const DebuggerError &) {
		kill(cpid, SIGKILL);
		waitpid(cpid, nullptr, 0);
		if (attach_required) {
			CHECK_MSG(false, "dbg.attach() failed");
		}
		close(addr_pipe[0]);
		close(sync_pipe[1]);
		return;
	}

	CHECK_MSG(proc != nullptr, "dbg.attach() returned null");

	AttachedContext ctx;
	ctx.child_pid     = cpid;
	ctx.sync_write_fd = sync_pipe[1];
	ctx.process       = proc;
	ctx.address       = address;

	parent_fn(ctx);

	close(addr_pipe[0]);
	close(sync_pipe[1]);
}

/**
 * @brief Polls for debug events from the process until a done condition is met or a timeout occurs.
 *
 * @param process The process to poll for debug events from.
 * @param total_timeout The total amount of time to wait for the done condition to be met before giving up.
 * @param poll_interval The amount of time to wait between polls for debug events.
 * @param callback The function to call with each debug event that occurs.
 * It should return an EventStatus indicating whether to continue polling or stop.
 * @param done The function to call after each poll to check if the done condition has been met.
 * It should return true if we are done and can stop polling, or false if we should keep polling.
 * @return true if the done condition was met within the total timeout, or false if the total timeout was reached without the done condition being met.
 */
template <class CallbackFn, class DoneFn>
bool poll_debug_events_until(
	const std::shared_ptr<Process> &process,
	std::chrono::milliseconds total_timeout,
	std::chrono::milliseconds poll_interval,
	const CallbackFn &callback,
	const DoneFn &done) {

	const auto deadline = std::chrono::steady_clock::now() + total_timeout;
	while (std::chrono::steady_clock::now() < deadline) {
		(void)process->next_debug_event(poll_interval, callback);
		if (done()) {
			return true;
		}
	}

	return done();
}

#endif
