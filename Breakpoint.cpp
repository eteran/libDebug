
#include "Breakpoint.hpp"
#include <cassert>

namespace {

const uint8_t BreakPoint[] = {0xcc};

}

/**
 * @brief Construct a new Breakpoint object
 *
 * @param process
 * @param address
 */
Breakpoint::Breakpoint(const Process *process, uint64_t address)
	: process_(process), address_(address), size_(sizeof(BreakPoint)) {

	enable();
}

/**
 * @brief Destroy the Breakpoint object
 *
 */
Breakpoint::~Breakpoint() {
	disable();
}

/**
 * @brief enables the breakpoint by backing up the bytes at the target address as needed
 * and then replacing them with bytes representing a breakpoint
 *
 */
void Breakpoint::enable() {
	const int64_t r = process_->read_memory(address_, prev_, size_);
	assert(r == static_cast<int64_t>(size_));

	const int64_t w = process_->write_memory(address_, &BreakPoint, size_);
	assert(w == static_cast<int64_t>(size_));
}

/**
 * @brief disables the breakpoint by restoring the backed up bytes at the target address
 *
 */
void Breakpoint::disable() {
	const int64_t w = process_->write_memory(address_, prev_, size_);
	assert(w == static_cast<int64_t>(size_));
}
