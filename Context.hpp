
#ifndef CONTEXT_HPP_
#define CONTEXT_HPP_

#include <cstddef>
#include <cstdint>

#ifdef __x86_64__

enum class RegisterId {

	Invalid,

	ORIG_RAX,

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
	DR6,
	DR7,

	EFLAGS,

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

#ifdef __x86_64__
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
#endif
};

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
	uint64_t eflags;
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

class Context {
public:
	[[nodiscard]] uint64_t &register_ref(RegisterId reg);
	[[nodiscard]] size_t type() const { return type_; }

public:
	void fill_from(const void *buffer, size_t n);
	void store_to(void *buffer, size_t n) const;

private:
	void store_to_x86_32(Context_x86_32 *ctx) const;
	void fill_from_x86_32(const Context_x86_32 *ctx);

private:
	// NOTE(eteran): normalizing on x86-64 for simplicity
	Context_x86_64 regs_;
	size_t type_ = sizeof(Context_x86_64);
};

#elif defined(__arm__)

struct Context_arm {
	uint32_t regs[18];
};

static_assert(sizeof(Context_arm) == 72, "Context_arm is messed up!");

class Context {
public:
	Context_arm regs_;
};
#else

#endif

#endif
