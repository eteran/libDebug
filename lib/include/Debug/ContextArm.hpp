
#ifndef CONTEXT_ARM_HPP_
#define CONTEXT_ARM_HPP_

#include "RegisterRef.hpp"

#include <cmath>
#include <cstddef>
#include <cstdint>

enum class RegisterId {

	Invalid,

	ORIG_R0, // Original value of R0 before a syscall (used for syscall return value)

	R0,  // First argument / Return value
	R1,  // Second argument
	R2,  // Third argument
	R3,  // Fourth argument
	R4,  // General purpose
	R5,  // General purpose
	R6,  // General purpose
	R7,  // General purpose / Syscall number
	R8,  // General purpose
	R9,  // General purpose
	R10, // General purpose
	R11, // Frame pointer
	R12, // Intra-procedure-call scratch register

	SP,   // Stack pointer
	LR,   // Link register
	PC,   // Program counter
	CPSR, // Current program status register

	X0,
	X1,
	X2,
	X3,
	X4,
	X5,
	X6,
	X7,
	X8,
	X9,
	X10,
	X11,
	X12,
	X13,
	X14,
	X15,
	X16,
	X17,
	X18,
	X19,
	X20,
	X21,
	X22,
	X23,
	X24,
	X25,
	X26,
	X27,
	X28,
	X29,
	X30,

	XSP,     // Stack pointer
	XPC,     // Program counter
	XPSTATE, // Current program status register

};

struct Context_Arm_32 {
	uint32_t regs[18];
};

struct Context_Arm_64 {
	uint64_t regs[31];
	uint64_t sp;
	uint64_t pc;
	uint64_t pstate;
};

static_assert(sizeof(Context_Arm_32) == 72, "Context_Arm_32 is messed up!");
static_assert(sizeof(Context_Arm_64) == 272, "Context_Arm_64 is messed up!");

class Context {
	friend class Thread;

public:
	static constexpr size_t BufferAlign = std::max(alignof(Context_Arm_64), alignof(Context_Arm_32));
	static constexpr size_t BufferSize  = std::max(sizeof(Context_Arm_64), sizeof(Context_Arm_32));

public:
	void dump();
	[[nodiscard]] RegisterRef operator[](RegisterId reg) const;
	[[nodiscard]] RegisterRef get(RegisterId reg) const;
	[[nodiscard]] bool is_64_bit() const { return is_64_bit_; }
	[[nodiscard]] bool is_set() const { return is_set_; }

private:
	[[nodiscard]] RegisterRef get_64(RegisterId reg) const;
	[[nodiscard]] RegisterRef get_32(RegisterId reg) const;

public:
private:
	struct Context64 {
		Context_Arm_64 regs;
	};

	static_assert(std::is_standard_layout_v<Context64>, "Context64 is not standard layout");

	struct Context32 {
		Context_Arm_32 regs;
	};

	static_assert(std::is_standard_layout_v<Context32>, "Context32 is not standard layout");

	union {
		Context64 ctx_64_ = {};
		Context32 ctx_32_;
	};

	bool is_64_bit_ = false;
	bool is_set_    = false;
};

#endif
