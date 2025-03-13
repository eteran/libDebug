
#include "Context.hpp"

#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {
#ifdef __x86_64__
void fill_from_x86_32(Context_x86_64 *ctx, const Context_x86_32 *regs) {
	ctx->rax      = regs->eax;
	ctx->rbx      = regs->ebx;
	ctx->rcx      = regs->ecx;
	ctx->rdx      = regs->edx;
	ctx->rsi      = regs->esi;
	ctx->rdi      = regs->edi;
	ctx->rbp      = regs->ebp;
	ctx->rsp      = regs->esp;
	ctx->rip      = regs->eip;
	ctx->eflags   = regs->eflags;
	ctx->cs       = regs->cs;
	ctx->ss       = regs->ss;
	ctx->ds       = regs->ds;
	ctx->es       = regs->es;
	ctx->fs       = regs->fs;
	ctx->gs       = regs->gs;
	ctx->orig_rax = regs->orig_eax;
}

void store_to_x86_32(Context_x86_32 *ctx, const Context_x86_64 *regs) {
	ctx->eax      = static_cast<uint32_t>(regs->rax);
	ctx->ebx      = static_cast<uint32_t>(regs->rbx);
	ctx->ecx      = static_cast<uint32_t>(regs->rcx);
	ctx->edx      = static_cast<uint32_t>(regs->rdx);
	ctx->esi      = static_cast<uint32_t>(regs->rsi);
	ctx->edi      = static_cast<uint32_t>(regs->rdi);
	ctx->ebp      = static_cast<uint32_t>(regs->rbp);
	ctx->esp      = static_cast<uint32_t>(regs->rsp);
	ctx->eip      = static_cast<uint32_t>(regs->rip);
	ctx->eflags   = static_cast<uint32_t>(regs->eflags);
	ctx->cs       = static_cast<uint32_t>(regs->cs);
	ctx->ss       = static_cast<uint32_t>(regs->ss);
	ctx->ds       = static_cast<uint32_t>(regs->ds);
	ctx->es       = static_cast<uint32_t>(regs->es);
	ctx->fs       = static_cast<uint32_t>(regs->fs);
	ctx->gs       = static_cast<uint32_t>(regs->gs);
	ctx->orig_eax = static_cast<uint32_t>(regs->orig_rax);
}
#endif

}

void fill_context(Context *ctx, const void *buffer, size_t n) {

	switch (n) {
#ifdef __x86_64__
	case sizeof(Context_x86_64):
		::memcpy(&ctx->regs_, buffer, sizeof(Context_x86_64));
		ctx->type_ = sizeof(Context_x86_64);
		break;
	case sizeof(Context_x86_32):
		// NOTE(eteran): I'd really prefer to use C++23's std::start_lifetime_as here
		Context_x86_32 regs32;
		::memcpy(&regs32, buffer, sizeof(Context_x86_32));
		fill_from_x86_32(&ctx->regs_, &regs32);
		ctx->type_ = sizeof(Context_x86_32);
		break;
#endif
	default:
		printf("Unknown Context Size: %zu\n", n);
		break;
	}
}

void store_context(void *buffer, const Context *ctx, size_t n) {

	switch (ctx->type_) {
#ifdef __x86_64__
	case sizeof(Context_x86_64):
		assert(n >= sizeof(Context_x86_64));
		::memcpy(buffer, &ctx->regs_, sizeof(Context_x86_64));
		break;
	case sizeof(Context_x86_32):
		assert(n >= sizeof(Context_x86_32));
		Context_x86_32 regs32;
		store_to_x86_32(&regs32, &ctx->regs_);
		::memcpy(buffer, &regs32, sizeof(Context_x86_32));
		break;
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
