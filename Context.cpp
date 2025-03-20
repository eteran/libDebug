
#include "Context.hpp"

#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>

/**
 * @brief Returns a reference to the given register in a 64-bit context.
 *
 * @param reg The register to return a reference to.
 * @return A reference to the given register.
 */
uint64_t &Context::register_ref_64(RegisterId reg) {
	switch (reg) {
#ifdef __x86_64__
	case RegisterId::R15:
		return regs_64_.r15;
	case RegisterId::R14:
		return regs_64_.r14;
	case RegisterId::R13:
		return regs_64_.r13;
	case RegisterId::R12:
		return regs_64_.r12;
	case RegisterId::RBP:
		return regs_64_.rbp;
	case RegisterId::RBX:
		return regs_64_.rbx;
	case RegisterId::R11:
		return regs_64_.r11;
	case RegisterId::R10:
		return regs_64_.r10;
	case RegisterId::R9:
		return regs_64_.r9;
	case RegisterId::R8:
		return regs_64_.r8;
	case RegisterId::RAX:
		return regs_64_.rax;
	case RegisterId::RCX:
		return regs_64_.rcx;
	case RegisterId::RDX:
		return regs_64_.rdx;
	case RegisterId::RSI:
		return regs_64_.rsi;
	case RegisterId::RDI:
		return regs_64_.rdi;
	case RegisterId::ORIG_RAX:
		return regs_64_.orig_rax;
	case RegisterId::RIP:
		return regs_64_.rip;
	case RegisterId::CS:
		return regs_64_.cs;
	case RegisterId::EFLAGS:
		return regs_64_.eflags;
	case RegisterId::RSP:
		return regs_64_.rsp;
	case RegisterId::SS:
		return regs_64_.ss;
	case RegisterId::FS_BASE:
		return regs_64_.fs_base;
	case RegisterId::GS_BASE:
		return regs_64_.gs_base;
	case RegisterId::DS:
		return regs_64_.ds;
	case RegisterId::ES:
		return regs_64_.es;
	case RegisterId::FS:
		return regs_64_.fs;
	case RegisterId::GS:
		return regs_64_.gs;
#endif
	default:
		std::printf("Unknown Register [64]: %d\n", static_cast<int>(reg));
		std::abort();
	}
}

/**
 * @brief Returns a reference to the given register in a 32-bit context.
 *
 * @param reg The register to return a reference to.
 * @return A reference to the given register.
 */
uint64_t &Context::register_ref_32(RegisterId reg) {
	switch (reg) {
#ifdef __i386__
	case RegisterId::EAX:
		return regs_32_.eax;
	case RegisterId::ECX:
		return regs_32_.ecx;
	case RegisterId::EDX:
		return regs_32_.edx;
	case RegisterId::ESI:
		return regs_32_.esi;
	case RegisterId::EDI:
		return regs_32_.edi;
	case RegisterId::ORIG_EAX:
		return regs_64_.orig_eax;
	case RegisterId::EIP:
		return regs_32_.eip;
	case RegisterId::CS:
		return regs_64_.cs;
	case RegisterId::EFLAGS:
		return regs_64_.eflags;
	case RegisterId::ESP:
		return regs_32_.esp;
	case RegisterId::SS:
		return regs_64_.ss;
	case RegisterId::FS_BASE:
		return regs_64_.fs_base;
	case RegisterId::GS_BASE:
		return regs_64_.gs_base;
	case RegisterId::DS:
		return regs_64_.ds;
	case RegisterId::ES:
		return regs_64_.es;
	case RegisterId::FS:
		return regs_64_.fs;
	case RegisterId::GS:
		return regs_64_.gs;
#endif
	default:
		std::printf("Unknown Register [32]: %d\n", static_cast<int>(reg));
		std::abort();
	}
}

/**
 * @brief Returns a reference to the given register.
 *
 * @param reg The register to return a reference to.
 * @return A reference to the given register.
 */
uint64_t &Context::register_ref(RegisterId reg) {

	if (is_64_bit_) {
		return register_ref_64(reg);
	} else {
		return register_ref_32(reg);
	}
}
