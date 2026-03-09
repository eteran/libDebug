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

struct AttachedChildContext {
	pid_t child_pid   = -1;
	int sync_write_fd = -1;
	std::shared_ptr<Process> process;
};

struct AttachedChildAddressContext : AttachedChildContext {
	uint64_t address = 0;
};

inline void notify_child_start(int sync_write_fd) {
	char one            = 1;
	const ssize_t wrote = write(sync_write_fd, &one, 1);
	CHECK_MSG(wrote == 1, "failed to notify child to start");
}

template <class ChildFn, class ParentFn>
void with_attached_child_with_address(const ChildFn &child_fn, const ParentFn &parent_fn, bool attach_required = false) {
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

	AttachedChildAddressContext ctx;
	ctx.child_pid     = cpid;
	ctx.sync_write_fd = sync_pipe[1];
	ctx.process       = proc;
	ctx.address       = address;

	parent_fn(ctx);

	close(addr_pipe[0]);
	close(sync_pipe[1]);
}

template <class ChildFn, class ParentFn>
void with_attached_child_sync(const ChildFn &child_fn, const ParentFn &parent_fn, bool attach_required = false) {
	int sync_pipe[2];
	CHECK_MSG(pipe(sync_pipe) == 0, "pipe(sync_pipe) failed");

	const pid_t cpid = fork();
	CHECK_MSG(cpid >= 0, "fork() failed");

	if (cpid == 0) {
		close(sync_pipe[1]);
		child_fn(sync_pipe[0]);
		_exit(0);
	}

	close(sync_pipe[0]);

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
		close(sync_pipe[1]);
		return;
	}

	CHECK_MSG(proc != nullptr, "dbg.attach() returned null");

	AttachedChildContext ctx;
	ctx.child_pid     = cpid;
	ctx.sync_write_fd = sync_pipe[1];
	ctx.process       = proc;

	parent_fn(ctx);

	close(sync_pipe[1]);
}

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

inline void child_run_basic_executable_buffer(int addr_write_fd, int sync_read_fd) {
	const size_t page = 4096;
	void *mem         = mmap(nullptr, page, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
	if (mem == MAP_FAILED) {
		perror("mmap");
		_exit(1);
	}

	auto code = static_cast<unsigned char *>(mem);
	code[0]   = 0x90;
	code[1]   = 0x90;
	code[2]   = 0xc3;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wuseless-cast"
	auto addr = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(code));
#pragma GCC diagnostic pop

	ssize_t wrote = write(addr_write_fd, &addr, sizeof(addr));
	if (wrote != static_cast<ssize_t>(sizeof(addr))) {
		_exit(1);
	}

	char ready;
	if (read(sync_read_fd, &ready, 1) != 1) {
		_exit(1);
	}

	int (*fn)() = reinterpret_cast<int (*)()>(code);
	fn();
	_exit(0);
}

#endif
