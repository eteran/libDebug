
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

	// FPU / SIMD
	ST0,
	ST1,
	ST2,
	ST3,
	ST4,
	ST5,
	ST6,
	ST7,

	CWD,        // Control Word
	SWD,        // Status Word
	FTW,        // Tag Word
	FOP,        // Last Instruction Opcode
	FIP,        // Last Instruction EIP
	FDP,        // Last Operand Offset
	MXCSR,      // SSE Control and Status Register
	MXCSR_MASK, // SSE Control and Status Register Mask

	MM0,
	MM1,
	MM2,
	MM3,
	MM4,
	MM5,
	MM6,
	MM7,

	XMM0,
	XMM1,
	XMM2,
	XMM3,
	XMM4,
	XMM5,
	XMM6,
	XMM7,
	XMM8,
	XMM9,
	XMM10,
	XMM11,
	XMM12,
	XMM13,
	XMM14,
	XMM15,
	YMM0,
	YMM1,
	YMM2,
	YMM3,
	YMM4,
	YMM5,
	YMM6,
	YMM7,
	YMM8,
	YMM9,
	YMM10,
	YMM11,
	YMM12,
	YMM13,
	YMM14,
	YMM15,
	ZMM0,
	ZMM1,
	ZMM2,
	ZMM3,
	ZMM4,
	ZMM5,
	ZMM6,
	ZMM7,
	ZMM8,
	ZMM9,
	ZMM10,
	ZMM11,
	ZMM12,
	ZMM13,
	ZMM14,
	ZMM15,

	// Size generic registers
	XAX,
	XCX,
	XDX,
	XSI,
	XDI,
	XIP,
	XSP,
	XFLAGS,
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
	uint16_t fop; // last instruction opcode
	uint32_t fip; // last instruction EIP
	uint32_t fcs; // last instruction CS
	uint32_t foo; // last operand offset
	uint32_t fos; // last operand selector in 32 bit mode
	uint32_t mxcsr;
	uint32_t mxcsr_mask;
	uint8_t st_space[128];  /* 8*16 bytes for each FP-reg = 128 bytes */
	uint32_t xmm_space[16]; /* 8*16 bytes for each XMM-reg = 128 bytes */
	uint32_t padding[60];
	union {
		uint64_t xcr0;
		uint8_t sw_usable_bytes[48];
	};

	union {
		uint64_t xstate_bv;
		uint8_t xstate_hdr_bytes[64];
	};

	uint8_t buffer[2112];
} __attribute__((packed, aligned(64)));

static_assert(offsetof(Context_x86_32_xstate, xstate_bv) == 512, "Context_x86_32_xstate is messed up!");
static_assert(offsetof(Context_x86_32_xstate, st_space) == 32, "ST space should appear at offset 32");
static_assert(offsetof(Context_x86_32_xstate, xmm_space) == 160, "XMM space should appear at offset 160");
static_assert(offsetof(Context_x86_32_xstate, xcr0) == 464, "XCR0 should appear at offset 464");
static_assert(sizeof(Context_x86_32_xstate) == 2688, "Context_x86_32_xstate is messed up!");
static_assert(std::is_standard_layout<Context_x86_32_xstate>::value, "Not standard layout");

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
	uint16_t fop; // last instruction opcode
	uint64_t rip; // last instruction EIP
	uint64_t rdp; // last operand offset
	uint32_t mxcsr;
	uint32_t mxcr_mask;
	uint8_t st_space[16 * 8];   // 8 16-byte fields
	uint8_t xmm_space[16 * 16]; // 16 16-byte fields, regardless of XMM_REG_COUNT
	uint8_t padding[48];

	union {
		uint64_t xcr0;
		uint8_t sw_usable_bytes[48];
	};

	union {
		uint64_t xstate_bv;
		uint8_t xstate_hdr_bytes[64];
	};

	uint8_t buffer[2112];
} __attribute__((packed, aligned(64)));

struct FpuRegister {
	uint8_t data[16];
};

struct AvxRegister {
	uint8_t data[64];
};

struct Context_xstate {

	struct {
		FpuRegister registers[8];
		uint64_t inst_ptr_offset   = 0;
		uint64_t data_ptr_offset   = 0;
		uint16_t inst_ptr_selector = 0;
		uint16_t data_ptr_selector = 0;
		uint16_t control_word      = 0;
		uint16_t status_word       = 0;
		uint16_t tag_word          = 0;
		uint16_t opcode            = 0;
		bool filled                = false;
	} x87;

	struct {
		AvxRegister registers[32]; // XMM0-XMM15, YMM0-YMM15, ZMM0-ZMM15
		uint32_t mxcsr      = 0;
		uint32_t mxcsr_mask = 0;
		bool sse_filled     = false;
		bool avx_filled     = false;
		bool zmm_filled     = false;
	} simd;
};

static_assert(offsetof(Context_x86_64_xstate, xstate_bv) == 512, "Context_x86_64_xstate is messed up!");
static_assert(offsetof(Context_x86_64_xstate, st_space) == 32, "ST space should appear at offset 32");
static_assert(offsetof(Context_x86_64_xstate, xmm_space) == 160, "XMM space should appear at offset 160");
static_assert(offsetof(Context_x86_64_xstate, xcr0) == 464, "XCR0 should appear at offset 464");
static_assert(sizeof(Context_x86_64_xstate) == 2688, "Context_x86_64_xstate is messed up!");
static_assert(std::is_standard_layout<Context_x86_64_xstate>::value, "Not standard layout");

// Reflects user_fpregs_struct in sys/user.h
// This is used for 32-bit processes on x86_64 systems.
struct user_fpregs_struct_32 {
	uint32_t cwd;
	uint32_t swd;
	uint32_t twd;
	uint32_t fip;
	uint32_t fcs;
	uint32_t foo;
	uint32_t fos;
	uint8_t st_space[80];
};

struct user_fpregs_struct_64 {
	uint16_t cwd;
	uint16_t swd;
	uint16_t ftw;
	uint16_t fop;
	uint64_t rip;
	uint64_t rdp;
	uint32_t mxcsr;
	uint32_t mxcr_mask;
	uint8_t st_space[128];  /* 8*16 bytes for each FP-reg = 128 bytes */
	uint8_t xmm_space[256]; /* 16*16 bytes for each XMM-reg = 256 bytes */
	uint8_t padding[96];
};

class Context {
	friend class Thread;

public:
	static constexpr size_t BufferAlign = std::max(alignof(Context_x86_64), alignof(Context_x86_32));
	static constexpr size_t BufferSize  = std::max(sizeof(Context_x86_64), sizeof(Context_x86_32));

public:
	void dump();
	[[nodiscard]] RegisterRef operator[](RegisterId reg);
	[[nodiscard]] RegisterRef get(RegisterId reg);
	[[nodiscard]] bool is_64_bit() const { return is_64_bit_; }
	[[nodiscard]] bool is_set() const { return is_set_; }

private:
	[[nodiscard]] RegisterRef get_64(RegisterId reg);
	[[nodiscard]] RegisterRef get_32(RegisterId reg);

private:
	struct Context64 {
		Context_x86_64 regs;
		uint64_t debug_regs[8];
	};

	static_assert(std::is_standard_layout_v<Context64>, "Context64 is not standard layout");

	struct Context32 {
		Context_x86_32 regs;
		uint32_t debug_regs[8];
		uint32_t fs_base;
		uint32_t gs_base;
	};

	static_assert(std::is_standard_layout_v<Context32>, "Context32 is not standard layout");

	union {
		Context64 ctx_64_ = {};
		Context32 ctx_32_;
	};

	union {
		// NOTE(eteran): these are mutable so that we can make our API logically const
		// and still reuse the same context we read in order to preserve the parts of
		// the xstate that we don't modify when writing back.
		mutable Context_x86_64_xstate ctx_64_xstate_;
		mutable Context_x86_32_xstate ctx_32_xstate_;
	};

	Context_xstate xstate_;

	bool is_64_bit_ = false;
	bool is_set_    = false;
};

#endif
