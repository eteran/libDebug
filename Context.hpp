
#ifndef CONTEXT_HPP_
#define CONTEXT_HPP_

#include <cstddef>
#include <cstdint>

#ifdef __x86_64__

enum class RegisterId {

	// x86_32 specific
	EAX,
	EBX,
	ECX,
	EDX,
	ESI,
	EDI,
	EBP,
	ESP,
	EIP,
	CS,
	SS,
	FS_BASE,
	GS_BASE,
	DS,
	ES,
	FS,
	GS,

	// x86_64 specific
	R15,
	R14,
	R13,
	R12,
	RBP,
	RBX,
	R11,
	R10,
	R9,
	R8,
	RAX,
	RCX,
	RDX,
	RSI,
	RDI,
	ORIG_RAX,
	RIP,
	EFLAGS,
	RSP,
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
	friend void fill_context(Context *ctx, const void *buffer, size_t n);


public:
	const uint64_t &
	register_ref(RegisterId reg) const;

public:
	// TODO(eteran): make this private
	// NOTE(eteran): normalizing on x86-64 for simplicity.
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

void fill_context(Context *ctx, const void *buffer, size_t n);
void store_context(void *buffer, const Context *ctx, size_t n);

#endif
