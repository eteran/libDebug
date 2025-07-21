
#ifndef BREAKPOINT_HPP_
#define BREAKPOINT_HPP_

#include "Process.hpp"
#include <cstdint>
#include <memory>

// TODO(eteran): make this platform agnostic.

class Breakpoint {
public:
	static constexpr size_t minBreakpointSize = 1;
	static constexpr size_t maxBreakpointSize = 2;

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
	[[nodiscard]] uint64_t address() const { return address_; }
	[[nodiscard]] size_t size() const { return size_; }
	void enable();
	void disable();
	void hit();
	[[nodiscard]] TypeId type() const { return type_; }
	[[nodiscard]] uint8_t *old_bytes() { return old_bytes_; }
	[[nodiscard]] uint8_t *new_bytes() { return new_bytes_; }

private:
	const Process *process_ = nullptr;
	uint64_t address_       = 0;
	uint64_t hit_count_     = 0;
	uint8_t old_bytes_[2]   = {};
	uint8_t new_bytes_[2]   = {};
	size_t size_            = 0;
	TypeId type_            = TypeId::Automatic;
	bool enabled_           = false;
};

#endif
