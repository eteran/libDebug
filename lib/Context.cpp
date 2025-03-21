
#include "Debug/Context.hpp"
#include <cstdio>

/**
 * @brief Returns a reference to the given register in a 64-bit context.
 *
 * @param reg The register to return a reference to.
 * @return A reference to the given register.
 */
RegisterRef Context::get_64(RegisterId reg) {

	// clang-format off
	switch (reg) {
	// Segment Registers
	case RegisterId::CS:		return make_register("cs", ctx_64_.regs.cs, 0);
	case RegisterId::DS:		return make_register("ds", ctx_64_.regs.ds, 0);
	case RegisterId::ES:		return make_register("es", ctx_64_.regs.es, 0);
	case RegisterId::FS:		return make_register("fs", ctx_64_.regs.fs, 0);
	case RegisterId::GS:		return make_register("gs", ctx_64_.regs.gs, 0);
	case RegisterId::SS:		return make_register("ss", ctx_64_.regs.ss, 0);
	case RegisterId::FS_BASE:	return make_register("fs_base", ctx_64_.regs.fs_base, 0);
	case RegisterId::GS_BASE:	return make_register("gs_base", ctx_64_.regs.gs_base, 0);

	// 64-bit GP registers
	case RegisterId::R15:		return make_register("r15", ctx_64_.regs.r15, 0);
	case RegisterId::R14:		return make_register("r14", ctx_64_.regs.r14, 0);
	case RegisterId::R13:		return make_register("r13", ctx_64_.regs.r13, 0);
	case RegisterId::R12:		return make_register("r12", ctx_64_.regs.r12, 0);
	case RegisterId::RBP:		return make_register("rbp", ctx_64_.regs.rbp, 0);
	case RegisterId::RBX:		return make_register("rbx", ctx_64_.regs.rbx, 0);
	case RegisterId::R11:		return make_register("r11", ctx_64_.regs.r11, 0);
	case RegisterId::R10:		return make_register("r10", ctx_64_.regs.r10, 0);
	case RegisterId::R9:		return make_register("r9",  ctx_64_.regs.r9,  0);
	case RegisterId::R8:		return make_register("r8",  ctx_64_.regs.r8,  0);
	case RegisterId::RAX:		return make_register("rax", ctx_64_.regs.rax, 0);
	case RegisterId::RCX:		return make_register("rcx", ctx_64_.regs.rcx, 0);
	case RegisterId::RDX:		return make_register("rdx", ctx_64_.regs.rdx, 0);
	case RegisterId::RSI:		return make_register("rsi", ctx_64_.regs.rsi, 0);
	case RegisterId::RDI:		return make_register("rdi", ctx_64_.regs.rdi, 0);
	case RegisterId::RIP:		return make_register("rip", ctx_64_.regs.rip, 0);
	case RegisterId::RSP:		return make_register("rsp", ctx_64_.regs.rsp, 0);
	case RegisterId::RFLAGS:	return make_register("rflags", ctx_64_.regs.rflags, 0);
	case RegisterId::ORIG_RAX:	return make_register("orig_rax", ctx_64_.regs.orig_rax, 0);

	// 32-bit GP registers
	case RegisterId::EAX:		return make_register("eax",  ctx_64_.regs.rax, sizeof(uint32_t), 0);
	case RegisterId::EBX:		return make_register("ebx",  ctx_64_.regs.rbx, sizeof(uint32_t), 0);
	case RegisterId::ECX:		return make_register("ecx",  ctx_64_.regs.rcx, sizeof(uint32_t), 0);
	case RegisterId::EDX:		return make_register("edx",  ctx_64_.regs.rdx, sizeof(uint32_t), 0);
	case RegisterId::ESI:		return make_register("esi",  ctx_64_.regs.rsi, sizeof(uint32_t), 0);
	case RegisterId::EDI:		return make_register("esi",  ctx_64_.regs.rdi, sizeof(uint32_t), 0);
	case RegisterId::EIP:		return make_register("eip",  ctx_64_.regs.rip, sizeof(uint32_t), 0);
	case RegisterId::ESP:		return make_register("esp",  ctx_64_.regs.rsp, sizeof(uint32_t), 0);
	case RegisterId::EBP:		return make_register("ebp",  ctx_64_.regs.rbp, sizeof(uint32_t), 0);
	case RegisterId::R8D:		return make_register("r8d",  ctx_64_.regs.r8,  sizeof(uint32_t), 0);
	case RegisterId::R9D:		return make_register("r9d",  ctx_64_.regs.r9,  sizeof(uint32_t), 0);
	case RegisterId::R10D:		return make_register("r10d", ctx_64_.regs.r10, sizeof(uint32_t), 0);
	case RegisterId::R11D:		return make_register("r11d", ctx_64_.regs.r11, sizeof(uint32_t), 0);
	case RegisterId::R12D:		return make_register("r12d", ctx_64_.regs.r12, sizeof(uint32_t), 0);
	case RegisterId::R13D:		return make_register("r13d", ctx_64_.regs.r13, sizeof(uint32_t), 0);
	case RegisterId::R14D:		return make_register("r14d", ctx_64_.regs.r14, sizeof(uint32_t), 0);
	case RegisterId::R15D:		return make_register("r15d", ctx_64_.regs.r15, sizeof(uint32_t), 0);
	case RegisterId::EFLAGS:	return make_register("eflags", ctx_64_.regs.rflags, sizeof(uint32_t), 0);
	case RegisterId::ORIG_EAX:	return make_register("orig_eax", ctx_64_.regs.orig_rax, sizeof(uint32_t), 0);

	// 16-bit GP registers
	case RegisterId::AX:		return make_register("ax",   ctx_64_.regs.rax, sizeof(uint16_t), 0);
	case RegisterId::BX:		return make_register("bx",   ctx_64_.regs.rbx, sizeof(uint16_t), 0);
	case RegisterId::CX:		return make_register("cx",   ctx_64_.regs.rcx, sizeof(uint16_t), 0);
	case RegisterId::DX:		return make_register("dx",   ctx_64_.regs.rdx, sizeof(uint16_t), 0);
	case RegisterId::SI:		return make_register("si",   ctx_64_.regs.rsi, sizeof(uint16_t), 0);
	case RegisterId::DI:		return make_register("di",   ctx_64_.regs.rdi, sizeof(uint16_t), 0);
	case RegisterId::BP:		return make_register("bp",   ctx_64_.regs.rbp, sizeof(uint16_t), 0);
	case RegisterId::SP:		return make_register("sp",   ctx_64_.regs.rsp, sizeof(uint16_t), 0);
	case RegisterId::R8W:		return make_register("r8w",  ctx_64_.regs.r8, sizeof(uint16_t),  0);
	case RegisterId::R9W:		return make_register("r9w",  ctx_64_.regs.r8, sizeof(uint16_t),  0);
	case RegisterId::R10W:		return make_register("r10w", ctx_64_.regs.r8, sizeof(uint16_t),  0);
	case RegisterId::R11W:		return make_register("r11w", ctx_64_.regs.r8, sizeof(uint16_t),  0);
	case RegisterId::R12W:		return make_register("r12w", ctx_64_.regs.r8, sizeof(uint16_t),  0);
	case RegisterId::R13W:		return make_register("r13w", ctx_64_.regs.r8, sizeof(uint16_t),  0);
	case RegisterId::R14W:		return make_register("r14w", ctx_64_.regs.r8, sizeof(uint16_t),  0);
	case RegisterId::R15W:		return make_register("r15w", ctx_64_.regs.r8, sizeof(uint16_t),  0);

	// 8-bit GP registers
	case RegisterId::AL:		return make_register("al", ctx_64_.regs.rax,   sizeof(uint8_t), 0);
	case RegisterId::BL:		return make_register("bl", ctx_64_.regs.rbx,   sizeof(uint8_t), 0);
	case RegisterId::CL:		return make_register("cl", ctx_64_.regs.rcx,   sizeof(uint8_t), 0);
	case RegisterId::DL:		return make_register("dl", ctx_64_.regs.rdx,   sizeof(uint8_t), 0);
	case RegisterId::AH:		return make_register("ah", ctx_64_.regs.rax,   sizeof(uint8_t), 1);
	case RegisterId::BH:		return make_register("bh", ctx_64_.regs.rbx,   sizeof(uint8_t), 1);
	case RegisterId::CH:		return make_register("ch", ctx_64_.regs.rcx,   sizeof(uint8_t), 1);
	case RegisterId::DH:		return make_register("dh", ctx_64_.regs.rdx,   sizeof(uint8_t), 1);
	case RegisterId::SIL:		return make_register("sil", ctx_64_.regs.rsi,  sizeof(uint8_t), 0);
	case RegisterId::DIL:		return make_register("dil", ctx_64_.regs.rdi,  sizeof(uint8_t), 0);
	case RegisterId::BPL:		return make_register("bpl", ctx_64_.regs.rbp,  sizeof(uint8_t), 0);
	case RegisterId::SPL:		return make_register("spl", ctx_64_.regs.rsp,  sizeof(uint8_t), 0);
	case RegisterId::R8B:		return make_register("r8b", ctx_64_.regs.r8,   sizeof(uint8_t), 0);
	case RegisterId::R9B:		return make_register("r9b", ctx_64_.regs.r9,   sizeof(uint8_t), 0);
	case RegisterId::R10B:		return make_register("r10b", ctx_64_.regs.r10, sizeof(uint8_t), 0);
	case RegisterId::R11B:		return make_register("r11b", ctx_64_.regs.r11, sizeof(uint8_t), 0);
	case RegisterId::R12B:		return make_register("r12b", ctx_64_.regs.r12, sizeof(uint8_t), 0);
	case RegisterId::R13B:		return make_register("r13b", ctx_64_.regs.r13, sizeof(uint8_t), 0);
	case RegisterId::R14B:		return make_register("r14b", ctx_64_.regs.r14, sizeof(uint8_t), 0);
	case RegisterId::R15B:		return make_register("r15b", ctx_64_.regs.r15, sizeof(uint8_t), 0);

	// Debug Registers
	case RegisterId::DR0:		return make_register("dr0", ctx_64_.debug_regs[0], 0);
	case RegisterId::DR1:		return make_register("dr1", ctx_64_.debug_regs[1], 0);
	case RegisterId::DR2:		return make_register("dr2", ctx_64_.debug_regs[2], 0);
	case RegisterId::DR3:		return make_register("dr3", ctx_64_.debug_regs[3], 0);
	case RegisterId::DR4:		return make_register("dr4", ctx_64_.debug_regs[4], 0);
	case RegisterId::DR5:		return make_register("dr5", ctx_64_.debug_regs[5], 0);
	case RegisterId::DR6:		return make_register("dr6", ctx_64_.debug_regs[6], 0);
	case RegisterId::DR7:		return make_register("dr7", ctx_64_.debug_regs[7], 0);

	default:
		std::printf("Unknown Register [64]: %d\n", static_cast<int>(reg));
		return RegisterRef();
	}
	// clang-format on
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
		return make_register("eax", ctx_32_.regs.eax, 0);
	case RegisterId::EBX:
		return make_register("ebx", ctx_32_.regs.ebx, 0);
	case RegisterId::ECX:
		return make_register("ecx", ctx_32_.regs.ecx, 0);
	case RegisterId::EDX:
		return make_register("edx", ctx_32_.regs.edx, 0);
	case RegisterId::ESI:
		return make_register("esi", ctx_32_.regs.esi, 0);
	case RegisterId::EDI:
		return make_register("edi", ctx_32_.regs.edi, 0);
	case RegisterId::ORIG_EAX:
		return make_register("orig_eax", ctx_32_.regs.orig_eax, 0);
	case RegisterId::EIP:
		return make_register("eip", ctx_32_.regs.eip, 0);
	case RegisterId::CS:
		return make_register("cs", ctx_32_.regs.cs, 0);
	case RegisterId::EFLAGS:
		return make_register("eflags", ctx_32_.regs.eflags, 0);
	case RegisterId::ESP:
		return make_register("esp", ctx_32_.regs.esp, 0);
	case RegisterId::EBP:
		return make_register("ebp", ctx_32_.regs.ebp, 0);
	case RegisterId::SS:
		return make_register("ss", ctx_32_.regs.ss, 0);
	case RegisterId::DS:
		return make_register("ds", ctx_32_.regs.ds, 0);
	case RegisterId::ES:
		return make_register("es", ctx_32_.regs.es, 0);
	case RegisterId::FS:
		return make_register("fs", ctx_32_.regs.fs, 0);
	case RegisterId::GS:
		return make_register("gs", ctx_32_.regs.gs, 0);

	case RegisterId::FS_BASE:
		return make_register("fs_base", ctx_32_.fs_base, 0);
	case RegisterId::GS_BASE:
		return make_register("gs_base", ctx_32_.gs_base, 0);

	// Debug Registers
	case RegisterId::DR0:
		return make_register("dr0", ctx_32_.debug_regs[0], 0);
	case RegisterId::DR1:
		return make_register("dr1", ctx_32_.debug_regs[1], 0);
	case RegisterId::DR2:
		return make_register("dr2", ctx_32_.debug_regs[2], 0);
	case RegisterId::DR3:
		return make_register("dr3", ctx_32_.debug_regs[3], 0);
	case RegisterId::DR4:
		return make_register("dr4", ctx_32_.debug_regs[4], 0);
	case RegisterId::DR5:
		return make_register("dr5", ctx_32_.debug_regs[5], 0);
	case RegisterId::DR6:
		return make_register("dr6", ctx_32_.debug_regs[6], 0);
	case RegisterId::DR7:
		return make_register("dr7", ctx_32_.debug_regs[7], 0);
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
