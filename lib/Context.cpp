
#include "Debug/Context.hpp"
#include <cinttypes>
#include <cstdio>

/**
 * @brief Dumps the context to stdout.
 *
 */
void Context::dump() {
	/*
	RIP: 0033:0x7ffff7fe31e7
	Code: c0 75 e0 e9 6e fe ff ff 66 2e 0f 1f 84 00 00 00 00 00 90 55 48 83 c7 08 49 89 f0 48 89 e5 41 55 41 54 53 48 81 ec 30 01 00 00 <48> 8b 47 f8 48 89 3d 86 98 01 00 89 05 88 98 01 00 48 98 48 8d 74
	RSP: 002b:00007fffffffd728 EFLAGS: 00010206
	RAX: 00007ffff7ffdab0 RBX: 00007ffff7fe5af0 RCX: 0000000000000024
	RDX: 00007ffff7fc54f0 RSI: 00007fffffffd880 RDI: 00000000ffffd988
	RBP: 00007fffffffd870 R08: 00007fffffffd880 R09: 00007ffff7ffcec8
	R10: 0000000000036360 R11: 0000000000000000 R12: 00007fffffffd940
	R13: 0000000000000000 R14: 00007ffff7fc5000 R15: 00007ffff7fc5590
	FS:  0000000000000000 GS:  0000000000000000
	*/

	if (is_64_bit()) {
		std::printf("RIP: %016" PRIx64 " RFL: %016" PRIx64 "\n", get(RegisterId::RIP).as<uint64_t>(), get(RegisterId::RFLAGS).as<uint64_t>());
		std::printf("RSP: %016" PRIx64 " R8 : %016" PRIx64 "\n", get(RegisterId::RSP).as<uint64_t>(), get(RegisterId::R8).as<uint64_t>());
		std::printf("RBP: %016" PRIx64 " R9 : %016" PRIx64 "\n", get(RegisterId::RBP).as<uint64_t>(), get(RegisterId::R9).as<uint64_t>());
		std::printf("RAX: %016" PRIx64 " R10: %016" PRIx64 "\n", get(RegisterId::RAX).as<uint64_t>(), get(RegisterId::R10).as<uint64_t>());
		std::printf("RBX: %016" PRIx64 " R11: %016" PRIx64 "\n", get(RegisterId::RBX).as<uint64_t>(), get(RegisterId::R11).as<uint64_t>());
		std::printf("RCX: %016" PRIx64 " R12: %016" PRIx64 "\n", get(RegisterId::RCX).as<uint64_t>(), get(RegisterId::R12).as<uint64_t>());
		std::printf("RDX: %016" PRIx64 " R13: %016" PRIx64 "\n", get(RegisterId::RDX).as<uint64_t>(), get(RegisterId::R13).as<uint64_t>());
		std::printf("RSI: %016" PRIx64 " R14: %016" PRIx64 "\n", get(RegisterId::RSI).as<uint64_t>(), get(RegisterId::R14).as<uint64_t>());
		std::printf("RDI: %016" PRIx64 " R15: %016" PRIx64 "\n", get(RegisterId::RDI).as<uint64_t>(), get(RegisterId::R15).as<uint64_t>());
		std::printf("CS: %04x SS : %04x FS_BASE:  %016" PRIx64 "\n", get(RegisterId::CS).as<uint16_t>(), get(RegisterId::SS).as<uint16_t>(), get(RegisterId::FS_BASE).as<uint64_t>());
		std::printf("DS: %04x ES : %04x GS_BASE:  %016" PRIx64 "\n", get(RegisterId::DS).as<uint16_t>(), get(RegisterId::ES).as<uint16_t>(), get(RegisterId::GS_BASE).as<uint64_t>());
		std::printf("FS: %04x GS : %04x\n", get(RegisterId::FS).as<uint16_t>(), get(RegisterId::GS).as<uint16_t>());
	} else {
		std::printf("EIP: %08x EFL: %08x\n", get(RegisterId::EIP).as<uint32_t>(), get(RegisterId::EFLAGS).as<uint32_t>());
		std::printf("ESP: %08x EBP: %08x\n", get(RegisterId::ESP).as<uint32_t>(), get(RegisterId::EBP).as<uint32_t>());
		std::printf("EAX: %08x EBX: %08x\n", get(RegisterId::EAX).as<uint32_t>(), get(RegisterId::EBX).as<uint32_t>());
		std::printf("ECX: %08x EDX: %08x\n", get(RegisterId::ECX).as<uint32_t>(), get(RegisterId::EDX).as<uint32_t>());
		std::printf("ESI: %08x EDI: %08x\n", get(RegisterId::ESI).as<uint32_t>(), get(RegisterId::EDI).as<uint32_t>());

		std::printf("CS: %04x SS : %04x FS_BASE:  %08x\n", get(RegisterId::CS).as<uint16_t>(), get(RegisterId::SS).as<uint16_t>(), get(RegisterId::FS_BASE).as<uint32_t>());
		std::printf("DS: %04x ES : %04x GS_BASE:  %08x\n", get(RegisterId::DS).as<uint16_t>(), get(RegisterId::ES).as<uint16_t>(), get(RegisterId::GS_BASE).as<uint32_t>());
		std::printf("FS: %04x GS : %04x\n", get(RegisterId::FS).as<uint16_t>(), get(RegisterId::GS).as<uint16_t>());
	}

	if (xstate_.simd.sse_filled) {
		std::printf("XSTATE SSE registers:\n");
		for (size_t n = 0; n < (is_64_bit() ? 16 : 8); ++n) {
			std::printf("XMM%02zu: ", n);
			for (size_t b = 0; b < 16; ++b) {
				std::printf("%02x", xstate_.simd.registers[n].data[b]);
			}
			std::printf("\n");
		}
	}

	if (xstate_.simd.avx_filled) {
		std::printf("XSTATE AVX registers:\n");
		for (size_t n = 0; n < (is_64_bit() ? 16 : 8); ++n) {
			std::printf("YMM%02zu: ", n);
			for (size_t b = 0; b < 32; ++b) {
				std::printf("%02x", xstate_.simd.registers[n].data[b]);
			}
			std::printf("\n");
		}
	}

	if (is_64_bit() && xstate_.simd.zmm_filled) {
		std::printf("XSTATE ZMM registers:\n");
		for (size_t n = 0; n < 32; ++n) {
			std::printf("ZMM%02zu: ", n);
			for (size_t b = 0; b < 64; ++b) {
				std::printf("%02x", xstate_.simd.registers[n].data[b]);
			}
			std::printf("\n");
		}
	}
}

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

	// Size generic registers
	case RegisterId::XAX:		return make_register("rax", ctx_64_.regs.rax, 0);
	case RegisterId::XCX:		return make_register("rcx", ctx_64_.regs.rcx, 0);
	case RegisterId::XDX:		return make_register("rdx", ctx_64_.regs.rdx, 0);
	case RegisterId::XSI:		return make_register("rsi", ctx_64_.regs.rsi, 0);
	case RegisterId::XDI:		return make_register("rdi", ctx_64_.regs.rdi, 0);
	case RegisterId::XIP:		return make_register("rip", ctx_64_.regs.rip, 0);
	case RegisterId::XSP:		return make_register("rsp", ctx_64_.regs.rsp, 0);
	case RegisterId::XFLAGS:	return make_register("rflags", ctx_64_.regs.rflags, 0);

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

	// FPU Registers
	case RegisterId::ST0:		return make_register("st0", xstate_.x87.registers[0].data, 16, 0);
	case RegisterId::ST1:		return make_register("st1", xstate_.x87.registers[1].data, 16, 0);
	case RegisterId::ST2:		return make_register("st2", xstate_.x87.registers[2].data, 16, 0);
	case RegisterId::ST3:		return make_register("st3", xstate_.x87.registers[3].data, 16, 0);
	case RegisterId::ST4:		return make_register("st4", xstate_.x87.registers[4].data, 16, 0);
	case RegisterId::ST5:		return make_register("st5", xstate_.x87.registers[5].data, 16, 0);
	case RegisterId::ST6:		return make_register("st6", xstate_.x87.registers[6].data, 16, 0);
	case RegisterId::ST7:		return make_register("st7", xstate_.x87.registers[7].data, 16, 0);

	case RegisterId::CWD:		return make_register("cwd",  xstate_.x87.control_word, sizeof(xstate_.x87.control_word), 0);
	case RegisterId::SWD:		return make_register("swd",  xstate_.x87.status_word, sizeof(xstate_.x87.status_word), 0);
	case RegisterId::FTW:		return make_register("ftw",  xstate_.x87.tag_word, sizeof(xstate_.x87.tag_word), 0);
	case RegisterId::FOP:		return make_register("fop",  xstate_.x87.opcode, sizeof(xstate_.x87.opcode), 0);
	case RegisterId::FIP:		return make_register("fip",  xstate_.x87.inst_ptr_offset, sizeof(xstate_.x87.inst_ptr_offset), 0);
	case RegisterId::FDP:		return make_register("fdp",  xstate_.x87.data_ptr_offset, sizeof(xstate_.x87.data_ptr_offset), 0);
	case RegisterId::MXCSR:		return make_register("mxcsr", xstate_.simd.mxcsr, sizeof(xstate_.simd.mxcsr), 0);
	case RegisterId::MXCSR_MASK:return make_register("mxcsr_mask", xstate_.simd.mxcsr_mask, sizeof(xstate_.simd.mxcsr_mask), 0);

	// MMX Registers (alias of ST0-ST7)
	case RegisterId::MM0:		return make_register("mm0", xstate_.x87.registers[0].data, 8, 0);
	case RegisterId::MM1:		return make_register("mm1", xstate_.x87.registers[1].data, 8, 0);
	case RegisterId::MM2:		return make_register("mm2", xstate_.x87.registers[2].data, 8, 0);
	case RegisterId::MM3:		return make_register("mm3", xstate_.x87.registers[3].data, 8, 0);
	case RegisterId::MM4:		return make_register("mm4", xstate_.x87.registers[4].data, 8, 0);
	case RegisterId::MM5:		return make_register("mm5", xstate_.x87.registers[5].data, 8, 0);
	case RegisterId::MM6:		return make_register("mm6", xstate_.x87.registers[6].data, 8, 0);
	case RegisterId::MM7:		return make_register("mm7", xstate_.x87.registers[7].data, 8, 0);

	// SIMD Registers
	case RegisterId::XMM0:		return make_register("xmm0",  xstate_.simd.registers[0].data,  16, 0);
	case RegisterId::XMM1:		return make_register("xmm1",  xstate_.simd.registers[1].data,  16, 0);
	case RegisterId::XMM2:		return make_register("xmm2",  xstate_.simd.registers[2].data,  16, 0);
	case RegisterId::XMM3:		return make_register("xmm3",  xstate_.simd.registers[3].data,  16, 0);
	case RegisterId::XMM4:		return make_register("xmm4",  xstate_.simd.registers[4].data,  16, 0);
	case RegisterId::XMM5:		return make_register("xmm5",  xstate_.simd.registers[5].data,  16, 0);
	case RegisterId::XMM6:		return make_register("xmm6",  xstate_.simd.registers[6].data,  16, 0);
	case RegisterId::XMM7:		return make_register("xmm7",  xstate_.simd.registers[7].data,  16, 0);
	case RegisterId::XMM8:		return make_register("xmm8",  xstate_.simd.registers[8].data,  16, 0);
	case RegisterId::XMM9:		return make_register("xmm9",  xstate_.simd.registers[9].data,  16, 0);
	case RegisterId::XMM10:		return make_register("xmm10", xstate_.simd.registers[10].data, 16, 0);
	case RegisterId::XMM11:		return make_register("xmm11", xstate_.simd.registers[11].data, 16, 0);
	case RegisterId::XMM12:		return make_register("xmm12", xstate_.simd.registers[12].data, 16, 0);
	case RegisterId::XMM13:		return make_register("xmm13", xstate_.simd.registers[13].data, 16, 0);
	case RegisterId::XMM14:		return make_register("xmm14", xstate_.simd.registers[14].data, 16, 0);
	case RegisterId::XMM15:		return make_register("xmm15", xstate_.simd.registers[15].data, 16, 0);

	case RegisterId::YMM0:		return make_register("ymm0",  xstate_.simd.registers[0].data,  32, 0);
	case RegisterId::YMM1:		return make_register("ymm1",  xstate_.simd.registers[1].data,  32, 0);
	case RegisterId::YMM2:		return make_register("ymm2",  xstate_.simd.registers[2].data,  32, 0);
	case RegisterId::YMM3:		return make_register("ymm3",  xstate_.simd.registers[3].data,  32, 0);
	case RegisterId::YMM4:		return make_register("ymm4",  xstate_.simd.registers[4].data,  32, 0);
	case RegisterId::YMM5:		return make_register("ymm5",  xstate_.simd.registers[5].data,  32, 0);
	case RegisterId::YMM6:		return make_register("ymm6",  xstate_.simd.registers[6].data,  32, 0);
	case RegisterId::YMM7:		return make_register("ymm7",  xstate_.simd.registers[7].data,  32, 0);
	case RegisterId::YMM8:		return make_register("ymm8",  xstate_.simd.registers[8].data,  32, 0);
	case RegisterId::YMM9:		return make_register("ymm9",  xstate_.simd.registers[9].data,  32, 0);
	case RegisterId::YMM10:		return make_register("ymm10", xstate_.simd.registers[10].data, 32, 0);
	case RegisterId::YMM11:		return make_register("ymm11", xstate_.simd.registers[11].data, 32, 0);
	case RegisterId::YMM12:		return make_register("ymm12", xstate_.simd.registers[12].data, 32, 0);
	case RegisterId::YMM13:		return make_register("ymm13", xstate_.simd.registers[13].data, 32, 0);
	case RegisterId::YMM14:		return make_register("ymm14", xstate_.simd.registers[14].data, 32, 0);
	case RegisterId::YMM15:		return make_register("ymm15", xstate_.simd.registers[15].data, 32, 0);

	case RegisterId::ZMM0:		return make_register("zmm0",  xstate_.simd.registers[0].data,  64, 0);
	case RegisterId::ZMM1:		return make_register("zmm1",  xstate_.simd.registers[1].data,  64, 0);
	case RegisterId::ZMM2:		return make_register("zmm2",  xstate_.simd.registers[2].data,  64, 0);
	case RegisterId::ZMM3:		return make_register("zmm3",  xstate_.simd.registers[3].data,  64, 0);
	case RegisterId::ZMM4:		return make_register("zmm4",  xstate_.simd.registers[4].data,  64, 0);
	case RegisterId::ZMM5:		return make_register("zmm5",  xstate_.simd.registers[5].data,  64, 0);
	case RegisterId::ZMM6:		return make_register("zmm6",  xstate_.simd.registers[6].data,  64, 0);
	case RegisterId::ZMM7:		return make_register("zmm7",  xstate_.simd.registers[7].data,  64, 0);
	case RegisterId::ZMM8:		return make_register("zmm8",  xstate_.simd.registers[8].data,  64, 0);
	case RegisterId::ZMM9:		return make_register("zmm9",  xstate_.simd.registers[9].data,  64, 0);
	case RegisterId::ZMM10:		return make_register("zmm10", xstate_.simd.registers[10].data, 64, 0);
	case RegisterId::ZMM11:		return make_register("zmm11", xstate_.simd.registers[11].data, 64, 0);
	case RegisterId::ZMM12:		return make_register("zmm12", xstate_.simd.registers[12].data, 64, 0);
	case RegisterId::ZMM13:		return make_register("zmm13", xstate_.simd.registers[13].data, 64, 0);
	case RegisterId::ZMM14:		return make_register("zmm14", xstate_.simd.registers[14].data, 64, 0);
	case RegisterId::ZMM15:		return make_register("zmm15", xstate_.simd.registers[15].data, 64, 0);

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

	// clang-format off
	switch (reg) {
	case RegisterId::EAX:		return make_register("eax", ctx_32_.regs.eax, 0);
	case RegisterId::EBX:		return make_register("ebx", ctx_32_.regs.ebx, 0);
	case RegisterId::ECX:		return make_register("ecx", ctx_32_.regs.ecx, 0);
	case RegisterId::EDX:		return make_register("edx", ctx_32_.regs.edx, 0);
	case RegisterId::ESI:		return make_register("esi", ctx_32_.regs.esi, 0);
	case RegisterId::EDI:		return make_register("edi", ctx_32_.regs.edi, 0);
	case RegisterId::ORIG_EAX:	return make_register("orig_eax", ctx_32_.regs.orig_eax, 0);
	case RegisterId::EIP:		return make_register("eip", ctx_32_.regs.eip, 0);
	case RegisterId::CS:		return make_register("cs", ctx_32_.regs.cs, 0);
	case RegisterId::EFLAGS:	return make_register("eflags", ctx_32_.regs.eflags, 0);
	case RegisterId::ESP:		return make_register("esp", ctx_32_.regs.esp, 0);
	case RegisterId::EBP:		return make_register("ebp", ctx_32_.regs.ebp, 0);
	case RegisterId::SS:		return make_register("ss", ctx_32_.regs.ss, 0);
	case RegisterId::DS:		return make_register("ds", ctx_32_.regs.ds, 0);
	case RegisterId::ES:		return make_register("es", ctx_32_.regs.es, 0);
	case RegisterId::FS:		return make_register("fs", ctx_32_.regs.fs, 0);
	case RegisterId::GS:		return make_register("gs", ctx_32_.regs.gs, 0);

	case RegisterId::FS_BASE:	return make_register("fs_base", ctx_32_.fs_base, 0);
	case RegisterId::GS_BASE:	return make_register("gs_base", ctx_32_.gs_base, 0);

	// Debug Registers
	case RegisterId::DR0:	return make_register("dr0", ctx_32_.debug_regs[0], 0);
	case RegisterId::DR1:	return make_register("dr1", ctx_32_.debug_regs[1], 0);
	case RegisterId::DR2:	return make_register("dr2", ctx_32_.debug_regs[2], 0);
	case RegisterId::DR3:	return make_register("dr3", ctx_32_.debug_regs[3], 0);
	case RegisterId::DR4:	return make_register("dr4", ctx_32_.debug_regs[4], 0);
	case RegisterId::DR5:	return make_register("dr5", ctx_32_.debug_regs[5], 0);
	case RegisterId::DR6:	return make_register("dr6", ctx_32_.debug_regs[6], 0);
	case RegisterId::DR7:	return make_register("dr7", ctx_32_.debug_regs[7], 0);

	// FPU Registers
	case RegisterId::ST0:		return make_register("st0", xstate_.x87.registers[0].data, 16, 0);
	case RegisterId::ST1:		return make_register("st1", xstate_.x87.registers[1].data, 16, 0);
	case RegisterId::ST2:		return make_register("st2", xstate_.x87.registers[2].data, 16, 0);
	case RegisterId::ST3:		return make_register("st3", xstate_.x87.registers[3].data, 16, 0);
	case RegisterId::ST4:		return make_register("st4", xstate_.x87.registers[4].data, 16, 0);
	case RegisterId::ST5:		return make_register("st5", xstate_.x87.registers[5].data, 16, 0);
	case RegisterId::ST6:		return make_register("st6", xstate_.x87.registers[6].data, 16, 0);
	case RegisterId::ST7:		return make_register("st7", xstate_.x87.registers[7].data, 16, 0);

	case RegisterId::CWD:		return make_register("cwd",  xstate_.x87.control_word, sizeof(xstate_.x87.control_word), 0);
	case RegisterId::SWD:		return make_register("swd",  xstate_.x87.status_word, sizeof(xstate_.x87.status_word), 0);
	case RegisterId::FTW:		return make_register("ftw",  xstate_.x87.tag_word, sizeof(xstate_.x87.tag_word), 0);
	case RegisterId::FOP:		return make_register("fop",  xstate_.x87.opcode, sizeof(xstate_.x87.opcode), 0);
	case RegisterId::FIP:		return make_register("fip",  xstate_.x87.inst_ptr_offset, sizeof(xstate_.x87.inst_ptr_offset), 0);
	case RegisterId::FDP:		return make_register("fdp",  xstate_.x87.data_ptr_offset, sizeof(xstate_.x87.data_ptr_offset), 0);
	case RegisterId::MXCSR:		return make_register("mxcsr", xstate_.simd.mxcsr, sizeof(xstate_.simd.mxcsr), 0);
	case RegisterId::MXCSR_MASK:return make_register("mxcsr_mask", xstate_.simd.mxcsr_mask, sizeof(xstate_.simd.mxcsr_mask), 0);

	// MMX Registers (alias of ST0-ST7)
	case RegisterId::MM0:		return make_register("mm0", xstate_.x87.registers[0].data, 8, 0);
	case RegisterId::MM1:		return make_register("mm1", xstate_.x87.registers[1].data, 8, 0);
	case RegisterId::MM2:		return make_register("mm2", xstate_.x87.registers[2].data, 8, 0);
	case RegisterId::MM3:		return make_register("mm3", xstate_.x87.registers[3].data, 8, 0);
	case RegisterId::MM4:		return make_register("mm4", xstate_.x87.registers[4].data, 8, 0);
	case RegisterId::MM5:		return make_register("mm5", xstate_.x87.registers[5].data, 8, 0);
	case RegisterId::MM6:		return make_register("mm6", xstate_.x87.registers[6].data, 8, 0);
	case RegisterId::MM7:		return make_register("mm7", xstate_.x87.registers[7].data, 8, 0);

	// SIMD Registers
	case RegisterId::XMM0:		return make_register("xmm0",  xstate_.simd.registers[0].data,  16, 0);
	case RegisterId::XMM1:		return make_register("xmm1",  xstate_.simd.registers[1].data,  16, 0);
	case RegisterId::XMM2:		return make_register("xmm2",  xstate_.simd.registers[2].data,  16, 0);
	case RegisterId::XMM3:		return make_register("xmm3",  xstate_.simd.registers[3].data,  16, 0);
	case RegisterId::XMM4:		return make_register("xmm4",  xstate_.simd.registers[4].data,  16, 0);
	case RegisterId::XMM5:		return make_register("xmm5",  xstate_.simd.registers[5].data,  16, 0);
	case RegisterId::XMM6:		return make_register("xmm6",  xstate_.simd.registers[6].data,  16, 0);
	case RegisterId::XMM7:		return make_register("xmm7",  xstate_.simd.registers[7].data,  16, 0);

	case RegisterId::YMM0:		return make_register("ymm0",  xstate_.simd.registers[0].data,  32, 0);
	case RegisterId::YMM1:		return make_register("ymm1",  xstate_.simd.registers[1].data,  32, 0);
	case RegisterId::YMM2:		return make_register("ymm2",  xstate_.simd.registers[2].data,  32, 0);
	case RegisterId::YMM3:		return make_register("ymm3",  xstate_.simd.registers[3].data,  32, 0);
	case RegisterId::YMM4:		return make_register("ymm4",  xstate_.simd.registers[4].data,  32, 0);
	case RegisterId::YMM5:		return make_register("ymm5",  xstate_.simd.registers[5].data,  32, 0);
	case RegisterId::YMM6:		return make_register("ymm6",  xstate_.simd.registers[6].data,  32, 0);
	case RegisterId::YMM7:		return make_register("ymm7",  xstate_.simd.registers[7].data,  32, 0);

	// Size generic registers
	case RegisterId::XAX:		return make_register("eax", ctx_32_.regs.eax, 0);
	case RegisterId::XCX:		return make_register("ecx", ctx_32_.regs.ecx, 0);
	case RegisterId::XDX:		return make_register("edx", ctx_32_.regs.edx, 0);
	case RegisterId::XSI:		return make_register("esi", ctx_32_.regs.esi, 0);
	case RegisterId::XDI:		return make_register("edi", ctx_32_.regs.edi, 0);
	case RegisterId::XIP:		return make_register("eip", ctx_32_.regs.eip, 0);
	case RegisterId::XSP:		return make_register("esp", ctx_32_.regs.esp, 0);
	case RegisterId::XFLAGS:	return make_register("eflags", ctx_32_.regs.eflags, 0);
	default:
		std::printf("Unknown Register [32]: %d\n", static_cast<int>(reg));
		return RegisterRef();
	}
	// clang-format on
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

/**
 * @brief Returns a reference to the given register.
 *
 * @param reg The register to return a reference to.
 * @return A reference to the given register.
 */
RegisterRef Context::operator[](RegisterId reg) {
	return get(reg);
}
