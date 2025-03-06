
#include "Breakpoint.hpp"
#include <cassert>

namespace {

const uint8_t BreakPoint[] = {0xcc};

}

Breakpoint::Breakpoint(const Process *process, uint64_t address)
	: process_(process), address_(address), size_(sizeof(BreakPoint)) {

	enable();
}

Breakpoint::~Breakpoint() {
	disable();
}

void Breakpoint::enable() {
	const int64_t r = process_->read_memory(address_, prev_, size_);
	assert(r == 1);

	const int64_t w = process_->write_memory(address_, &BreakPoint, size_);
	assert(w == 1);
}

void Breakpoint::disable() {
	const int64_t w = process_->write_memory(address_, prev_, size_);
	assert(w == 1);
}
