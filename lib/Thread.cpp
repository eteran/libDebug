
#include "Debug/Thread.hpp"
#include "Debug/Context.hpp"
#include "Debug/DebuggerError.hpp"

#include <cassert>
#include <cinttypes>
#include <csignal>
#include <cstdio>
#include <cstdlib>

#include <asm/ldt.h>
#include <elf.h>
#include <sys/procfs.h>
#include <sys/ptrace.h>
#include <sys/syscall.h>
#include <sys/uio.h>
#include <sys/user.h>
#include <sys/wait.h>
#include <unistd.h>

namespace {
// The extended state feature bits
enum FeatureBit : uint64_t {
	FEATURE_X87 = 1 << 0,
	FEATURE_SSE = 1 << 1,
	FEATURE_AVX = 1 << 2,
	// MPX adds two feature bits
	FEATURE_BNDREGS = 1 << 3,
	FEATURE_BNDCFG  = 1 << 4,
	FEATURE_MPX     = FEATURE_BNDREGS | FEATURE_BNDCFG,
	// AVX-512 adds three feature bits
	FEATURE_K      = 1 << 5,
	FEATURE_ZMM_H  = 1 << 6,
	FEATURE_ZMM    = 1 << 7,
	FEATURE_AVX512 = FEATURE_K | FEATURE_ZMM_H | FEATURE_ZMM,
};

constexpr long TraceOptions = PTRACE_O_TRACECLONE |
							  PTRACE_O_TRACEFORK |
							  PTRACE_O_TRACEEXIT;

/**
 * @brief Create a ptrace options object
 *
 * @param f Flags that control the attach behavior.
 * @return ptrace options to use when attaching to a thread.
 */
long create_ptrace_options(Thread::Flag f) {
	long options = TraceOptions;
	if (f & Thread::KillOnTracerExit) {
		options |= PTRACE_O_EXITKILL;
	}
	return options;
}

}

/**
 * @brief Construct a new Thread object.
 *
 * @param tid The thread id to attach to.
 * @param f Controls the attach behavior of this constructor.
 */
Thread::Thread(pid_t pid, pid_t tid, Flag f)
	: pid_(pid), tid_(tid) {

	if (f & Thread::Attach) {
		if (ptrace(PTRACE_ATTACH, tid, 0L, 0L) == -1) {
			throw DebuggerError("Failed to attach to thread %d: %s", tid, strerror(errno));
		}
	}

	wait();

	const long options = create_ptrace_options(f);
	if (ptrace(PTRACE_SETOPTIONS, tid, 0L, options) != 0) {
		throw DebuggerError("Failed to set ptrace options for thread %d: %s", tid, strerror(errno));
	}

	is_64_bit_ = detect_64_bit();
}

/**
 * @brief Destroy the Thread object.
 */
Thread::~Thread() {
	detach();
}

/**
 * @brief Detects if the thread is 64-bit or 32-bit.
 *
 * @return true if the thread is 64-bit, false otherwise.
 */
bool Thread::detect_64_bit() const {

	assert(state_ == State::Stopped);

	// determine if this thread is 64-bit or 32-bit
	alignas(Context::BufferAlign) char buffer[Context::BufferSize];
	struct iovec iov = {buffer, sizeof(buffer)};
	if (ptrace(PTRACE_GETREGSET, tid_, NT_PRSTATUS, &iov) == -1) {
		throw DebuggerError("Failed to get register set for thread %d: %s", tid_, strerror(errno));
	}

	switch (iov.iov_len) {
	case sizeof(Context_x86_32):
		return false;
	case sizeof(Context_x86_64):
		return true;
	default:
		throw DebuggerError("Unknown iov_len: %zu", iov.iov_len);
	}
}

/**
 * @brief Waits for an event on this thread.
 */
void Thread::wait() {

	assert(state_ == State::Running);

	if (waitpid(tid_, &wstatus_, __WALL) == -1) {
		throw DebuggerError("Failed to wait for thread %d: %s", tid_, strerror(errno));
	}

	state_ = State::Stopped;
}

/**
 * @brief Detaches from the associated thread, if any.
 * no-op if already detached
 */
void Thread::detach() {
	if (tid_ != -1) {
		ptrace(PTRACE_DETACH, tid_, 0L, 0L);
		tid_ = -1;
	}
}

/**
 * @brief Causes the thread to step one instruction. This will be
 * eventually followed by a debug event when it stops again.
 */
void Thread::step() {

	assert(state_ == State::Stopped);

	if (ptrace(PTRACE_SINGLESTEP, tid_, 0L, 0L) == -1) {
		throw DebuggerError("Failed to step thread %d: %s", tid_, strerror(errno));
	}

	state_ = State::Running;
}

/**
 * @brief Causes the thread to resume execution.
 */
void Thread::resume() {

	assert(state_ == State::Stopped);

	if (ptrace(PTRACE_CONT, tid_, 0L, 0L) == -1) {
		throw DebuggerError("Failed to continue thread %d: %s", tid_, strerror(errno));
	}

	state_ = State::Running;
}

/**
 * @brief Causes a running thread the stop execution. This will be
 * eventually followed by a debug event when it actually stops.
 */
void Thread::stop() const {
	assert(state_ == State::Running);

	if (syscall(SYS_tgkill, pid_, tid_, SIGSTOP) == -1) {
		throw DebuggerError("Failed to stop thread %d: %s", tid_, strerror(errno));
	}
}

/**
 * @brief Terminates this thread.
 */
void Thread::kill() const {
	assert(state_ == State::Running);

	if (syscall(SYS_tgkill, pid_, tid_, SIGKILL) == -1) {
		throw DebuggerError("Failed to kill thread %d: %s", tid_, strerror(errno));
	}
}

/**
 * @brief Checks if the thread status is exited.
 *
 * @return true if the thread status is exited, false otherwise.
 */
bool Thread::is_exited() const {
	assert(state_ == State::Stopped);
	return WIFEXITED(wstatus_);
}

/**
 * @brief Checks if the thread status is signaled.
 *
 * @return true if the thread status is signaled, false otherwise.
 */
bool Thread::is_signaled() const {
	assert(state_ == State::Stopped);
	return WIFSIGNALED(wstatus_);
}

/**
 * @brief Checks if the thread status is stopped.
 *
 * @return true if the thread status is stopped, false otherwise.
 */
bool Thread::is_stopped() const {
	assert(state_ == State::Stopped);
	return WIFSTOPPED(wstatus_);
}

/**
 * @brief Checks if the thread status is continued.
 *
 * @return true if the thread status is continued, false otherwise.
 */
bool Thread::is_continued() const {
	assert(state_ == State::Stopped);
	return WIFCONTINUED(wstatus_);
}

/**
 * @brief Retrieves the exit status of the thread.
 *
 * @return The exit status of the thread.
 */
int Thread::exit_status() const {
	assert(state_ == State::Stopped);
	return WEXITSTATUS(wstatus_);
}

/**
 * @brief Retrieves the signal status of the thread.
 *
 * @return The signal status of the thread.
 */
int Thread::signal_status() const {
	assert(state_ == State::Stopped);
	return WTERMSIG(wstatus_);
}

/**
 * @brief Retrieves the stop status of the thread.
 *
 * @return The stop status of the thread.
 */
int Thread::stop_status() const {
	assert(state_ == State::Stopped);
	return WSTOPSIG(wstatus_);
}

/**
 * @brief Retrieves the thread GP registers.
 *
 * @param ctx A pointer to the context object.
 */
void Thread::get_registers(Context *ctx) const {
#if defined(__x86_64__)
	get_registers64(ctx);
#elif defined(__i386__)
	get_registers32(ctx);
#endif
}

/**
 * @brief Retrieves the thread GP registers (64-bit).
 *
 * @param ctx A pointer to the context object.
 */
void Thread::get_registers64(Context *ctx) const {
	// 64-bit GETREGS is always 64-bit even if the thread is 32-bit
	// We don't use NT_PRSTATUS because this API correctly normalizes the
	// registers to 64-bit even if the thread is 32-bit.
	if (ptrace(PTRACE_GETREGS, tid_, 0, &ctx->ctx_64_.regs) == -1) {
		throw DebuggerError("Failed to get registers for thread %d: %s", tid_, strerror(errno));
	}
}

/**
 * @brief Retrieves the thread GP registers (32-bit).
 *
 * @param ctx A pointer to the context object.
 */
void Thread::get_registers32(Context *ctx) const {
	struct iovec iov;

	if (is_64_bit_) {
		iov.iov_base = &ctx->ctx_64_.regs;
		iov.iov_len  = sizeof(ctx->ctx_64_.regs);
	} else {
		iov.iov_base = &ctx->ctx_32_.regs;
		iov.iov_len  = sizeof(ctx->ctx_32_.regs);
	}

	if (ptrace(PTRACE_GETREGSET, tid_, NT_PRSTATUS, &iov) == -1) {
		throw DebuggerError("Failed to get registers for thread %d: %s", tid_, strerror(errno));
	}
}

/**
 * @brief Retrieves the thread xstate.
 *
 * @param ctx A pointer to the context object.
 */
void Thread::get_xstate(Context *ctx) const {
#if defined(__x86_64__)
	get_xstate64(ctx);
#elif defined(__i386__)
	if (is_64_bit_) {
		get_xstate64(ctx);
	} else {
		get_xstate32(ctx);
	}
#endif
}

/**
 * @brief Retrieves the thread xstate (64-bit).
 *
 * @param ctx A pointer to the context object.
 */
void Thread::get_xstate64(Context *ctx) const {

#if 0
	// Possible sizes of X86_XSTATE
	static constexpr size_t BNDREGS_SIZE           = 1024;
	static constexpr size_t BNDCFG_SIZE            = 1088;
	static constexpr size_t AVX512_SIZE            = 2688;
	static constexpr size_t MAX_SIZE               = 2688;
#endif

	auto xsave       = &ctx->ctx_64_xstate_;
	struct iovec iov = {xsave, sizeof(Context_x86_64_xstate)};

	if (ptrace(PTRACE_GETREGSET, tid_, NT_X86_XSTATE, &iov) == -1) {
		throw DebuggerError("Failed to get xstate for thread %d: %s", tid_, strerror(errno));
	}

	const bool x87_present = xsave->xstate_bv & FEATURE_X87;
	const bool sse_present = xsave->xstate_bv & FEATURE_SSE;
	const bool avx_present = xsave->xstate_bv & FEATURE_AVX;
	const bool zmm_present = (xsave->xstate_bv & FEATURE_AVX512) == FEATURE_AVX512;

	std::printf("Thread::get_xstate64: size=%zu, xstate_bv=0x%016" PRIx64 " (x87=%d sse=%d avx=%d zmm=%d)\n",
				iov.iov_len,
				xsave->xstate_bv,
				static_cast<int>(x87_present),
				static_cast<int>(sse_present),
				static_cast<int>(avx_present),
				static_cast<int>(zmm_present));

	// Due to the lazy saving the feature bits may be unset in XSTATE_BV if the app
	// has not touched the corresponding registers yet. But once the registers are
	// touched, they are initialized to zero by the OS (not control/tag ones). To the app
	// it looks as if the registers have always been zero. Thus we should provide the same
	// illusion to the user.
	constexpr size_t FpuRegisterCount = 8;
	constexpr size_t FpuRegisterSize  = 16;
	constexpr size_t SseRegisterCount = 32; // SIMD structure now has 32 registers (ZMM0-ZMM31)
	constexpr size_t SseRegisterSize  = 16; // Each XMM register is 128 bits = 16 bytes
	constexpr size_t AvxRegisterSize  = 32; // Each YMM register is 256 bits = 32 bytes
	constexpr size_t ZmmRegisterSize  = 64; // Each ZMM register is 512 bits = 64 bytes

	if (x87_present) {
		ctx->xstate_.x87.status_word = xsave->swd; // should be first for RIndexToSTIndex() to work

		for (size_t n = 0; n < FpuRegisterCount; ++n) {
			std::memcpy(ctx->xstate_.x87.registers[n].data, xsave->st_space + FpuRegisterSize * xsave->st_space[n * FpuRegisterSize], FpuRegisterSize);
		}

		ctx->xstate_.x87.control_word      = xsave->cwd;
		ctx->xstate_.x87.tag_word          = xsave->ftw;
		ctx->xstate_.x87.inst_ptr_offset   = xsave->rip;
		ctx->xstate_.x87.data_ptr_offset   = xsave->rdp;
		ctx->xstate_.x87.inst_ptr_selector = 0;
		ctx->xstate_.x87.data_ptr_selector = 0;
		ctx->xstate_.x87.opcode            = xsave->fop;
		ctx->xstate_.x87.filled            = true;
	} else {

		for (size_t n = 0; n < FpuRegisterCount; ++n) {
			std::memset(ctx->xstate_.x87.registers[n].data, 0, FpuRegisterSize);
		}

		ctx->xstate_.x87              = {};
		ctx->xstate_.x87.control_word = xsave->cwd; // this appears always present
		ctx->xstate_.x87.tag_word     = 0xffff;
		ctx->xstate_.x87.filled       = true;
	}

	// NOTE(eteran): on 64-bit systems, SSE is always present, so we always populate it
	// even if xsave->xstate_bv & FEATURE_SSE is false.
	if (sse_present) {
		ctx->xstate_.simd.mxcsr      = xsave->mxcsr;
		ctx->xstate_.simd.mxcsr_mask = xsave->mxcr_mask;

		// Only populate ZMM0-ZMM15 for SSE (XMM registers)
		for (size_t n = 0; n < 16; ++n) {
			// Copy the 128-bit XMM register data to the first 16 bytes of the AvxRegister
			std::memcpy(ctx->xstate_.simd.registers[n].data, xsave->xmm_space + SseRegisterSize * n, SseRegisterSize);
			// Clear the upper 240 bytes since SSE only uses the lower 128 bits
			std::memset(ctx->xstate_.simd.registers[n].data + SseRegisterSize, 0, sizeof(ctx->xstate_.simd.registers[n].data) - SseRegisterSize);
		}

		// Initialize ZMM16-ZMM31 to zero for SSE
		for (size_t n = 16; n < SseRegisterCount; ++n) {
			std::memset(ctx->xstate_.simd.registers[n].data, 0, sizeof(ctx->xstate_.simd.registers[n].data));
		}

		ctx->xstate_.simd.sse_filled = true;
	} else {
		// SSE not present, initialize with zeros
		ctx->xstate_.simd.mxcsr      = 0x1f80; // Default MXCSR value
		ctx->xstate_.simd.mxcsr_mask = 0;

		for (size_t n = 0; n < SseRegisterCount; ++n) {
			std::memset(ctx->xstate_.simd.registers[n].data, 0, sizeof(ctx->xstate_.simd.registers[n].data));
		}

		ctx->xstate_.simd.sse_filled = true;
	}

	if (avx_present) {
		// AVX extends the XMM registers to YMM registers (256 bits)
		// The lower 128 bits are already populated by SSE above
		// We need to populate the upper 128 bits from the AVX extended state

		// AVX state starts after the legacy area (576 bytes) in the xsave buffer
		// Each YMM register's upper 128 bits are stored sequentially
		constexpr size_t AvxStateOffset = 576;
		constexpr size_t AvxUpperSize   = AvxRegisterSize - SseRegisterSize;

		auto avx_upper_data = reinterpret_cast<const uint8_t *>(xsave) + AvxStateOffset;

		// Only populate ZMM0-ZMM15 for AVX (YMM registers)
		for (size_t n = 0; n < 16; ++n) {
			// Copy the upper 128 bits (bytes 16-31) of each YMM register
			std::memcpy(ctx->xstate_.simd.registers[n].data + SseRegisterSize, avx_upper_data + AvxUpperSize * n, AvxUpperSize);
			// Clear the remaining 224 bytes (bytes 32-255)
			std::memset(ctx->xstate_.simd.registers[n].data + SseRegisterSize + AvxUpperSize, 0, sizeof(ctx->xstate_.simd.registers[n].data) - SseRegisterSize - AvxUpperSize);
		}

		ctx->xstate_.simd.avx_filled = true;
	}

	if (zmm_present) {
		// AVX-512 extends the YMM registers to ZMM registers (512 bits)
		// The lower 256 bits are already populated by SSE and AVX above
		// We need to populate the upper 256 bits from the AVX-512 extended state

		// AVX-512 state layout in xsave buffer:
		// - Opmask state (K0-K7) starts at offset 1088
		// - ZMM_Hi256 state (upper 256 bits of ZMM0-ZMM15) starts at offset 1152
		// - Hi16_ZMM state (ZMM16-ZMM31, full 512 bits) starts at offset 1664

		constexpr size_t ZmmHi256StateOffset = 1152;                              // Upper 256 bits of ZMM0-ZMM15
		constexpr size_t Hi16ZmmStateOffset  = 1664;                              // Full 512 bits of ZMM16-ZMM31
		constexpr size_t ZmmUpperSize        = ZmmRegisterSize - AvxRegisterSize; // 64 - 32 = 32 bytes

		const uint8_t *xsave_data = reinterpret_cast<const uint8_t *>(xsave);

		// Handle ZMM0-ZMM15: populate upper 256 bits
		const uint8_t *zmm_upper_data = xsave_data + ZmmHi256StateOffset;
		for (size_t n = 0; n < 16; ++n) {
			// Copy the upper 256 bits (bytes 32-63) of each ZMM register
			std::memcpy(ctx->xstate_.simd.registers[n].data + AvxRegisterSize, zmm_upper_data + ZmmUpperSize * n, ZmmUpperSize);
		}

		// Handle ZMM16-ZMM31: populate full 512 bits
		const uint8_t *hi16_zmm_data = xsave_data + Hi16ZmmStateOffset;
		for (size_t n = 16; n < 32; ++n) {
			// Copy the full 512 bits (bytes 0-63) of each ZMM register
			std::memcpy(ctx->xstate_.simd.registers[n].data, hi16_zmm_data + ZmmRegisterSize * (n - 16), ZmmRegisterSize);
		}

		ctx->xstate_.simd.zmm_filled = true;
	}
}
/**
 * @brief Retrieves the thread xstate using NT_X86_XSTATE (32-bit).
 *
 * @param ctx A pointer to the context object.
 */
int Thread::get_xstate32_modern(Context *ctx) const {
	auto xsave = &ctx->ctx_32_xstate_;

	struct iovec iov = {xsave, sizeof(Context_x86_32_xstate)};
	if (ptrace(PTRACE_GETREGSET, tid_, NT_X86_XSTATE, &iov) == -1) {
		return errno;
	}

	const bool x87_present = xsave->xstate_bv & FEATURE_X87;
	const bool sse_present = xsave->xstate_bv & FEATURE_SSE;
	const bool avx_present = xsave->xstate_bv & FEATURE_AVX;

	std::printf("Thread::get_xstate32: size=%zu, xstate_bv=0x%016" PRIx64 " (x87=%d sse=%d avx=%d)\n",
				iov.iov_len,
				xsave->xstate_bv,
				static_cast<int>(x87_present),
				static_cast<int>(sse_present),
				static_cast<int>(avx_present));

	// Due to the lazy saving the feature bits may be unset in XSTATE_BV if the app
	// has not touched the corresponding registers yet. But once the registers are
	// touched, they are initialized to zero by the OS (not control/tag ones). To the app
	// it looks as if the registers have always been zero. Thus we should provide the same
	// illusion to the user.
	constexpr size_t FpuRegisterCount = 8;
	constexpr size_t FpuRegisterSize  = 16;
	constexpr size_t SseRegisterCount = 32; // SIMD structure now has 32 registers
	constexpr size_t SseRegisterSize  = 16; // Each XMM register is 128 bits = 16 bytes
	constexpr size_t AvxRegisterSize  = 32; // Each YMM register is 256 bits = 32 bytes

	if (x87_present) {
		ctx->xstate_.x87.status_word = xsave->swd;

		// Convert 32-bit st_space format to our format
		// st_space in 32-bit format contains 8 registers of 16 bytes each, stored as uint32_t[32]
		for (size_t n = 0; n < FpuRegisterCount; ++n) {
			// Each register is 16 bytes = 4 uint32_t values
			const uint32_t *reg_data = &xsave->st_space[n * 4];
			std::memcpy(ctx->xstate_.x87.registers[n].data, reg_data, FpuRegisterSize);
		}

		ctx->xstate_.x87.control_word      = xsave->cwd;
		ctx->xstate_.x87.tag_word          = xsave->twd;
		ctx->xstate_.x87.inst_ptr_offset   = xsave->fip;
		ctx->xstate_.x87.data_ptr_offset   = xsave->foo;
		ctx->xstate_.x87.inst_ptr_selector = static_cast<uint16_t>(xsave->fcs);
		ctx->xstate_.x87.data_ptr_selector = static_cast<uint16_t>(xsave->fos);
		ctx->xstate_.x87.opcode            = xsave->fop;
		ctx->xstate_.x87.filled            = true;
	} else {
		for (size_t n = 0; n < FpuRegisterCount; ++n) {
			std::memset(ctx->xstate_.x87.registers[n].data, 0, FpuRegisterSize);
		}

		ctx->xstate_.x87              = {};
		ctx->xstate_.x87.control_word = xsave->cwd; // this appears always present
		ctx->xstate_.x87.tag_word     = 0xffff;
		ctx->xstate_.x87.filled       = true;
	}

	if (sse_present) {
		ctx->xstate_.simd.mxcsr      = xsave->mxcsr;
		ctx->xstate_.simd.mxcsr_mask = xsave->mxcsr_mask;

		// Only populate first 8 XMM registers for 32-bit (XMM0-XMM7)
		// 32-bit x86 only has 8 XMM registers, not 16 like x86-64
		for (size_t n = 0; n < 8; ++n) {
			// Convert 32-bit xmm_space format to our format
			// xmm_space contains 8 registers of 16 bytes each, stored as uint32_t[16]
			const uint32_t *reg_data = &xsave->xmm_space[n * 4];
			std::memcpy(ctx->xstate_.simd.registers[n].data, reg_data, SseRegisterSize);
			// Clear the upper 240 bytes since SSE only uses the lower 128 bits
			std::memset(ctx->xstate_.simd.registers[n].data + SseRegisterSize, 0, sizeof(ctx->xstate_.simd.registers[n].data) - SseRegisterSize);
		}

		// Initialize remaining registers (XMM8-XMM31) to zero since 32-bit doesn't have them
		for (size_t n = 8; n < SseRegisterCount; ++n) {
			std::memset(ctx->xstate_.simd.registers[n].data, 0, sizeof(ctx->xstate_.simd.registers[n].data));
		}

		ctx->xstate_.simd.sse_filled = true;
	} else {
		// SSE not present, initialize with zeros
		ctx->xstate_.simd.mxcsr      = 0x1f80; // Default MXCSR value
		ctx->xstate_.simd.mxcsr_mask = 0;

		for (size_t n = 0; n < SseRegisterCount; ++n) {
			std::memset(ctx->xstate_.simd.registers[n].data, 0, sizeof(ctx->xstate_.simd.registers[n].data));
		}

		ctx->xstate_.simd.sse_filled = true;
	}

	if (avx_present) {
		// AVX extends the XMM registers to YMM registers (256 bits)
		// The lower 128 bits are already populated by SSE above
		// We need to populate the upper 128 bits from the AVX extended state

		// AVX state starts in the buffer area for 32-bit
		// The buffer contains the extended state areas
		constexpr size_t AvxUpperSize = AvxRegisterSize - SseRegisterSize; // 32 - 16 = 16 bytes

		// For 32-bit, AVX upper halves are stored in the buffer area
		// They start at the beginning of the buffer (not offset by header bytes)
		const uint8_t *avx_upper_data = xsave->buffer;

		// Only populate first 8 YMM registers for 32-bit (YMM0-YMM7)
		for (size_t n = 0; n < 8; ++n) {
			// Copy the upper 128 bits (bytes 16-31) of each YMM register
			std::memcpy(ctx->xstate_.simd.registers[n].data + SseRegisterSize, avx_upper_data + AvxUpperSize * n, AvxUpperSize);
			// Clear the remaining 224 bytes (bytes 32-255) since 32-bit AVX only uses 256 bits
			std::memset(ctx->xstate_.simd.registers[n].data + AvxRegisterSize, 0, sizeof(ctx->xstate_.simd.registers[n].data) - AvxRegisterSize);
		}

		ctx->xstate_.simd.avx_filled = true;
	}

	// 32-bit doesn't support AVX-512, so this remains unset
	ctx->xstate_.simd.zmm_filled = false;
	return 0;
}

/**
 * @brief Retrieves the thread xstate using legacy APIs (32-bit).
 *
 * @param ctx A pointer to the context object.
 */
int Thread::get_xstate32_legacy(Context *ctx) const {
	// Try to get SSE/MMX state using PTRACE_GETFPXREGS first
	// Note: user_fpxregs_struct is from older kernel headers, use Context_x86_32_xstate as fallback
	Context_x86_32_xstate fpxregs;
	if (ptrace(PTRACE_GETFPXREGS, tid_, 0, &fpxregs) == 0) {
		// Successfully got extended FP state (SSE + x87)

		// Extract x87 state from fpxregs
		ctx->xstate_.x87.control_word      = fpxregs.cwd;
		ctx->xstate_.x87.status_word       = fpxregs.swd;
		ctx->xstate_.x87.tag_word          = fpxregs.twd;
		ctx->xstate_.x87.opcode            = fpxregs.fop;
		ctx->xstate_.x87.inst_ptr_offset   = fpxregs.fip;
		ctx->xstate_.x87.data_ptr_offset   = fpxregs.foo;
		ctx->xstate_.x87.inst_ptr_selector = static_cast<uint16_t>(fpxregs.fcs);
		ctx->xstate_.x87.data_ptr_selector = static_cast<uint16_t>(fpxregs.fos);

		// Copy x87 register data (8 registers of 16 bytes each)
		constexpr size_t FpuRegisterCount = 8;
		constexpr size_t FpuRegisterSize  = 16;
		for (size_t n = 0; n < FpuRegisterCount; ++n) {
			// Convert from uint32_t array format to byte array
			const uint32_t *reg_data = &fpxregs.st_space[n * 4];
			std::memcpy(ctx->xstate_.x87.registers[n].data, reg_data, FpuRegisterSize);
		}
		ctx->xstate_.x87.filled = true;

		// Extract SSE state from fpxregs
		ctx->xstate_.simd.mxcsr      = fpxregs.mxcsr;
		ctx->xstate_.simd.mxcsr_mask = fpxregs.mxcsr_mask;

		// Copy XMM registers (8 registers for 32-bit, 16 bytes each)
		constexpr size_t SseRegisterCount = 32; // Total SIMD structure size
		constexpr size_t SseRegisterSize  = 16; // Each XMM register is 128 bits

		for (size_t n = 0; n < 8; ++n) { // 32-bit x86 only has XMM0-XMM7
			// Copy XMM register data (128 bits)
			const uint32_t *reg_data = &fpxregs.xmm_space[n * 4];
			std::memcpy(ctx->xstate_.simd.registers[n].data, reg_data, SseRegisterSize);
			// Clear upper portions since legacy API only supports SSE (128-bit)
			std::memset(ctx->xstate_.simd.registers[n].data + SseRegisterSize, 0,
						sizeof(ctx->xstate_.simd.registers[n].data) - SseRegisterSize);
		}

		// Initialize remaining registers (XMM8-XMM31) to zero since 32-bit doesn't have them
		for (size_t n = 8; n < SseRegisterCount; ++n) {
			std::memset(ctx->xstate_.simd.registers[n].data, 0, sizeof(ctx->xstate_.simd.registers[n].data));
		}

		ctx->xstate_.simd.sse_filled = true;
		ctx->xstate_.simd.avx_filled = false; // Legacy API doesn't support AVX
		ctx->xstate_.simd.zmm_filled = false; // Legacy API doesn't support AVX-512

		return 0;
	}

#if 0
	// PTRACE_GETFPXREGS failed, try legacy PTRACE_GETFPREGS for basic x87 state
	struct user_fpregs_struct fpregs;
	if (ptrace(PTRACE_GETFPREGS, tid_, 0, &fpregs) == 0) {
		// Successfully got basic x87 FP state only

		ctx->xstate_.x87.control_word      = fpregs.cwd;
		ctx->xstate_.x87.status_word       = fpregs.swd;
		ctx->xstate_.x87.tag_word          = fpregs.ftw; // Note: ftw is different from twd
		ctx->xstate_.x87.opcode            = fpregs.fop;

		// For x86-64, the structure uses rip/rdp instead of fip/fcs/foo/fos
		ctx->xstate_.x87.inst_ptr_offset   = static_cast<uint32_t>(fpregs.rip & 0xFFFFFFFF);
		ctx->xstate_.x87.data_ptr_offset   = static_cast<uint32_t>(fpregs.rdp & 0xFFFFFFFF);
		ctx->xstate_.x87.inst_ptr_selector = static_cast<uint16_t>((fpregs.rip >> 32) & 0xFFFF);
		ctx->xstate_.x87.data_ptr_selector = static_cast<uint16_t>((fpregs.rdp >> 32) & 0xFFFF);

		// Copy x87 register data from st_space (32-bit words -> 80-bit registers)
		constexpr size_t FpuRegisterCount = 8;

		for (size_t n = 0; n < FpuRegisterCount; ++n) {
			// st_space contains 32 words (128 bytes total), 4 words per 80-bit register
			const auto* src = &fpregs.st_space[n * 4];
			auto* dst = ctx->xstate_.x87.registers[n].data;

			// Copy 80-bit (10 bytes) from the 4 32-bit words
			std::memcpy(dst, src, 10);
			std::memset(dst + 10, 0, 6); // Zero-pad to 16 bytes
		}
		ctx->xstate_.x87.filled = true;

		// Copy XMM registers from xmm_space for SSE state
		ctx->xstate_.simd.mxcsr      = fpregs.mxcsr;
		ctx->xstate_.simd.mxcsr_mask = fpregs.mxcr_mask;

		constexpr size_t XmmRegisterCount = 16; // x86-64 has 16 XMM registers
		for (size_t n = 0; n < XmmRegisterCount; ++n) {
			// xmm_space contains 64 words (256 bytes total), 4 words per 128-bit XMM register
			const auto* src = &fpregs.xmm_space[n * 4];
			auto* dst = ctx->xstate_.simd.registers[n].data;

			std::memcpy(dst, src, 16); // Copy 128-bit XMM register
			std::memset(dst + 16, 0, 48); // Zero upper YMM/ZMM portions
		}

		// Clear remaining SIMD registers (17-31)
		for (size_t n = XmmRegisterCount; n < 32; ++n) {
			std::memset(ctx->xstate_.simd.registers[n].data, 0, sizeof(ctx->xstate_.simd.registers[n].data));
		}

		ctx->xstate_.simd.sse_filled = true;  // XMM registers are available
		ctx->xstate_.simd.avx_filled = false; // No AVX support in legacy API
		ctx->xstate_.simd.zmm_filled = false;

		std::printf("Thread::get_xstate32_legacy: Successfully retrieved x87+SSE state via PTRACE_GETFPREGS\n");
		return 0; // Success
	}
#endif

	// Both legacy APIs failed
	return errno;
}

/**
 * @brief Retrieves the thread xstate (32-bit).
 *
 * @param ctx A pointer to the context object.
 */
void Thread::get_xstate32(Context *ctx) const {
	int ret = get_xstate32_modern(ctx);
	if (ret != 0) {
		ret = get_xstate32_legacy(ctx);
		if (ret != 0) {
			throw DebuggerError("Failed to get xstate for thread %d: %s", tid_, strerror(errno));
		}
	}
}

/**
 * @brief Retrieves the thread hardware debug registers.
 *
 * @param ctx A pointer to the context object.
 */
void Thread::get_debug_registers(Context *ctx) const {
#ifdef __x86_64__
	get_debug_registers64(ctx);
#elif defined(__i386__)
	if (is_64_bit_) {
		// NOTE(eteran): on 32-bit systems, this doesn't work correctly
		// because the debug registers are 64-bit, but the ONLY way to
		// retrieve them is through PTRACE_PEEKUSER which only works with 32-bit
		// registers.
		printf("get_debug_registers called on 64-bit thread with 32-bit debugger\n");
	} else {
		get_debug_registers32(ctx);
	}
#endif
}

/**
 * @brief Retrieves the thread hardware debug registers (64-bit).
 *
 * @param ctx A pointer to the context object.
 */
void Thread::get_debug_registers64(Context *ctx) const {
	ctx->ctx_64_.debug_regs[0] = static_cast<uint64_t>(ptrace(PTRACE_PEEKUSER, tid_, offsetof(struct user, u_debugreg[0]), 0L));
	ctx->ctx_64_.debug_regs[1] = static_cast<uint64_t>(ptrace(PTRACE_PEEKUSER, tid_, offsetof(struct user, u_debugreg[1]), 0L));
	ctx->ctx_64_.debug_regs[2] = static_cast<uint64_t>(ptrace(PTRACE_PEEKUSER, tid_, offsetof(struct user, u_debugreg[2]), 0L));
	ctx->ctx_64_.debug_regs[3] = static_cast<uint64_t>(ptrace(PTRACE_PEEKUSER, tid_, offsetof(struct user, u_debugreg[3]), 0L));
	ctx->ctx_64_.debug_regs[4] = static_cast<uint64_t>(ptrace(PTRACE_PEEKUSER, tid_, offsetof(struct user, u_debugreg[4]), 0L));
	ctx->ctx_64_.debug_regs[5] = static_cast<uint64_t>(ptrace(PTRACE_PEEKUSER, tid_, offsetof(struct user, u_debugreg[5]), 0L));
	ctx->ctx_64_.debug_regs[6] = static_cast<uint64_t>(ptrace(PTRACE_PEEKUSER, tid_, offsetof(struct user, u_debugreg[6]), 0L));
	ctx->ctx_64_.debug_regs[7] = static_cast<uint64_t>(ptrace(PTRACE_PEEKUSER, tid_, offsetof(struct user, u_debugreg[7]), 0L));
}

/**
 * @brief Retrieves the thread hardware debug registers (32-bit).
 *
 * @param ctx A pointer to the context object.
 */
void Thread::get_debug_registers32(Context *ctx) const {
	// TODO(eteran): i think this might be incorrect.
	ctx->ctx_32_.debug_regs[0] = static_cast<uint32_t>(ptrace(PTRACE_PEEKUSER, tid_, offsetof(struct user, u_debugreg[0]), 0L));
	ctx->ctx_32_.debug_regs[1] = static_cast<uint32_t>(ptrace(PTRACE_PEEKUSER, tid_, offsetof(struct user, u_debugreg[1]), 0L));
	ctx->ctx_32_.debug_regs[2] = static_cast<uint32_t>(ptrace(PTRACE_PEEKUSER, tid_, offsetof(struct user, u_debugreg[2]), 0L));
	ctx->ctx_32_.debug_regs[3] = static_cast<uint32_t>(ptrace(PTRACE_PEEKUSER, tid_, offsetof(struct user, u_debugreg[3]), 0L));
	ctx->ctx_32_.debug_regs[4] = static_cast<uint32_t>(ptrace(PTRACE_PEEKUSER, tid_, offsetof(struct user, u_debugreg[4]), 0L));
	ctx->ctx_32_.debug_regs[5] = static_cast<uint32_t>(ptrace(PTRACE_PEEKUSER, tid_, offsetof(struct user, u_debugreg[5]), 0L));
	ctx->ctx_32_.debug_regs[6] = static_cast<uint32_t>(ptrace(PTRACE_PEEKUSER, tid_, offsetof(struct user, u_debugreg[6]), 0L));
	ctx->ctx_32_.debug_regs[7] = static_cast<uint32_t>(ptrace(PTRACE_PEEKUSER, tid_, offsetof(struct user, u_debugreg[7]), 0L));
}

/**
 * @brief Get a specific segment base for a given segment register.
 *
 * @param ctx A pointer to the context object.
 * @param reg The register to get the segment base for.
 * @return The segment base.
 */
uint32_t Thread::get_segment_base(Context *ctx, RegisterId reg) const {

	uint16_t segment = ctx->get(reg).as<uint16_t>();
	if (segment == 0) {
		return 0;
	}

	// otherwise the selector picks descriptor from LDT
	const bool from_gdt = !(segment & 0x04);
	if (!from_gdt) {
		return 0;
	}

	struct user_desc desc;
	if (ptrace(PTRACE_GET_THREAD_AREA, tid_, segment / LDT_ENTRY_SIZE, &desc) == -1) {
		std::perror("ptrace(PTRACE_GET_THREAD_AREA)");
		return 0;
	}

	return desc.base_addr;
}

/**
 * @brief Get the segment bases
 *
 * @param ctx A pointer to the context object.
 */
void Thread::get_segment_bases([[maybe_unused]] Context *ctx) const {

	// NOTE(eteran): on x86-64, FS and GS are already populated as part of the context
	// so we don't need to do anything here.
	if (!is_64_bit_) {
		ctx->ctx_32_.gs_base = get_segment_base(ctx, RegisterId::GS);
		ctx->ctx_32_.fs_base = get_segment_base(ctx, RegisterId::FS);
	}
}

/**
 * @brief Retrieves the thread context.
 *
 * @param ctx A pointer to the context object.
 */
void Thread::get_context(Context *ctx) const {

	assert(state_ == State::Stopped);

	ctx->is_64_bit_ = is_64_bit_;
	ctx->is_set_    = true;

	get_registers(ctx);
	get_xstate(ctx);
	get_debug_registers(ctx);
	get_segment_bases(ctx);
}

/**
 * @brief Sets the thread 64-bit registers.
 *
 * @param ctx A pointer to the context object.
 */
void Thread::set_registers64(const Context *ctx) const {
	// See get_registers64
	if (ptrace(PTRACE_SETREGS, tid_, 0, &ctx->ctx_64_.regs) == -1) {
		throw DebuggerError("Failed to set registers for thread %d: %s", tid_, strerror(errno));
	}
}

/**
 * @brief Sets the thread 32-bit registers.
 *
 * @param ctx A pointer to the context object.
 */
void Thread::set_registers32(const Context *ctx) const {
	struct iovec iov;
	if (is_64_bit_) {
		iov.iov_base = const_cast<Context_x86_64 *>(&ctx->ctx_64_.regs);
		iov.iov_len  = sizeof(ctx->ctx_64_.regs);
	} else {
		iov.iov_base = const_cast<Context_x86_32 *>(&ctx->ctx_32_.regs);
		iov.iov_len  = sizeof(ctx->ctx_32_.regs);
	}

	if (ptrace(PTRACE_SETREGSET, tid_, NT_PRSTATUS, &iov) == -1) {
		throw DebuggerError("Failed to set registers for thread %d: %s", tid_, strerror(errno));
	}
}

/**
 * @brief Sets the thread xstate (64-bit or 32-bit).
 *
 * @param ctx A pointer to the context object.
 */
void Thread::set_xstate64(const Context *ctx) const {

	// Prepare the xsave buffer structure
	auto xsave = &ctx->ctx_64_xstate_;

	// Set up basic control/status fields from x87 state
	if (ctx->xstate_.x87.filled) {
		xsave->cwd = ctx->xstate_.x87.control_word;
		xsave->swd = ctx->xstate_.x87.status_word;
		xsave->ftw = ctx->xstate_.x87.tag_word;
		xsave->fop = ctx->xstate_.x87.opcode;
		xsave->rip = ctx->xstate_.x87.inst_ptr_offset;
		xsave->rdp = ctx->xstate_.x87.data_ptr_offset;

		// Copy x87 register data
		constexpr size_t FpuRegisterCount = 8;
		constexpr size_t FpuRegisterSize  = 16;
		for (size_t n = 0; n < FpuRegisterCount; ++n) {
			std::memcpy(xsave->st_space + FpuRegisterSize * n,
						ctx->xstate_.x87.registers[n].data,
						FpuRegisterSize);
		}

		xsave->xstate_bv |= FEATURE_X87;
	}

	// Set up SIMD state
	if (ctx->xstate_.simd.sse_filled) {
		xsave->mxcsr     = ctx->xstate_.simd.mxcsr;
		xsave->mxcr_mask = ctx->xstate_.simd.mxcsr_mask;

		// Copy XMM register data (lower 128 bits of ZMM0-ZMM15)
		constexpr size_t SseRegisterSize = 16;
		for (size_t n = 0; n < 16; ++n) {
			std::memcpy(xsave->xmm_space + SseRegisterSize * n, ctx->xstate_.simd.registers[n].data, SseRegisterSize);
		}

		xsave->xstate_bv |= FEATURE_SSE;
	}

	// Set up AVX state (upper 128 bits of YMM0-YMM15)
	if (ctx->xstate_.simd.avx_filled) {
		constexpr size_t AvxStateOffset  = 576;
		constexpr size_t AvxRegisterSize = 32;
		constexpr size_t SseRegisterSize = 16;
		constexpr size_t AvxUpperSize    = AvxRegisterSize - SseRegisterSize;

		uint8_t *avx_upper_data = reinterpret_cast<uint8_t *>(xsave) + AvxStateOffset;
		for (size_t n = 0; n < 16; ++n) {
			std::memcpy(avx_upper_data + AvxUpperSize * n, ctx->xstate_.simd.registers[n].data + SseRegisterSize, AvxUpperSize);
		}

		xsave->xstate_bv |= FEATURE_AVX;
	}

	// Set up AVX-512 state
	if (ctx->xstate_.simd.zmm_filled) {
		constexpr size_t ZmmHi256StateOffset = 1152; // Upper 256 bits of ZMM0-ZMM15
		constexpr size_t Hi16ZmmStateOffset  = 1664; // Full 512 bits of ZMM16-ZMM31
		constexpr size_t AvxRegisterSize     = 32;
		constexpr size_t ZmmRegisterSize     = 64;
		constexpr size_t ZmmUpperSize        = ZmmRegisterSize - AvxRegisterSize;

		uint8_t *xsave_data = reinterpret_cast<uint8_t *>(xsave);

		// Copy upper 256 bits of ZMM0-ZMM15
		uint8_t *zmm_upper_data = xsave_data + ZmmHi256StateOffset;
		for (size_t n = 0; n < 16; ++n) {
			std::memcpy(zmm_upper_data + ZmmUpperSize * n, ctx->xstate_.simd.registers[n].data + AvxRegisterSize, ZmmUpperSize);
		}

		// Copy full 512 bits of ZMM16-ZMM31
		uint8_t *hi16_zmm_data = xsave_data + Hi16ZmmStateOffset;
		for (size_t n = 16; n < 32; ++n) {
			std::memcpy(hi16_zmm_data + ZmmRegisterSize * (n - 16), ctx->xstate_.simd.registers[n].data, ZmmRegisterSize);
		}

		// Set AVX-512 feature bits
		xsave->xstate_bv |= FEATURE_K;
		xsave->xstate_bv |= FEATURE_ZMM_H;
		xsave->xstate_bv |= FEATURE_ZMM;
	}

	// Write the xsave buffer back to the process
	struct iovec iov = {xsave, sizeof(Context_x86_64_xstate)};
	if (ptrace(PTRACE_SETREGSET, tid_, NT_X86_XSTATE, &iov) == -1) {
		throw DebuggerError("Failed to set xstate for thread %d: %s", tid_, strerror(errno));
	}
}

int Thread::set_xstate32_modern(const Context *ctx) const {
	// Prepare the xsave buffer structure for 32-bit
	auto xsave = &ctx->ctx_32_xstate_;

	// Set up basic control/status fields from x87 state
	if (ctx->xstate_.x87.filled) {
		xsave->cwd = ctx->xstate_.x87.control_word;
		xsave->swd = ctx->xstate_.x87.status_word;
		xsave->twd = ctx->xstate_.x87.tag_word;
		xsave->fop = ctx->xstate_.x87.opcode;
		xsave->fip = static_cast<uint32_t>(ctx->xstate_.x87.inst_ptr_offset);
		xsave->foo = static_cast<uint32_t>(ctx->xstate_.x87.data_ptr_offset);
		xsave->fcs = ctx->xstate_.x87.inst_ptr_selector;
		xsave->fos = ctx->xstate_.x87.data_ptr_selector;

		// Copy x87 register data
		constexpr size_t FpuRegisterCount = 8;
		constexpr size_t FpuRegisterSize  = 16;
		for (size_t n = 0; n < FpuRegisterCount; ++n) {
			// Convert our format to 32-bit st_space format
			// Each register is 16 bytes = 4 uint32_t values
			uint32_t *reg_data = &xsave->st_space[n * 4];
			std::memcpy(reg_data, ctx->xstate_.x87.registers[n].data, FpuRegisterSize);
		}

		xsave->xstate_bv |= FEATURE_X87;
	}

	// Set up SIMD state (SSE and AVX for 32-bit)
	if (ctx->xstate_.simd.sse_filled) {
		xsave->mxcsr      = ctx->xstate_.simd.mxcsr;
		xsave->mxcsr_mask = ctx->xstate_.simd.mxcsr_mask;

		// Copy first 8 XMM registers (32-bit x86 only has XMM0-XMM7)
		constexpr size_t SseRegisterSize = 16;
		for (size_t n = 0; n < 8; ++n) {
			// Convert our format to 32-bit xmm_space format
			// Each register is 16 bytes = 4 uint32_t values
			uint32_t *reg_data = &xsave->xmm_space[n * 4];
			std::memcpy(reg_data, ctx->xstate_.simd.registers[n].data, SseRegisterSize);
		}

		xsave->xstate_bv |= FEATURE_SSE;
	}

	// Set up AVX state (upper 128 bits of YMM0-YMM7)
	if (ctx->xstate_.simd.avx_filled) {
		constexpr size_t AvxRegisterSize = 32;
		constexpr size_t SseRegisterSize = 16;
		constexpr size_t AvxUpperSize    = AvxRegisterSize - SseRegisterSize; // 32 - 16 = 16 bytes

		// For 32-bit, AVX upper halves are stored in the buffer area
		// They start at the beginning of the buffer (not offset by header bytes)
		uint8_t *avx_upper_data = xsave->buffer;
		for (size_t n = 0; n < 8; ++n) {
			std::memcpy(avx_upper_data + AvxUpperSize * n, ctx->xstate_.simd.registers[n].data + SseRegisterSize, AvxUpperSize);
		}

		xsave->xstate_bv |= FEATURE_AVX;
	}

	// Use NT_X86_XSTATE for full AVX support on 32-bit
	struct iovec iov = {xsave, sizeof(Context_x86_32_xstate)};
	if (ptrace(PTRACE_SETREGSET, tid_, NT_X86_XSTATE, &iov) == -1) {
		return errno;
	}

	return 0;
}

int Thread::set_xstate32_legacy(const Context *ctx) const {
	// Legacy xstate setting for 32-bit is not implemented
	// This is a placeholder for future implementation if needed
	(void)ctx; // Suppress unused parameter warning
	return 0;
}

/**
 * @brief Sets the thread xstate (32-bit).
 *
 * @param ctx A pointer to the context object.
 */
void Thread::set_xstate32(const Context *ctx) const {

	int ret = set_xstate32_modern(ctx);
	if (ret != 0) {
		ret = set_xstate32_legacy(ctx);
		if (ret != 0) {
			throw DebuggerError("Failed to set xstate for thread %d: %s", tid_, strerror(errno));
		}
	}
}

/**
 * @brief Sets the thread debug registers (64-bit or 32-bit).
 *
 * @param ctx A pointer to the context object.
 */
void Thread::set_debug_registers64(const Context *ctx) const {
	ptrace(PTRACE_POKEUSER, tid_, offsetof(struct user, u_debugreg[0]), ctx->ctx_64_.debug_regs[0]);
	ptrace(PTRACE_POKEUSER, tid_, offsetof(struct user, u_debugreg[1]), ctx->ctx_64_.debug_regs[1]);
	ptrace(PTRACE_POKEUSER, tid_, offsetof(struct user, u_debugreg[2]), ctx->ctx_64_.debug_regs[2]);
	ptrace(PTRACE_POKEUSER, tid_, offsetof(struct user, u_debugreg[3]), ctx->ctx_64_.debug_regs[3]);
	ptrace(PTRACE_POKEUSER, tid_, offsetof(struct user, u_debugreg[4]), ctx->ctx_64_.debug_regs[4]);
	ptrace(PTRACE_POKEUSER, tid_, offsetof(struct user, u_debugreg[5]), ctx->ctx_64_.debug_regs[5]);
	ptrace(PTRACE_POKEUSER, tid_, offsetof(struct user, u_debugreg[6]), ctx->ctx_64_.debug_regs[6]);
	ptrace(PTRACE_POKEUSER, tid_, offsetof(struct user, u_debugreg[7]), ctx->ctx_64_.debug_regs[7]);
}

/**
 * @brief Sets the thread debug registers (32-bit).
 *
 * @param ctx A pointer to the context object.
 */
void Thread::set_debug_registers32(const Context *ctx) const {
	ptrace(PTRACE_POKEUSER, tid_, offsetof(struct user, u_debugreg[0]), ctx->ctx_32_.debug_regs[0]);
	ptrace(PTRACE_POKEUSER, tid_, offsetof(struct user, u_debugreg[1]), ctx->ctx_32_.debug_regs[1]);
	ptrace(PTRACE_POKEUSER, tid_, offsetof(struct user, u_debugreg[2]), ctx->ctx_32_.debug_regs[2]);
	ptrace(PTRACE_POKEUSER, tid_, offsetof(struct user, u_debugreg[3]), ctx->ctx_32_.debug_regs[3]);
	ptrace(PTRACE_POKEUSER, tid_, offsetof(struct user, u_debugreg[4]), ctx->ctx_32_.debug_regs[4]);
	ptrace(PTRACE_POKEUSER, tid_, offsetof(struct user, u_debugreg[5]), ctx->ctx_32_.debug_regs[5]);
	ptrace(PTRACE_POKEUSER, tid_, offsetof(struct user, u_debugreg[6]), ctx->ctx_32_.debug_regs[6]);
	ptrace(PTRACE_POKEUSER, tid_, offsetof(struct user, u_debugreg[7]), ctx->ctx_32_.debug_regs[7]);
}

/**
 * @brief Sets the thread GP registers.
 *
 * @param ctx A pointer to the context object.
 */
void Thread::set_registers(const Context *ctx) const {
#if defined(__x86_64__)
	set_registers64(ctx);
#elif defined(__i386__)
	set_registers32(ctx);
#endif
}

/**
 * @brief Sets the thread hardware debug registers.
 *
 * @param ctx A pointer to the context object.
 */
void Thread::set_debug_registers(const Context *ctx) const {
#ifdef __x86_64__
	set_debug_registers64(ctx);
#elif defined(__i386__)
	if (is_64_bit_) {
		// NOTE(eteran): on 32-bit systems, this doesn't work correctly
		// because the debug registers are 64-bit, but the ONLY way to
		// retrieve them is through PTRACE_PEEKUSER which only works with 32-bit
		// registers.
		printf("set_debug_registers called on 64-bit thread with 32-bit debugger\n");
	} else {
		set_debug_registers32(ctx);
	}
#endif
}

/**
 * @brief Sets the thread xstate.
 *
 * @param ctx A pointer to the context object.
 */
void Thread::set_xstate(const Context *ctx) const {
#if defined(__x86_64__)
	set_xstate64(ctx);
#elif defined(__i386__)
	set_xstate32(ctx);
#endif
}

/**
 * @brief Sets the thread context.
 *
 * @param ctx A pointer to the context object.
 */
void Thread::set_context(const Context *ctx) const {

	assert(state_ == State::Stopped);

	set_registers(ctx);
	set_xstate(ctx);
	set_debug_registers(ctx);
}

/**
 * @brief Retrieves the instruction pointer for the thread.
 *
 * @return The instruction pointer value.
 */
uint64_t Thread::get_instruction_pointer() const {
#if defined(__x86_64__)
	return static_cast<uint64_t>(ptrace(PTRACE_PEEKUSER, tid_, offsetof(struct user, regs.rip), 0L));
#elif defined(__i386__)

	// NOTE(eteran): unfortunately, PTRACE_PEEKUSER still gets 32-bit values when is_64_bit_ is true
	// so we have to use PTRACE_GETREGSET for 64-bit threads when in a 32-bit debugger.
	if (is_64_bit_) {
		Context_x86_64 ctx;
		struct iovec iov;

		iov.iov_base = &ctx;
		iov.iov_len  = sizeof(ctx);

		if (ptrace(PTRACE_GETREGSET, tid_, NT_PRSTATUS, &iov) == -1) {
			throw DebuggerError("Failed to get registers for thread %d: %s", tid_, strerror(errno));
		}

		return ctx.rip;
	}
	return static_cast<uint64_t>(ptrace(PTRACE_PEEKUSER, tid_, offsetof(struct user, regs.eip), 0L));
#endif
}

void Thread::set_instruction_pointer(uint64_t ip) const {
#if defined(__x86_64__)
	ptrace(PTRACE_POKEUSER, tid_, offsetof(struct user, regs.rip), ip);
#elif defined(__i386__)

	// NOTE(eteran): unfortunately, PTRACE_PEEKUSER still gets 32-bit values when is_64_bit_ is true
	// so we have to use PTRACE_GETREGSET for 64-bit threads when in a 32-bit debugger.
	// The "set" operation is sadly even worse than the "get" operation, because it requires
	// that we first get the registers, modify them, and then set them back.
	if (is_64_bit_) {
		Context_x86_64 ctx;
		struct iovec iov;

		iov.iov_base = &ctx;
		iov.iov_len  = sizeof(ctx);

		if (ptrace(PTRACE_GETREGSET, tid_, NT_PRSTATUS, &iov) == -1) {
			throw DebuggerError("Failed to get registers for thread %d: %s", tid_, strerror(errno));
		}

		ctx.rip = ip;

		if (ptrace(PTRACE_SETREGSET, tid_, NT_PRSTATUS, &iov) == -1) {
			throw DebuggerError("Failed to set registers for thread %d: %s", tid_, strerror(errno));
		}

		return;
	}
	ptrace(PTRACE_POKEUSER, tid_, offsetof(struct user, regs.eip), static_cast<uint32_t>(ip));
#endif
}
