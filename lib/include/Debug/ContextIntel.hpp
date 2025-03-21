
#ifndef CONTEXT_INTEL_HPP_
#define CONTEXT_INTEL_HPP_

#include "RegisterRef.hpp"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <type_traits>

enum class RegisterId {

	Invalid,

	ORIG_EAX,

	GS,
	FS,
	ES,
	DS,
	CS,
	SS,
	FS_BASE,
	GS_BASE,

	DR0,
	DR1,
	DR2,
	DR3,
	DR4,
	DR5,
	DR6,
	DR7,

	EFLAGS,
	RFLAGS,

	EAX,
	AX,
	AH,
	AL,
	EBX,
	BX,
	BH,
	BL,
	ECX,
	CX,
	CH,
	CL,
	EDX,
	DX,
	DH,
	DL,
	EDI,
	DI,
	ESI,
	SI,
	EBP,
	BP,
	ESP,
	SP,
	EIP,

	ORIG_RAX,
	RAX,
	RBX,
	RCX,
	RDX,
	RSI,
	SIL,
	RDI,
	DIL,
	RBP,
	BPL,
	RSP,
	SPL,
	RIP,
	R8,
	R8D,
	R8W,
	R8B,
	R9,
	R9D,
	R9W,
	R9B,
	R10,
	R10D,
	R10W,
	R10B,
	R11,
	R11D,
	R11W,
	R11B,
	R12,
	R12D,
	R12W,
	R12B,
	R13,
	R13D,
	R13W,
	R13B,
	R14,
	R14D,
	R14W,
	R14B,
	R15,
	R15D,
	R15W,
	R15B,
};

// Reflects user_regs_struct in sys/user.h.
struct Context_x86_32 {
	uint32_t ebx;
	uint32_t ecx;
	uint32_t edx;
	uint32_t esi;
	uint32_t edi;
	uint32_t ebp;
	uint32_t eax;
	uint32_t ds;
	uint32_t es;
	uint32_t fs;
	uint32_t gs;
	uint32_t orig_eax;
	uint32_t eip;
	uint32_t cs;
	uint32_t eflags;
	uint32_t esp;
	uint32_t ss;
};

static_assert(sizeof(Context_x86_32) == 68, "Context_x86_32 is messed up!");
static_assert(std::is_standard_layout<Context_x86_32>::value, "Not standard layout");

// Reflects user_fpxregs_struct in sys/user.h
struct Context_x86_32_xstate {
	uint16_t cwd;
	uint16_t swd;
	uint16_t twd;
	uint16_t fop;
	uint32_t fip;
	uint32_t fcs;
	uint32_t foo;
	uint32_t fos;
	uint32_t mxcsr;
	uint32_t reserved;
	uint32_t st_space[32];
	uint32_t xmm_space[32];
	uint32_t padding[56];
};

// Reflects user_regs_struct in sys/user.h.
struct Context_x86_64 {
	uint64_t r15;
	uint64_t r14;
	uint64_t r13;
	uint64_t r12;
	uint64_t rbp;
	uint64_t rbx;
	uint64_t r11;
	uint64_t r10;
	uint64_t r9;
	uint64_t r8;
	uint64_t rax;
	uint64_t rcx;
	uint64_t rdx;
	uint64_t rsi;
	uint64_t rdi;
	uint64_t orig_rax;
	uint64_t rip;
	uint64_t cs;
	uint64_t rflags;
	uint64_t rsp;
	uint64_t ss;
	uint64_t fs_base;
	uint64_t gs_base;
	uint64_t ds;
	uint64_t es;
	uint64_t fs;
	uint64_t gs;
};

static_assert(sizeof(Context_x86_64) == 216, "Context_x86_64 is messed up!");
static_assert(std::is_standard_layout<Context_x86_64>::value, "Not standard layout");

// Reflects user_fpregs_struct in sys/user.h
struct Context_x86_64_xstate {
	uint16_t cwd;
	uint16_t swd;
	uint16_t ftw;
	uint16_t fop;
	uint64_t rip;
	uint64_t rdp;
	uint32_t mxcsr;
	uint32_t mxcr_mask;
	uint32_t st_space[32];
	uint32_t xmm_space[64];
	uint32_t padding[24];
};

class Context {
	friend class Thread;

public:
	static constexpr size_t BufferAlign = std::max(alignof(Context_x86_64), alignof(Context_x86_32));
	static constexpr size_t BufferSize  = std::max(sizeof(Context_x86_64), sizeof(Context_x86_32));

public:
	[[nodiscard]] RegisterRef get(RegisterId reg);
	[[nodiscard]] bool is_64_bit() const { return is_64_bit_; }
	[[nodiscard]] bool is_set() const { return is_set_; }

private:
	[[nodiscard]] RegisterRef get_64(RegisterId reg);
	[[nodiscard]] RegisterRef get_32(RegisterId reg);

private:
	struct Context64 {
		Context_x86_64 regs;
		Context_x86_64_xstate xstate;
		uint64_t debug_regs[8];
	};

	static_assert(std::is_standard_layout_v<Context64>, "Context64 is not standard layout");

	struct Context32 {
		Context_x86_32 regs;
		Context_x86_32_xstate xstate;
		uint32_t debug_regs[8];
		uint32_t fs_base;
		uint32_t gs_base;
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
