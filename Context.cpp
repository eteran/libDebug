
#include "Context.hpp"
#include <cstdio>

/**
 * @brief Returns a reference to the given register in a 64-bit context.
 *
 * @param reg The register to return a reference to.
 * @return A reference to the given register.
 */
RegisterRef Context::get_64(RegisterId reg) {

	using T = std::underlying_type_t<RegisterId>;

	switch (reg) {

	// Segment Registers
	case RegisterId::CS:
		return make_register(regs_64_.cs);
	case RegisterId::DS:
		return make_register(regs_64_.ds);
	case RegisterId::ES:
		return make_register(regs_64_.es);
	case RegisterId::FS:
		return make_register(regs_64_.fs);
	case RegisterId::GS:
		return make_register(regs_64_.gs);
	case RegisterId::SS:
		return make_register(regs_64_.ss);
	case RegisterId::FS_BASE:
		return make_register(regs_64_.fs_base);
	case RegisterId::GS_BASE:
		return make_register(regs_64_.gs_base);

	// 64-bit GP registers
	case RegisterId::R15:
		return make_register(regs_64_.r15);
	case RegisterId::R14:
		return make_register(regs_64_.r14);
	case RegisterId::R13:
		return make_register(regs_64_.r13);
	case RegisterId::R12:
		return make_register(regs_64_.r12);
	case RegisterId::RBP:
		return make_register(regs_64_.rbp);
	case RegisterId::RBX:
		return make_register(regs_64_.rbx);
	case RegisterId::R11:
		return make_register(regs_64_.r11);
	case RegisterId::R10:
		return make_register(regs_64_.r10);
	case RegisterId::R9:
		return make_register(regs_64_.r9);
	case RegisterId::R8:
		return make_register(regs_64_.r8);
	case RegisterId::RAX:
		return make_register(regs_64_.rax);
	case RegisterId::RCX:
		return make_register(regs_64_.rcx);
	case RegisterId::RDX:
		return make_register(regs_64_.rdx);
	case RegisterId::RSI:
		return make_register(regs_64_.rsi);
	case RegisterId::RDI:
		return make_register(regs_64_.rdi);
	case RegisterId::ORIG_RAX:
		return make_register(regs_64_.orig_rax);
	case RegisterId::RIP:
		return make_register(regs_64_.rip);
	case RegisterId::EFLAGS:
		return make_register(regs_64_.rflags);
	case RegisterId::RSP:
		return make_register(regs_64_.rsp);

	// 32-bit GP registers
	case RegisterId::EAX:
		return make_register(regs_64_.rax, sizeof(uint32_t));
	case RegisterId::EBX:
		return make_register(regs_64_.rbx, sizeof(uint32_t));
	case RegisterId::ECX:
		return make_register(regs_64_.rcx, sizeof(uint32_t));
	case RegisterId::EDX:
		return make_register(regs_64_.rdx, sizeof(uint32_t));
	case RegisterId::ESI:
		return make_register(regs_64_.rsi, sizeof(uint32_t));
	case RegisterId::EDI:
		return make_register(regs_64_.rdi, sizeof(uint32_t));
	case RegisterId::ORIG_EAX:
		return make_register(regs_64_.orig_rax, sizeof(uint32_t));
	case RegisterId::EIP:
		return make_register(regs_64_.rip, sizeof(uint32_t));
	case RegisterId::ESP:
		return make_register(regs_64_.rsp, sizeof(uint32_t));
	case RegisterId::EBP:
		return make_register(regs_64_.rbp, sizeof(uint32_t));

	// Debug Registers
	case RegisterId::DR0:
	case RegisterId::DR1:
	case RegisterId::DR2:
	case RegisterId::DR3:
	case RegisterId::DR4:
	case RegisterId::DR5:
	case RegisterId::DR6:
	case RegisterId::DR7:
		return make_register(debug_regs_[static_cast<T>(reg) - static_cast<T>(RegisterId::DR0)]);
	default:
		std::printf("Unknown Register [64]: %d\n", static_cast<T>(reg));
		return RegisterRef();
	}
}

/**
 * @brief Returns a reference to the given register in a 32-bit context.
 *
 * @param reg The register to return a reference to.
 * @return A reference to the given register.
 */
RegisterRef Context::get_32(RegisterId reg) {

	using T = std::underlying_type_t<RegisterId>;

	switch (reg) {
	case RegisterId::EAX:
		return make_register(regs_32_.eax);
	case RegisterId::EBX:
		return make_register(regs_32_.ebx);
	case RegisterId::ECX:
		return make_register(regs_32_.ecx);
	case RegisterId::EDX:
		return make_register(regs_32_.edx);
	case RegisterId::ESI:
		return make_register(regs_32_.esi);
	case RegisterId::EDI:
		return make_register(regs_32_.edi);
	case RegisterId::ORIG_EAX:
		return make_register(regs_32_.orig_eax);
	case RegisterId::EIP:
		return make_register(regs_32_.eip);
	case RegisterId::CS:
		return make_register(regs_32_.cs);
	case RegisterId::EFLAGS:
		return make_register(regs_32_.eflags);
	case RegisterId::ESP:
		return make_register(regs_32_.esp);
	case RegisterId::EBP:
		return make_register(regs_32_.ebp);
	case RegisterId::SS:
		return make_register(regs_32_.ss);
	case RegisterId::DS:
		return make_register(regs_32_.ds);
	case RegisterId::ES:
		return make_register(regs_32_.es);
	case RegisterId::FS:
		return make_register(regs_32_.fs);
	case RegisterId::GS:
		return make_register(regs_32_.gs);

	case RegisterId::FS_BASE:
		return RegisterRef();
	case RegisterId::GS_BASE:
		return RegisterRef();

	// Debug Registers
	case RegisterId::DR0:
	case RegisterId::DR1:
	case RegisterId::DR2:
	case RegisterId::DR3:
	case RegisterId::DR4:
	case RegisterId::DR5:
	case RegisterId::DR6:
	case RegisterId::DR7:
		return make_register(debug_regs_[static_cast<T>(reg) - static_cast<T>(RegisterId::DR0)]);

	default:
		std::printf("Unknown Register [32]: %d\n", static_cast<int>(reg));
		return RegisterRef();
	}
}

/**
 * @brief Returns a reference to the given register.
 *
 * @param reg The register to return a reference to.
 * @return A reference to the given register.
 */
RegisterRef Context::get(RegisterId reg) {

#if defined(__x86_64__)
	return get_64(reg);
#elif defined(__i386__)
	if (is_64_bit_) {
		return get_64(reg);
	} else {
		return get_32(reg);
	}
#endif
}
