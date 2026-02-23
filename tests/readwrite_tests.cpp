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

TEST(ReadWrite) {

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
		void *mem         = mmap(nullptr, page, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
		if (mem == MAP_FAILED) {
			perror("mmap");
			_exit(1);
		}

		// fill memory with a known pattern that the parent can verify after writing to it
		auto ptr = static_cast<uint8_t *>(mem);
		for (int i = 0; i < 32; ++i) {
			ptr[i] = static_cast<uint8_t>(0xaa + i);
		}

		// NOTE(eteran): This cast is only circumstantially "useless" depending
		// on if we are building with -m32 or -m64, so we need to silence the warning here.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wuseless-cast"
		auto addr = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(ptr));
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

		_exit(0);
	}

	close(addr_pipe[1]);
	close(sync_pipe[0]);

	uint64_t mem_addr = 0;
	ssize_t rr        = read(addr_pipe[0], &mem_addr, sizeof(mem_addr));
	CHECK_MSG(rr == static_cast<ssize_t>(sizeof(mem_addr)), "failed to read memory address from pipe");

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

	// verify we can read the known pattern from the child process
	uint8_t buffer[32];
	int64_t nread = proc->read_memory(mem_addr, buffer, sizeof(buffer));
	CHECK_MSG(nread == sizeof(buffer), "proc->read_memory returned unexpected size");
	for (int i = 0; i < 32; ++i) {
		CHECK_MSG(buffer[i] == static_cast<uint8_t>(0xaa + i), "initial pattern mismatch in child memory");
	}

	// verify we can write to the child process
	for (int i = 0; i < 32; ++i) {
		buffer[i] = static_cast<uint8_t>(0x55 + i);
	}
	int64_t nwritten = proc->write_memory(mem_addr, buffer, sizeof(buffer));
	CHECK_MSG(nwritten == sizeof(buffer), "proc->write_memory wrote unexpected number of bytes");

	// verify the changes were written to the child process
	uint8_t verify_buffer[32];
	nread = proc->read_memory(mem_addr, verify_buffer, sizeof(verify_buffer));
	CHECK_MSG(nread == sizeof(verify_buffer), "proc->read_memory returned unexpected size for verification");
	for (int i = 0; i < 32; ++i) {
		CHECK_MSG(verify_buffer[i] == static_cast<uint8_t>(0x55 + i), "verify buffer mismatch after write");
	}

	// signal child to continue
	char one = 1;
	if (write(sync_pipe[1], &one, 1) != 1) {
		_exit(1);
	}

	proc->resume();
	proc->kill();
	proc->wait();
}
