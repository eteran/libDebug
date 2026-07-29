
#include "Debug/ContextArm.hpp"
#include "Debug/Debugger.hpp"
#include <cinttypes>
#include <cstdio>

namespace {

RegisterId simd_register_id(size_t index) {
	// clang-format off
	static constexpr RegisterId SimdRegisters[] = {
		RegisterId::V0, RegisterId::V1, RegisterId::V2, RegisterId::V3,
		RegisterId::V4, RegisterId::V5, RegisterId::V6, RegisterId::V7,
		RegisterId::V8, RegisterId::V9, RegisterId::V10, RegisterId::V11,
		RegisterId::V12, RegisterId::V13, RegisterId::V14, RegisterId::V15,
		RegisterId::V16, RegisterId::V17, RegisterId::V18, RegisterId::V19,
		RegisterId::V20, RegisterId::V21, RegisterId::V22, RegisterId::V23,
		RegisterId::V24, RegisterId::V25, RegisterId::V26, RegisterId::V27,
		RegisterId::V28, RegisterId::V29, RegisterId::V30, RegisterId::V31,
	};
	// clang-format on

	if (index >= (sizeof(SimdRegisters) / sizeof(SimdRegisters[0]))) {
		return RegisterId::Invalid;
	}

	return SimdRegisters[index];
}

}

/**
 * @brief Dumps the context to stdout.
 *
 */
void Context::dump() {
	if (is_64_bit()) {
		std::printf("PC : %016" PRIx64 " PSTATE: %016" PRIx64 "\n", get(RegisterId::XPC).as<uint64_t>(), get(RegisterId::XPSTATE).as<uint64_t>());
		std::printf("SP : %016" PRIx64 " X30   : %016" PRIx64 "\n", get(RegisterId::XSP).as<uint64_t>(), get(RegisterId::X30).as<uint64_t>());
		std::printf("X0 : %016" PRIx64 " X1    : %016" PRIx64 "\n", get(RegisterId::X0).as<uint64_t>(), get(RegisterId::X1).as<uint64_t>());
		std::printf("X2 : %016" PRIx64 " X3    : %016" PRIx64 "\n", get(RegisterId::X2).as<uint64_t>(), get(RegisterId::X3).as<uint64_t>());
		std::printf("X4 : %016" PRIx64 " X5    : %016" PRIx64 "\n", get(RegisterId::X4).as<uint64_t>(), get(RegisterId::X5).as<uint64_t>());
		std::printf("X6 : %016" PRIx64 " X7    : %016" PRIx64 "\n", get(RegisterId::X6).as<uint64_t>(), get(RegisterId::X7).as<uint64_t>());
		std::printf("X8 : %016" PRIx64 " X9    : %016" PRIx64 "\n", get(RegisterId::X8).as<uint64_t>(), get(RegisterId::X9).as<uint64_t>());
		std::printf("X10: %016" PRIx64 " X11   : %016" PRIx64 "\n", get(RegisterId::X10).as<uint64_t>(), get(RegisterId::X11).as<uint64_t>());
		std::printf("X12: %016" PRIx64 " X13   : %016" PRIx64 "\n", get(RegisterId::X12).as<uint64_t>(), get(RegisterId::X13).as<uint64_t>());
		std::printf("X14: %016" PRIx64 " X15   : %016" PRIx64 "\n", get(RegisterId::X14).as<uint64_t>(), get(RegisterId::X15).as<uint64_t>());
		std::printf("X16: %016" PRIx64 " X17   : %016" PRIx64 "\n", get(RegisterId::X16).as<uint64_t>(), get(RegisterId::X17).as<uint64_t>());
		std::printf("X18: %016" PRIx64 " X19   : %016" PRIx64 "\n", get(RegisterId::X18).as<uint64_t>(), get(RegisterId::X19).as<uint64_t>());
		std::printf("X20: %016" PRIx64 " X21   : %016" PRIx64 "\n", get(RegisterId::X20).as<uint64_t>(), get(RegisterId::X21).as<uint64_t>());
		std::printf("X22: %016" PRIx64 " X23   : %016" PRIx64 "\n", get(RegisterId::X22).as<uint64_t>(), get(RegisterId::X23).as<uint64_t>());
		std::printf("X24: %016" PRIx64 " X25   : %016" PRIx64 "\n", get(RegisterId::X24).as<uint64_t>(), get(RegisterId::X25).as<uint64_t>());
		std::printf("X26: %016" PRIx64 " X27   : %016" PRIx64 "\n", get(RegisterId::X26).as<uint64_t>(), get(RegisterId::X27).as<uint64_t>());
		std::printf("X28: %016" PRIx64 " X29   : %016" PRIx64 "\n", get(RegisterId::X28).as<uint64_t>(), get(RegisterId::X29).as<uint64_t>());
	} else {
		std::printf("PC   : %08" PRIx32 " CPSR : %08" PRIx32 "\n", get(RegisterId::PC).as<uint32_t>(), get(RegisterId::CPSR).as<uint32_t>());
		std::printf("SP   : %08" PRIx32 " LR   : %08" PRIx32 "\n", get(RegisterId::SP).as<uint32_t>(), get(RegisterId::LR).as<uint32_t>());
		std::printf("R0   : %08" PRIx32 " R1   : %08" PRIx32 "\n", get(RegisterId::R0).as<uint32_t>(), get(RegisterId::R1).as<uint32_t>());
		std::printf("R2   : %08" PRIx32 " R3   : %08" PRIx32 "\n", get(RegisterId::R2).as<uint32_t>(), get(RegisterId::R3).as<uint32_t>());
		std::printf("R4   : %08" PRIx32 " R5   : %08" PRIx32 "\n", get(RegisterId::R4).as<uint32_t>(), get(RegisterId::R5).as<uint32_t>());
		std::printf("R6   : %08" PRIx32 " R7   : %08" PRIx32 "\n", get(RegisterId::R6).as<uint32_t>(), get(RegisterId::R7).as<uint32_t>());
		std::printf("R8   : %08" PRIx32 " R9   : %08" PRIx32 "\n", get(RegisterId::R8).as<uint32_t>(), get(RegisterId::R9).as<uint32_t>());
		std::printf("R10  : %08" PRIx32 " R11  : %08" PRIx32 "\n", get(RegisterId::R10).as<uint32_t>(), get(RegisterId::R11).as<uint32_t>());
		std::printf("R12  : %08" PRIx32 " ORIG_R0: %08" PRIx32 "\n", get(RegisterId::R12).as<uint32_t>(), get(RegisterId::ORIG_R0).as<uint32_t>());
	}

	if (vfp_filled_) {
		std::printf("VFP FPSR: %08" PRIx32 " FPCR: %08" PRIx32 "\n", get(RegisterId::FPSR).as<uint32_t>(), get(RegisterId::FPCR).as<uint32_t>());
		std::printf("VFP/SIMD registers:\n");
		for (size_t n = 0; n < 32; ++n) {
			const auto reg_id = simd_register_id(n);
			const auto reg    = get(reg_id);
			std::printf("V%02zu: %s\n", n, reg.to_string().c_str());
		}
	}
}

/**
 * @brief Returns a reference to the given register in a 64-bit context.
 *
 * @param reg The register to return a reference to.
 * @return A reference to the given register.
 */
RegisterRef Context::get_64(RegisterId reg) const {

	// clang-format off
	switch (reg) {
	case RegisterId::X0:		return make_register("x0", ctx_64_.regs.regs[0], 0);
	case RegisterId::X1:		return make_register("x1", ctx_64_.regs.regs[1], 0);
	case RegisterId::X2:		return make_register("x2", ctx_64_.regs.regs[2], 0);
	case RegisterId::X3:		return make_register("x3", ctx_64_.regs.regs[3], 0);
	case RegisterId::X4:		return make_register("x4", ctx_64_.regs.regs[4], 0);
	case RegisterId::X5:		return make_register("x5", ctx_64_.regs.regs[5], 0);
	case RegisterId::X6:		return make_register("x6", ctx_64_.regs.regs[6], 0);
	case RegisterId::X7:		return make_register("x7", ctx_64_.regs.regs[7], 0);
	case RegisterId::X8:		return make_register("x8", ctx_64_.regs.regs[8], 0);
	case RegisterId::X9:		return make_register("x9", ctx_64_.regs.regs[9], 0);
	case RegisterId::X10:		return make_register("x10", ctx_64_.regs.regs[10], 0);
	case RegisterId::X11:		return make_register("x11", ctx_64_.regs.regs[11], 0);
	case RegisterId::X12:		return make_register("x12", ctx_64_.regs.regs[12], 0);
	case RegisterId::X13:		return make_register("x13", ctx_64_.regs.regs[13], 0);
	case RegisterId::X14:		return make_register("x14", ctx_64_.regs.regs[14], 0);
	case RegisterId::X15:		return make_register("x15", ctx_64_.regs.regs[15], 0);
	case RegisterId::X16:		return make_register("x16", ctx_64_.regs.regs[16], 0);
	case RegisterId::X17:		return make_register("x17", ctx_64_.regs.regs[17], 0);
	case RegisterId::X18:		return make_register("x18", ctx_64_.regs.regs[18], 0);
	case RegisterId::X19:		return make_register("x19", ctx_64_.regs.regs[19], 0);
	case RegisterId::X20:		return make_register("x20", ctx_64_.regs.regs[20], 0);
	case RegisterId::X21:		return make_register("x21", ctx_64_.regs.regs[21], 0);
	case RegisterId::X22:		return make_register("x22", ctx_64_.regs.regs[22], 0);
	case RegisterId::X23:		return make_register("x23", ctx_64_.regs.regs[23], 0);
	case RegisterId::X24:		return make_register("x24", ctx_64_.regs.regs[24], 0);
	case RegisterId::X25:		return make_register("x25", ctx_64_.regs.regs[25], 0);
	case RegisterId::X26:		return make_register("x26", ctx_64_.regs.regs[26], 0);
	case RegisterId::X27:		return make_register("x27", ctx_64_.regs.regs[27], 0);
	case RegisterId::X28:		return make_register("x28", ctx_64_.regs.regs[28], 0);
	case RegisterId::X29:		return make_register("x29", ctx_64_.regs.regs[29], 0);
	case RegisterId::X30:		return make_register("x30", ctx_64_.regs.regs[30], 0);
	case RegisterId::XSP:		return make_register("sp", ctx_64_.regs.sp, 0);
	case RegisterId::XPC:		return make_register("pc", ctx_64_.regs.pc, 0);
	case RegisterId::XPSTATE:	return make_register("pstate", ctx_64_.regs.pstate, 0);
	case RegisterId::FPSR:		return make_register("fpsr", vfp_regs_.fpsr, 0, sizeof(vfp_regs_.fpsr));
	case RegisterId::FPCR:		return make_register("fpcr", vfp_regs_.fpcr, 0, sizeof(vfp_regs_.fpcr));
	case RegisterId::V0:		return make_register("v0", vfp_regs_.vregs[0], 0, sizeof(vfp_regs_.vregs[0]));
	case RegisterId::V1:		return make_register("v1", vfp_regs_.vregs[1], 0, sizeof(vfp_regs_.vregs[1]));
	case RegisterId::V2:		return make_register("v2", vfp_regs_.vregs[2], 0, sizeof(vfp_regs_.vregs[2]));
	case RegisterId::V3:		return make_register("v3", vfp_regs_.vregs[3], 0, sizeof(vfp_regs_.vregs[3]));
	case RegisterId::V4:		return make_register("v4", vfp_regs_.vregs[4], 0, sizeof(vfp_regs_.vregs[4]));
	case RegisterId::V5:		return make_register("v5", vfp_regs_.vregs[5], 0, sizeof(vfp_regs_.vregs[5]));
	case RegisterId::V6:		return make_register("v6", vfp_regs_.vregs[6], 0, sizeof(vfp_regs_.vregs[6]));
	case RegisterId::V7:		return make_register("v7", vfp_regs_.vregs[7], 0, sizeof(vfp_regs_.vregs[7]));
	case RegisterId::V8:		return make_register("v8", vfp_regs_.vregs[8], 0, sizeof(vfp_regs_.vregs[8]));
	case RegisterId::V9:		return make_register("v9", vfp_regs_.vregs[9], 0, sizeof(vfp_regs_.vregs[9]));
	case RegisterId::V10:		return make_register("v10", vfp_regs_.vregs[10], 0, sizeof(vfp_regs_.vregs[10]));
	case RegisterId::V11:		return make_register("v11", vfp_regs_.vregs[11], 0, sizeof(vfp_regs_.vregs[11]));
	case RegisterId::V12:		return make_register("v12", vfp_regs_.vregs[12], 0, sizeof(vfp_regs_.vregs[12]));
	case RegisterId::V13:		return make_register("v13", vfp_regs_.vregs[13], 0, sizeof(vfp_regs_.vregs[13]));
	case RegisterId::V14:		return make_register("v14", vfp_regs_.vregs[14], 0, sizeof(vfp_regs_.vregs[14]));
	case RegisterId::V15:		return make_register("v15", vfp_regs_.vregs[15], 0, sizeof(vfp_regs_.vregs[15]));
	case RegisterId::V16:		return make_register("v16", vfp_regs_.vregs[16], 0, sizeof(vfp_regs_.vregs[16]));
	case RegisterId::V17:		return make_register("v17", vfp_regs_.vregs[17], 0, sizeof(vfp_regs_.vregs[17]));
	case RegisterId::V18:		return make_register("v18", vfp_regs_.vregs[18], 0, sizeof(vfp_regs_.vregs[18]));
	case RegisterId::V19:		return make_register("v19", vfp_regs_.vregs[19], 0, sizeof(vfp_regs_.vregs[19]));
	case RegisterId::V20:		return make_register("v20", vfp_regs_.vregs[20], 0, sizeof(vfp_regs_.vregs[20]));
	case RegisterId::V21:		return make_register("v21", vfp_regs_.vregs[21], 0, sizeof(vfp_regs_.vregs[21]));
	case RegisterId::V22:		return make_register("v22", vfp_regs_.vregs[22], 0, sizeof(vfp_regs_.vregs[22]));
	case RegisterId::V23:		return make_register("v23", vfp_regs_.vregs[23], 0, sizeof(vfp_regs_.vregs[23]));
	case RegisterId::V24:		return make_register("v24", vfp_regs_.vregs[24], 0, sizeof(vfp_regs_.vregs[24]));
	case RegisterId::V25:		return make_register("v25", vfp_regs_.vregs[25], 0, sizeof(vfp_regs_.vregs[25]));
	case RegisterId::V26:		return make_register("v26", vfp_regs_.vregs[26], 0, sizeof(vfp_regs_.vregs[26]));
	case RegisterId::V27:		return make_register("v27", vfp_regs_.vregs[27], 0, sizeof(vfp_regs_.vregs[27]));
	case RegisterId::V28:		return make_register("v28", vfp_regs_.vregs[28], 0, sizeof(vfp_regs_.vregs[28]));
	case RegisterId::V29:		return make_register("v29", vfp_regs_.vregs[29], 0, sizeof(vfp_regs_.vregs[29]));
	case RegisterId::V30:		return make_register("v30", vfp_regs_.vregs[30], 0, sizeof(vfp_regs_.vregs[30]));
	case RegisterId::V31:		return make_register("v31", vfp_regs_.vregs[31], 0, sizeof(vfp_regs_.vregs[31]));

	case RegisterId::STACK_POINTER:		  return make_register("sp", ctx_64_.regs.sp, 0);
	case RegisterId::INSTRUCTION_POINTER: return make_register("pc", ctx_64_.regs.pc, 0);
	case RegisterId::FLAGS_REGISTER:      return make_register("pstate", ctx_64_.regs.pstate, 0);


	default:
		Debugger::log("Unknown Register [64]: %d", static_cast<int>(reg));
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
RegisterRef Context::get_32(RegisterId reg) const {

	// clang-format off
	switch (reg) {
	case RegisterId::R0:		return make_register("r0", ctx_32_.regs.regs[0], 0);
	case RegisterId::R1:		return make_register("r1", ctx_32_.regs.regs[1], 0);
	case RegisterId::R2:		return make_register("r2", ctx_32_.regs.regs[2], 0);
	case RegisterId::R3:		return make_register("r3", ctx_32_.regs.regs[3], 0);
	case RegisterId::R4:		return make_register("r4", ctx_32_.regs.regs[4], 0);
	case RegisterId::R5:		return make_register("r5", ctx_32_.regs.regs[5], 0);
	case RegisterId::R6:		return make_register("r6", ctx_32_.regs.regs[6], 0);
	case RegisterId::R7:		return make_register("r7", ctx_32_.regs.regs[7], 0);
	case RegisterId::R8:		return make_register("r8", ctx_32_.regs.regs[8], 0);
	case RegisterId::R9:		return make_register("r9", ctx_32_.regs.regs[9], 0);
	case RegisterId::R10:		return make_register("r10", ctx_32_.regs.regs[10], 0);
	case RegisterId::R11:		return make_register("r11", ctx_32_.regs.regs[11], 0);
	case RegisterId::R12:		return make_register("r12", ctx_32_.regs.regs[12], 0);
	case RegisterId::SP:		return make_register("sp", ctx_32_.regs.regs[13], 0);
	case RegisterId::LR:		return make_register("lr", ctx_32_.regs.regs[14], 0);
	case RegisterId::PC:		return make_register("pc", ctx_32_.regs.regs[15], 0);
	case RegisterId::CPSR:		return make_register("cpsr", ctx_32_.regs.regs[16], 0);
	case RegisterId::ORIG_R0:	return make_register("orig_r0", ctx_32_.regs.regs[17], 0);
	case RegisterId::FPSR:		return make_register("fpsr", vfp_regs_.fpsr, 0, sizeof(vfp_regs_.fpsr));
	case RegisterId::FPCR:		return make_register("fpcr", vfp_regs_.fpcr, 0, sizeof(vfp_regs_.fpcr));
	case RegisterId::V0:		return make_register("v0", vfp_regs_.vregs[0], 0, sizeof(vfp_regs_.vregs[0]));
	case RegisterId::V1:		return make_register("v1", vfp_regs_.vregs[1], 0, sizeof(vfp_regs_.vregs[1]));
	case RegisterId::V2:		return make_register("v2", vfp_regs_.vregs[2], 0, sizeof(vfp_regs_.vregs[2]));
	case RegisterId::V3:		return make_register("v3", vfp_regs_.vregs[3], 0, sizeof(vfp_regs_.vregs[3]));
	case RegisterId::V4:		return make_register("v4", vfp_regs_.vregs[4], 0, sizeof(vfp_regs_.vregs[4]));
	case RegisterId::V5:		return make_register("v5", vfp_regs_.vregs[5], 0, sizeof(vfp_regs_.vregs[5]));
	case RegisterId::V6:		return make_register("v6", vfp_regs_.vregs[6], 0, sizeof(vfp_regs_.vregs[6]));
	case RegisterId::V7:		return make_register("v7", vfp_regs_.vregs[7], 0, sizeof(vfp_regs_.vregs[7]));
	case RegisterId::V8:		return make_register("v8", vfp_regs_.vregs[8], 0, sizeof(vfp_regs_.vregs[8]));
	case RegisterId::V9:		return make_register("v9", vfp_regs_.vregs[9], 0, sizeof(vfp_regs_.vregs[9]));
	case RegisterId::V10:		return make_register("v10", vfp_regs_.vregs[10], 0, sizeof(vfp_regs_.vregs[10]));
	case RegisterId::V11:		return make_register("v11", vfp_regs_.vregs[11], 0, sizeof(vfp_regs_.vregs[11]));
	case RegisterId::V12:		return make_register("v12", vfp_regs_.vregs[12], 0, sizeof(vfp_regs_.vregs[12]));
	case RegisterId::V13:		return make_register("v13", vfp_regs_.vregs[13], 0, sizeof(vfp_regs_.vregs[13]));
	case RegisterId::V14:		return make_register("v14", vfp_regs_.vregs[14], 0, sizeof(vfp_regs_.vregs[14]));
	case RegisterId::V15:		return make_register("v15", vfp_regs_.vregs[15], 0, sizeof(vfp_regs_.vregs[15]));
	case RegisterId::V16:		return make_register("v16", vfp_regs_.vregs[16], 0, sizeof(vfp_regs_.vregs[16]));
	case RegisterId::V17:		return make_register("v17", vfp_regs_.vregs[17], 0, sizeof(vfp_regs_.vregs[17]));
	case RegisterId::V18:		return make_register("v18", vfp_regs_.vregs[18], 0, sizeof(vfp_regs_.vregs[18]));
	case RegisterId::V19:		return make_register("v19", vfp_regs_.vregs[19], 0, sizeof(vfp_regs_.vregs[19]));
	case RegisterId::V20:		return make_register("v20", vfp_regs_.vregs[20], 0, sizeof(vfp_regs_.vregs[20]));
	case RegisterId::V21:		return make_register("v21", vfp_regs_.vregs[21], 0, sizeof(vfp_regs_.vregs[21]));
	case RegisterId::V22:		return make_register("v22", vfp_regs_.vregs[22], 0, sizeof(vfp_regs_.vregs[22]));
	case RegisterId::V23:		return make_register("v23", vfp_regs_.vregs[23], 0, sizeof(vfp_regs_.vregs[23]));
	case RegisterId::V24:		return make_register("v24", vfp_regs_.vregs[24], 0, sizeof(vfp_regs_.vregs[24]));
	case RegisterId::V25:		return make_register("v25", vfp_regs_.vregs[25], 0, sizeof(vfp_regs_.vregs[25]));
	case RegisterId::V26:		return make_register("v26", vfp_regs_.vregs[26], 0, sizeof(vfp_regs_.vregs[26]));
	case RegisterId::V27:		return make_register("v27", vfp_regs_.vregs[27], 0, sizeof(vfp_regs_.vregs[27]));
	case RegisterId::V28:		return make_register("v28", vfp_regs_.vregs[28], 0, sizeof(vfp_regs_.vregs[28]));
	case RegisterId::V29:		return make_register("v29", vfp_regs_.vregs[29], 0, sizeof(vfp_regs_.vregs[29]));
	case RegisterId::V30:		return make_register("v30", vfp_regs_.vregs[30], 0, sizeof(vfp_regs_.vregs[30]));
	case RegisterId::V31:		return make_register("v31", vfp_regs_.vregs[31], 0, sizeof(vfp_regs_.vregs[31]));

	case RegisterId::STACK_POINTER:		  return make_register("sp", ctx_32_.regs.regs[13], 0);
	case RegisterId::INSTRUCTION_POINTER: return make_register("pc", ctx_32_.regs.regs[15], 0);
	case RegisterId::FLAGS_REGISTER:      return make_register("cpsr", ctx_32_.regs.regs[16], 0);

	default:
		Debugger::log("Unknown Register [32]: %d", static_cast<int>(reg));
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
RegisterRef Context::get(RegisterId reg) const {

#if defined(__aarch64__) || defined(__arm64__)
	return get_64(reg);
#elif defined(__arm__)
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
RegisterRef Context::operator[](RegisterId reg) const {
	return get(reg);
}
