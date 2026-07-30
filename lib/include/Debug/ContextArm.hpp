
#ifndef CONTEXT_ARM_HPP_
#define CONTEXT_ARM_HPP_

#include "RegisterRef.hpp"

#include <cmath>
#include <cstddef>
#include <cstdint>

enum class RegisterId {

	Invalid,

	// 32-bit registers
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

	// 64-bit registers
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
	W0,
	W1,
	W2,
	W3,
	W4,
	W5,
	W6,
	W7,
	W8,
	W9,
	W10,
	W11,
	W12,
	W13,
	W14,
	W15,
	W16,
	W17,
	W18,
	W19,
	W20,
	W21,
	W22,
	W23,
	W24,
	W25,
	W26,
	W27,
	W28,
	W29,
	W30,

	XSP,     // Stack pointer
	XPC,     // Program counter
	XPSTATE, // Current program status register

	// VFP / SIMD
	FPSR,
	FPCR,
	V0,
	V1,
	V2,
	V3,
	V4,
	V5,
	V6,
	V7,
	V8,
	V9,
	V10,
	V11,
	V12,
	V13,
	V14,
	V15,
	V16,
	V17,
	V18,
	V19,
	V20,
	V21,
	V22,
	V23,
	V24,
	V25,
	V26,
	V27,
	V28,
	V29,
	V30,
	V31,

	// Generic names
	INSTRUCTION_POINTER,
	STACK_POINTER,
	FLAGS_REGISTER,

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

struct Context_Arm_Vfp {
	uint32_t vregs[32][4];
	uint32_t fpsr;
	uint32_t fpcr;
};

static_assert(sizeof(Context_Arm_32) == 72, "Context_Arm_32 is messed up!");
static_assert(sizeof(Context_Arm_64) == 272, "Context_Arm_64 is messed up!");
static_assert(sizeof(Context_Arm_Vfp) == 520, "Context_Arm_Vfp is messed up!");

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

	union {
		Context_Arm_Vfp vfp_regs_;
	};

	bool vfp_filled_ = false;
	bool is_64_bit_ = false;
	bool is_set_    = false;
};

#endif
