
#include "Breakpoint.hpp"
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

constexpr uint8_t BreakpointInstructionINT3[]  = {0xcc};
constexpr uint8_t BreakpointInstructionINT1[]  = {0xf1};
constexpr uint8_t BreakpointInstructionHLT[]   = {0xf4};
constexpr uint8_t BreakpointInstructionCLI[]   = {0xfa};
constexpr uint8_t BreakpointInstructionSTI[]   = {0xfb};
constexpr uint8_t BreakpointInstructionINSB[]  = {0x6c};
constexpr uint8_t BreakpointInstructionINSD[]  = {0x6d};
constexpr uint8_t BreakpointInstructionOUTSB[] = {0x6e};
constexpr uint8_t BreakpointInstructionOUTSD[] = {0x6f};
constexpr uint8_t BreakpointInstructionUD2[]   = {0x0f, 0x0b};
constexpr uint8_t BreakpointInstructionUD0[]   = {0x0f, 0xff};

}

/**
 * @brief Construct a new Breakpoint object, and then enable it
 *
 * @param process
 * @param address
 * @param type
 */
Breakpoint::Breakpoint(const Process *process, uint64_t address, TypeId type)
	: process_(process), address_(address), type_(type) {

	switch (type) {
	case TypeId::Automatic:
	case TypeId::INT3:
		size_ = sizeof(BreakpointInstructionINT3);
		::memcpy(new_bytes_, BreakpointInstructionINT3, size_);
		break;
	case TypeId::INT1:
		size_ = sizeof(BreakpointInstructionINT1);
		::memcpy(new_bytes_, BreakpointInstructionINT1, size_);
		break;
	case TypeId::HLT:
		size_ = sizeof(BreakpointInstructionHLT);
		::memcpy(new_bytes_, BreakpointInstructionHLT, size_);
		break;
	case TypeId::CLI:
		size_ = sizeof(BreakpointInstructionCLI);
		::memcpy(new_bytes_, BreakpointInstructionCLI, size_);
		break;
	case TypeId::STI:
		size_ = sizeof(BreakpointInstructionSTI);
		::memcpy(new_bytes_, BreakpointInstructionSTI, size_);
		break;
	case TypeId::INSB:
		size_ = sizeof(BreakpointInstructionINSB);
		::memcpy(new_bytes_, BreakpointInstructionINSB, size_);
		break;
	case TypeId::INSD:
		size_ = sizeof(BreakpointInstructionINSD);
		::memcpy(new_bytes_, BreakpointInstructionINSD, size_);
		break;
	case TypeId::OUTSB:
		size_ = sizeof(BreakpointInstructionOUTSB);
		::memcpy(new_bytes_, BreakpointInstructionOUTSB, size_);
		break;
	case TypeId::OUTSD:
		size_ = sizeof(BreakpointInstructionOUTSD);
		::memcpy(new_bytes_, BreakpointInstructionOUTSD, size_);
		break;
	case TypeId::UD2:
		size_ = sizeof(BreakpointInstructionUD2);
		::memcpy(new_bytes_, BreakpointInstructionUD2, size_);
		break;
	case TypeId::UD0:
		size_ = sizeof(BreakpointInstructionUD0);
		::memcpy(new_bytes_, BreakpointInstructionUD0, size_);
		break;
	default:
		::abort();
	}

	enable();
}

/**
 * @brief disable and then destroy the Breakpoint object
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
	const int64_t r = process_->read_memory(address_, old_bytes_, size_);
	if (r == -1) {
		::perror("read_memory");
		::abort();
	}

	const int64_t w = process_->write_memory(address_, new_bytes_, size_);
	if (w == -1) {
		::perror("write_memory");
		::abort();
	}
}

/**
 * @brief disables the breakpoint by restoring the backed up bytes at the target address
 *
 */
void Breakpoint::disable() {
	const int64_t w = process_->write_memory(address_, old_bytes_, size_);
	if (w == -1) {
		::perror("write_memory");
		::abort();
	}
}

/**
 * @brief increments the hit count for the breakpoint
 *
 */
void Breakpoint::hit() {
	hit_count_++;
}
