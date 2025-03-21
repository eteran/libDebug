
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
		return make_register("cs", ctx_64_.regs.cs);
	case RegisterId::DS:
		return make_register("ds", ctx_64_.regs.ds);
	case RegisterId::ES:
		return make_register("es", ctx_64_.regs.es);
	case RegisterId::FS:
		return make_register("fs", ctx_64_.regs.fs);
	case RegisterId::GS:
		return make_register("gs", ctx_64_.regs.gs);
	case RegisterId::SS:
		return make_register("ss", ctx_64_.regs.ss);
	case RegisterId::FS_BASE:
		return make_register("fs_base", ctx_64_.regs.fs_base);
	case RegisterId::GS_BASE:
		return make_register("gs_base", ctx_64_.regs.gs_base);

	// 64-bit GP registers
	case RegisterId::R15:
		return make_register("r15", ctx_64_.regs.r15);
	case RegisterId::R14:
		return make_register("r14", ctx_64_.regs.r14);
	case RegisterId::R13:
		return make_register("r13", ctx_64_.regs.r13);
	case RegisterId::R12:
		return make_register("r12", ctx_64_.regs.r12);
	case RegisterId::RBP:
		return make_register("rbp", ctx_64_.regs.rbp);
	case RegisterId::RBX:
		return make_register("rbx", ctx_64_.regs.rbx);
	case RegisterId::R11:
		return make_register("r11", ctx_64_.regs.r11);
	case RegisterId::R10:
		return make_register("r10", ctx_64_.regs.r10);
	case RegisterId::R9:
		return make_register("r9", ctx_64_.regs.r9);
	case RegisterId::R8:
		return make_register("r8", ctx_64_.regs.r8);
	case RegisterId::RAX:
		return make_register("rax", ctx_64_.regs.rax);
	case RegisterId::RCX:
		return make_register("rcx", ctx_64_.regs.rcx);
	case RegisterId::RDX:
		return make_register("rdx", ctx_64_.regs.rdx);
	case RegisterId::RSI:
		return make_register("rsi", ctx_64_.regs.rsi);
	case RegisterId::RDI:
		return make_register("rdi", ctx_64_.regs.rdi);
	case RegisterId::ORIG_RAX:
		return make_register("orig_rax", ctx_64_.regs.orig_rax);
	case RegisterId::RIP:
		return make_register("rip", ctx_64_.regs.rip);
	case RegisterId::EFLAGS:
		return make_register("rflags", ctx_64_.regs.rflags);
	case RegisterId::RSP:
		return make_register("rsp", ctx_64_.regs.rsp);

	// 32-bit GP registers
	case RegisterId::EAX:
		return make_register("eax", ctx_64_.regs.rax, sizeof(uint32_t));
	case RegisterId::EBX:
		return make_register("ebx", ctx_64_.regs.rbx, sizeof(uint32_t));
	case RegisterId::ECX:
		return make_register("ecx", ctx_64_.regs.rcx, sizeof(uint32_t));
	case RegisterId::EDX:
		return make_register("edx", ctx_64_.regs.rdx, sizeof(uint32_t));
	case RegisterId::ESI:
		return make_register("esi", ctx_64_.regs.rsi, sizeof(uint32_t));
	case RegisterId::EDI:
		return make_register("esi", ctx_64_.regs.rdi, sizeof(uint32_t));
	case RegisterId::ORIG_EAX:
		return make_register("orig_eax", ctx_64_.regs.orig_rax, sizeof(uint32_t));
	case RegisterId::EIP:
		return make_register("eip", ctx_64_.regs.rip, sizeof(uint32_t));
	case RegisterId::ESP:
		return make_register("esp", ctx_64_.regs.rsp, sizeof(uint32_t));
	case RegisterId::EBP:
		return make_register("ebp", ctx_64_.regs.rbp, sizeof(uint32_t));

	// Debug Registers
	case RegisterId::DR0:
		return make_register("dr0", ctx_64_.debug_regs[0]);
	case RegisterId::DR1:
		return make_register("dr1", ctx_64_.debug_regs[1]);
	case RegisterId::DR2:
		return make_register("dr2", ctx_64_.debug_regs[2]);
	case RegisterId::DR3:
		return make_register("dr3", ctx_64_.debug_regs[3]);
	case RegisterId::DR4:
		return make_register("dr4", ctx_64_.debug_regs[4]);
	case RegisterId::DR5:
		return make_register("dr5", ctx_64_.debug_regs[5]);
	case RegisterId::DR6:
		return make_register("dr6", ctx_64_.debug_regs[6]);
	case RegisterId::DR7:
		return make_register("dr7", ctx_64_.debug_regs[7]);


	case RegisterId::AX: return RegisterRef();
	case RegisterId::AH: return RegisterRef();
	case RegisterId::AL: return RegisterRef();
	case RegisterId::BX: return RegisterRef();
	case RegisterId::BH: return RegisterRef();
	case RegisterId::BL: return RegisterRef();
	case RegisterId::CX: return RegisterRef();
	case RegisterId::CH: return RegisterRef();
	case RegisterId::CL: return RegisterRef();
	case RegisterId::DX: return RegisterRef();
	case RegisterId::DH: return RegisterRef();
	case RegisterId::DL: return RegisterRef();
	case RegisterId::DI: return RegisterRef();
	case RegisterId::SI: return RegisterRef();;
	case RegisterId::BP: return RegisterRef();;
	case RegisterId::SP: return RegisterRef();
	case RegisterId::SIL: return RegisterRef();
	case RegisterId::DIL: return RegisterRef();
	case RegisterId::BPL: return RegisterRef();
	case RegisterId::SPL: return RegisterRef();
	case RegisterId::R8D: return RegisterRef();
	case RegisterId::R8W: return RegisterRef();
	case RegisterId::R8B: return RegisterRef();
	case RegisterId::R9D: return RegisterRef();
	case RegisterId::R9W: return RegisterRef();
	case RegisterId::R9B: return RegisterRef();
	case RegisterId::R10D: return RegisterRef();
	case RegisterId::R10W: return RegisterRef();
	case RegisterId::R10B: return RegisterRef();
	case RegisterId::R11D: return RegisterRef();
	case RegisterId::R11W: return RegisterRef();
	case RegisterId::R11B: return RegisterRef();
	case RegisterId::R12D: return RegisterRef();
	case RegisterId::R12W: return RegisterRef();
	case RegisterId::R12B: return RegisterRef();
	case RegisterId::R13D: return RegisterRef();
	case RegisterId::R13W: return RegisterRef();
	case RegisterId::R13B: return RegisterRef();
	case RegisterId::R14D: return RegisterRef();
	case RegisterId::R14W: return RegisterRef();
	case RegisterId::R14B: return RegisterRef();
	case RegisterId::R15D: return RegisterRef();
	case RegisterId::R15W: return RegisterRef();
	case RegisterId::R15B: return RegisterRef();

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

	switch (reg) {
	case RegisterId::EAX:
		return make_register("eax", ctx_32_.regs.eax);
	case RegisterId::EBX:
		return make_register("ebx", ctx_32_.regs.ebx);
	case RegisterId::ECX:
		return make_register("ecx", ctx_32_.regs.ecx);
	case RegisterId::EDX:
		return make_register("edx", ctx_32_.regs.edx);
	case RegisterId::ESI:
		return make_register("esi", ctx_32_.regs.esi);
	case RegisterId::EDI:
		return make_register("edi", ctx_32_.regs.edi);
	case RegisterId::ORIG_EAX:
		return make_register("orig_eax", ctx_32_.regs.orig_eax);
	case RegisterId::EIP:
		return make_register("eip", ctx_32_.regs.eip);
	case RegisterId::CS:
		return make_register("cs", ctx_32_.regs.cs);
	case RegisterId::EFLAGS:
		return make_register("eflags", ctx_32_.regs.eflags);
	case RegisterId::ESP:
		return make_register("esp", ctx_32_.regs.esp);
	case RegisterId::EBP:
		return make_register("ebp", ctx_32_.regs.ebp);
	case RegisterId::SS:
		return make_register("ss", ctx_32_.regs.ss);
	case RegisterId::DS:
		return make_register("ds", ctx_32_.regs.ds);
	case RegisterId::ES:
		return make_register("es", ctx_32_.regs.es);
	case RegisterId::FS:
		return make_register("fs", ctx_32_.regs.fs);
	case RegisterId::GS:
		return make_register("gs", ctx_32_.regs.gs);

	case RegisterId::FS_BASE:
		return make_register("fs_base", ctx_32_.fs_base);
	case RegisterId::GS_BASE:
		return make_register("gs_base", ctx_32_.gs_base);

	// Debug Registers
	case RegisterId::DR0:
		return make_register("dr0", ctx_32_.debug_regs[0]);
	case RegisterId::DR1:
		return make_register("dr1", ctx_32_.debug_regs[1]);
	case RegisterId::DR2:
		return make_register("dr2", ctx_32_.debug_regs[2]);
	case RegisterId::DR3:
		return make_register("dr3", ctx_32_.debug_regs[3]);
	case RegisterId::DR4:
		return make_register("dr4", ctx_32_.debug_regs[4]);
	case RegisterId::DR5:
		return make_register("dr5", ctx_32_.debug_regs[5]);
	case RegisterId::DR6:
		return make_register("dr6", ctx_32_.debug_regs[6]);
	case RegisterId::DR7:
		return make_register("dr7", ctx_32_.debug_regs[7]);
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
