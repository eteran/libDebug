#include "Debug/Debugger.hpp"
#include "Debug/DebuggerError.hpp"
#include "Debug/Event.hpp"
#include "Debug/EventStatus.hpp"
#include "Debug/Proc.hpp"
#include "Debug/Process.hpp"
#include "Test.hpp"

#include <sys/mman.h>
#include <sys/wait.h>

using namespace std::chrono_literals;

TEST(BreakpointHit) {

	int addr_pipe[2];
	int sync_pipe[2];
	CHECK_MSG(pipe(addr_pipe) == 0, "pipe(addr_pipe) failed");
	CHECK_MSG(pipe(sync_pipe) == 0, "pipe(sync_pipe) failed");

	pid_t cpid = fork();
	CHECK_MSG(cpid >= 0, "fork() failed");

	if (cpid == 0) {
		close(addr_pipe[0]);
		close(sync_pipe[1]);

		const size_t page = 4096;
		void *mem         = mmap(nullptr, page, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
		if (mem == MAP_FAILED) {
			perror("mmap");
			_exit(1);
		}

		auto code = static_cast<unsigned char *>(mem);

		code[0] = 0x90; // NOP
		code[1] = 0x90; // NOP
		code[2] = 0xc3; // RET

		// NOTE(eteran): This cast is only circumstantially "useless" depending
		// on if we are building with -m32 or -m64, so we need to silence the warning here.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wuseless-cast"
		auto addr = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(code));
#pragma GCC diagnostic pop

		ssize_t wrote = write(addr_pipe[1], &addr, sizeof(addr));
		if (wrote != static_cast<ssize_t>(sizeof(addr))) {
			_exit(1);
		}

		// wait for parent to signal it's ready
		char ready;
		if (read(sync_pipe[0], &ready, 1) != 1) {
			_exit(1);
		}

		int (*fn)() = reinterpret_cast<int (*)()>(code);
		fn();
		_exit(0);
	}

	// Parent: receive address, attach, set breakpoint, then allow child to continue.
	close(addr_pipe[1]);
	close(sync_pipe[0]);

	uint64_t code_addr = 0;
	ssize_t rr         = read(addr_pipe[0], &code_addr, sizeof(code_addr));
	CHECK_MSG(rr == static_cast<ssize_t>(sizeof(code_addr)), "failed to read code address from pipe");

	Debugger dbg;
	std::shared_ptr<Process> proc;
	try {
		proc = dbg.attach(cpid);
	} catch (const DebuggerError &e) {
		// ptrace not permitted in this environment (e.g., YAMA/ptrace_scope)
		kill(cpid, SIGKILL);
		waitpid(cpid, nullptr, 0);
		return;
	}
	CHECK_MSG(proc != nullptr, "dbg.attach() returned null");

	proc->add_breakpoint(code_addr);

	// signal child to continue
	char one = 1;
	if (write(sync_pipe[1], &one, 1) != 1) {
		_exit(1);
	}

	proc->resume();

	volatile bool hit = false;
	auto cb           = [&](const Event &e) -> EventStatus {
        printf("Debug event received: pid=%d tid=%d status=%d type=%d\n", e.pid, e.tid, e.status, static_cast<int>(e.type));

        if (e.type == Event::Type::Stopped) {
            hit = true;
        }

        return EventStatus::Continue;
	};

	// Some SIGCHLDs may wake the wait without delivering a trace event
	// for our thread; poll next_debug_event until we observe the breakpoint
	// or until a total timeout elapses.
	const auto deadline = std::chrono::steady_clock::now() + 5s;
	bool observed       = false;
	while (std::chrono::steady_clock::now() < deadline) {
		if (proc->next_debug_event(std::chrono::milliseconds(200), cb)) {
			if (hit) {
				observed = true;
				break;
			}

			printf("Spurious debug event observed, waiting for breakpoint...\n");
		}
	}
	CHECK_MSG(observed, "did not observe expected breakpoint event");

	proc->kill();
	proc->wait();
}
