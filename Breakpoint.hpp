
#ifndef BREAKPOINT_H_
#define BREAKPOINT_H_

#include "Process.hpp"
#include <cstdint>
#include <memory>

class Breakpoint {
public:
	Breakpoint(const Process *process, uint64_t address);
	~Breakpoint();

public:
	uint64_t address() const { return address_; }
	size_t size() const { return size_; }
	void enable();
	void disable();

private:
	const Process *process_ = nullptr;
	uint64_t address_       = 0;
	uint8_t prev_[16]       = {};
	size_t size_            = 0;
};

#endif
