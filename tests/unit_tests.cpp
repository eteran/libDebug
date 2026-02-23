#include "Debug/Proc.hpp"
#include "Debug/Region.hpp"
#include "Debug/RegisterRef.hpp"
#include "Test.hpp"

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <unistd.h>

TEST(RegisterRef) {

	constexpr uint32_t TestValue = 0xdeadbeef;

	uint64_t val = 0x1122334455667788ULL;
	auto r       = make_register("rax", val, 0);
	CHECK_MSG(r.as<uint64_t>() == val, "RegisterRef::as<uint64_t>() returned unexpected value");

	r = TestValue;
	CHECK_MSG((r.as<uint64_t>() & 0xffffffffULL) == TestValue, "RegisterRef low 32 bits mismatch");

	r += 1;
	CHECK_MSG((r.as<uint64_t>() & 0xffffffffULL) == TestValue + 1, "RegisterRef increment (+=1) failed");

	++r;
	CHECK_MSG((r.as<uint64_t>() & 0xffffffffULL) == TestValue + 2, "RegisterRef pre-increment failed");
}

TEST(Region) {
	Region rr(0x1000, 0x2000, 0, Region::Read | Region::Write, "[heap]");
	CHECK_MSG(rr.is_heap(), "Region not detected as heap");
	CHECK_MSG(rr.is_readable(), "Region not readable");
	CHECK_MSG(rr.is_writable(), "Region not writable");
}

TEST(ProcHelper) {
	pid_t pid    = getpid();
	auto threads = enumerate_threads(pid);
	CHECK_MSG(!threads.empty(), "enumerate_threads returned empty list");

	bool found_self = false;
	for (auto t : threads)
		if (t == pid) found_self = true;
	CHECK_MSG(found_self, "enumerate_threads did not include current pid");

	auto regions = enumerate_regions(pid);
	CHECK_MSG(!regions.empty(), "enumerate_regions returned empty list");

	auto h = hash_regions(pid);
	CHECK_MSG(h != 0 || !regions.empty(), "hash_regions returned zero and regions is empty");
}
