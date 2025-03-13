
#include "Context.hpp"

#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>

#ifdef __x86_64__
/**
 * @brief store the given context into the given x86_32 context
 *
 * @param ctx the x86_32 context to store the given context into
 */
void Context::store_to_x86_32(Context_x86_32 *ctx) const {
	ctx->eax      = static_cast<uint32_t>(regs_.rax);
	ctx->ebx      = static_cast<uint32_t>(regs_.rbx);
	ctx->ecx      = static_cast<uint32_t>(regs_.rcx);
	ctx->edx      = static_cast<uint32_t>(regs_.rdx);
	ctx->esi      = static_cast<uint32_t>(regs_.rsi);
	ctx->edi      = static_cast<uint32_t>(regs_.rdi);
	ctx->ebp      = static_cast<uint32_t>(regs_.rbp);
	ctx->esp      = static_cast<uint32_t>(regs_.rsp);
	ctx->eip      = static_cast<uint32_t>(regs_.rip);
	ctx->eflags   = static_cast<uint32_t>(regs_.eflags);
	ctx->cs       = static_cast<uint32_t>(regs_.cs);
	ctx->ss       = static_cast<uint32_t>(regs_.ss);
	ctx->ds       = static_cast<uint32_t>(regs_.ds);
	ctx->es       = static_cast<uint32_t>(regs_.es);
	ctx->fs       = static_cast<uint32_t>(regs_.fs);
	ctx->gs       = static_cast<uint32_t>(regs_.gs);
	ctx->orig_eax = static_cast<uint32_t>(regs_.orig_rax);
}

/**
 * @brief fill the given context with the given x86_32 context
 *
 * @param ctx the x86_32 context to fill the given context with
 */
void Context::fill_from_x86_32(const Context_x86_32 *ctx) {
	regs_.rax      = ctx->eax;
	regs_.rbx      = ctx->ebx;
	regs_.rcx      = ctx->ecx;
	regs_.rdx      = ctx->edx;
	regs_.rsi      = ctx->esi;
	regs_.rdi      = ctx->edi;
	regs_.rbp      = ctx->ebp;
	regs_.rsp      = ctx->esp;
	regs_.rip      = ctx->eip;
	regs_.eflags   = ctx->eflags;
	regs_.cs       = ctx->cs;
	regs_.ss       = ctx->ss;
	regs_.ds       = ctx->ds;
	regs_.es       = ctx->es;
	regs_.fs       = ctx->fs;
	regs_.gs       = ctx->gs;
	regs_.orig_rax = ctx->orig_eax;
}
#endif

/**
 * @brief fill the given context with the given buffer
 *
 * @param buffer the buffer to fill the context with
 * @param n the size of the buffer
 */

void Context::fill_from(const void *buffer, size_t n) {
	switch (n) {
#ifdef __x86_64__
	case sizeof(Context_x86_64):
		::memcpy(&regs_, buffer, sizeof(Context_x86_64));
		type_ = sizeof(Context_x86_64);
		break;
	case sizeof(Context_x86_32): {
		// NOTE(eteran): I'd really prefer to use C++23's std::start_lifetime_as here
		// but this will do for now
		__asm__("" ::"r"(buffer) : "memory");
		auto ptr = static_cast<const Context_x86_32 *>(buffer);
		fill_from_x86_32(ptr);
		type_ = sizeof(Context_x86_32);
		break;
	}
#endif
	default:
		printf("Unknown Context Size: %zu\n", n);
		break;
	}
}

/**
 * @brief store the context into the given buffer
 *
 * @param buffer the buffer to store the context into
 * @param n the size of the buffer
 */
void Context::store_to(void *buffer, size_t n) const {
	switch (type_) {
#ifdef __x86_64__
	case sizeof(Context_x86_64):
		assert(n >= sizeof(Context_x86_64));
		::memcpy(buffer, &regs_, sizeof(Context_x86_64));
		break;
	case sizeof(Context_x86_32): {
		assert(n >= sizeof(Context_x86_32));
		// NOTE(eteran): I'd really prefer to use C++23's std::start_lifetime_as here
		// but this will do for now
		__asm__("" ::"r"(buffer) : "memory");
		auto ptr = static_cast<Context_x86_32 *>(buffer);
		store_to_x86_32(ptr);
		break;
	}
#endif
	default:
		printf("Unknown Context Size: %zu\n", n);
		break;
	}
}

/**
 * @brief returns a reference to the given register
 *
 * @param reg
 * @return uint64_t&
 */
uint64_t &Context::register_ref(RegisterId reg) {
	switch (reg) {
#ifdef __x86_64__
	case RegisterId::R15:
		return regs_.r15;
	case RegisterId::R14:
		return regs_.r14;
	case RegisterId::R13:
		return regs_.r13;
	case RegisterId::R12:
		return regs_.r12;
	case RegisterId::RBP:
		return regs_.rbp;
	case RegisterId::RBX:
		return regs_.rbx;
	case RegisterId::R11:
		return regs_.r11;
	case RegisterId::R10:
		return regs_.r10;
	case RegisterId::R9:
		return regs_.r9;
	case RegisterId::R8:
		return regs_.r8;
	case RegisterId::RAX:
		return regs_.rax;
	case RegisterId::RCX:
		return regs_.rcx;
	case RegisterId::RDX:
		return regs_.rdx;
	case RegisterId::RSI:
		return regs_.rsi;
	case RegisterId::RDI:
		return regs_.rdi;
	case RegisterId::ORIG_RAX:
		return regs_.orig_rax;
	case RegisterId::RIP:
		return regs_.rip;
	case RegisterId::CS:
		return regs_.cs;
	case RegisterId::EFLAGS:
		return regs_.eflags;
	case RegisterId::RSP:
		return regs_.rsp;
	case RegisterId::SS:
		return regs_.ss;
	case RegisterId::FS_BASE:
		return regs_.fs_base;
	case RegisterId::GS_BASE:
		return regs_.gs_base;
	case RegisterId::DS:
		return regs_.ds;
	case RegisterId::ES:
		return regs_.es;
	case RegisterId::FS:
		return regs_.fs;
	case RegisterId::GS:
		return regs_.gs;
#endif
	default:
		printf("Unknown Register: %d\n", static_cast<int>(reg));
		::abort();
	}
}
