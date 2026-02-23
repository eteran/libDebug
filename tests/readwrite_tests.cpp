#include "Debug/Debugger.hpp"
#include "Debug/Proc.hpp"
#include "Debug/Process.hpp"
#include "Debug/Region.hpp"
#include "Test.hpp"
#include "Debug/DebuggerError.hpp"

#include <cstring>
#include <vector>

TEST(ProcessReadWrite) {
	Debugger dbg;
	const char *argv[] = {"./TestApp64", nullptr};
	std::shared_ptr<Process> proc;
	try {
		proc = dbg.spawn(nullptr, argv, nullptr);
	} catch (const DebuggerError &e) {
		printf("Skipping test: spawn failed: %s\n", e.what());
		fflush(stdout);
		return;
	}
	CHECK(proc != nullptr);

	pid_t pid = proc->pid();

	// Find writable heap region
	auto regions  = enumerate_regions(pid);
	uint64_t addr = 0;
	for (auto &r : regions) {
		if (r.is_writable() && r.is_heap()) {
			addr = r.start();
			break;
		}
	}

	CHECK(addr != 0);

	const size_t N = 8;
	std::vector<uint8_t> orig(N);
	std::vector<uint8_t> pattern(N);
	for (size_t i = 0; i < N; ++i)
		pattern[i] = static_cast<uint8_t>(0xAA + i);

	const int64_t r = proc->read_memory(addr, orig.data(), N);
	CHECK(r == static_cast<int64_t>(N));

	const int64_t w = proc->write_memory(addr, pattern.data(), N);
	CHECK(w == static_cast<int64_t>(N));

	std::vector<uint8_t> check(N);
	const int64_t r2 = proc->read_memory(addr, check.data(), N);
	CHECK(r2 == static_cast<int64_t>(N));
	CHECK(std::memcmp(check.data(), pattern.data(), N) == 0);

	// restore
	const int64_t w2 = proc->write_memory(addr, orig.data(), N);
	CHECK(w2 == static_cast<int64_t>(N));

	proc->kill();
}
