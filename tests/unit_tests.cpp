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
	CHECK(r.as<uint64_t>() == val);

	r = TestValue;
	CHECK((r.as<uint64_t>() & 0xffffffffULL) == TestValue);

	r += 1;
	CHECK((r.as<uint64_t>() & 0xffffffffULL) == TestValue + 1);

	++r;
	CHECK((r.as<uint64_t>() & 0xffffffffULL) == TestValue + 2);
}

TEST(Region) {
	Region rr(0x1000, 0x2000, 0, Region::Read | Region::Write, "[heap]");
	CHECK(rr.is_heap());
	CHECK(rr.is_readable());
	CHECK(rr.is_writable());
}

TEST(ProcHelper) {
	pid_t pid    = getpid();
	auto threads = enumerate_threads(pid);
	CHECK(!threads.empty());

	bool found_self = false;
	for (auto t : threads)
		if (t == pid) found_self = true;
	CHECK(found_self);

	auto regions = enumerate_regions(pid);
	CHECK(!regions.empty());

	auto h = hash_regions(pid);
	CHECK(h != 0 || !regions.empty());
}
