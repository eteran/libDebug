
#include "Debug/BreakpointArm.hpp"
#include "Debug/DebuggerError.hpp"
#include "Debug/Process.hpp"

#include <cassert>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <signal.h>

namespace {

// NOTE(eteran): All of these instruction have a 16-bit, which we currently set to 0.
// This is the "immediate" value for the instruction, which can be used to encode additional information about the breakpoint.
// But we don't currently use it for anything.
constexpr uint8_t BreakpointInstructionBRK[] = {0x00, 0x00, 0x3E, 0xD4}; // BRK #0
constexpr uint8_t BreakpointInstructionHLT[] = {0x00, 0x00, 0x00, 0xD4}; // HLT #0
constexpr uint8_t BreakpointInstructionUDF[] = {0x00, 0x00, 0x00, 0x00}; // UDF #0

}

/**
 * @brief Construct a new Breakpoint object, and then enable it.
 *
 * @param process The process to set the breakpoint in.
 * @param address The address to set the breakpoint at.
 * @param type The type of breakpoint to set.
 */
Breakpoint::Breakpoint(const Process *process, uint64_t address, TypeId type)
	: process_(process), address_(address), type_(type) {

	switch (type) {
	case TypeId::Automatic:
	case TypeId::BRK:
		size_ = sizeof(BreakpointInstructionBRK);
		std::memcpy(new_bytes_, BreakpointInstructionBRK, size_);
		break;
	case TypeId::HLT:
		size_ = sizeof(BreakpointInstructionHLT);
		std::memcpy(new_bytes_, BreakpointInstructionHLT, size_);
		break;
	case TypeId::UDF:
		size_ = sizeof(BreakpointInstructionUDF);
		std::memcpy(new_bytes_, BreakpointInstructionUDF, size_);
		break;
	default:
		__builtin_unreachable();
	}
}
