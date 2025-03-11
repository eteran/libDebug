
#ifndef BREAKPOINT_HPP_
#define BREAKPOINT_HPP_

#include "Process.hpp"
#include <cstdint>
#include <memory>

class Breakpoint {
public:
	enum class TypeId : int {
		Automatic = 0,
		INT3,
		INT1,
		HLT,
		CLI,
		STI,
		INSB,
		INSD,
		OUTSB,
		OUTSD,
		UD2,
		UD0,

		TYPE_COUNT,
	};

public:
	Breakpoint(const Process *process, uint64_t address, TypeId type = TypeId::Automatic);
	~Breakpoint();

public:
	uint64_t address() const { return address_; }
	size_t size() const { return size_; }
	void enable();
	void disable();
	void hit();
	TypeId type() const { return type_; }
	uint8_t *old_bytes() { return old_bytes_; }
	uint8_t *new_bytes() { return new_bytes_; }

private:
	const Process *process_ = nullptr;
	uint64_t address_       = 0;
	uint64_t hit_count_     = 0;
	uint8_t old_bytes_[2]   = {};
	uint8_t new_bytes_[2]   = {};
	size_t size_            = 0;
	TypeId type_            = TypeId::Automatic;
};

#endif
