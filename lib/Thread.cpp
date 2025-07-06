
#include "Debug/Thread.hpp"
#include "Debug/Context.hpp"

#include <cassert>
#include <cinttypes>
#include <csignal>
#include <cstdio>
#include <cstdlib>

#include <asm/ldt.h>
#include <elf.h>
#include <sys/procfs.h>
#include <sys/ptrace.h>
#include <sys/uio.h>
#include <sys/user.h>
#include <sys/wait.h>

#define IGNORE(x) (void)x

constexpr long TraceOptions = PTRACE_O_TRACECLONE |
							  PTRACE_O_TRACEFORK |
							  PTRACE_O_TRACEEXIT;

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
			std::perror("ptrace(PTRACE_ATTACH)");
			std::exit(0);
		}
	}

	wait();

	long options = TraceOptions;
	if (f & KillOnTracerExit) {
		options |= PTRACE_O_EXITKILL;
	}

	ptrace(PTRACE_SETOPTIONS, tid, 0L, options);

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
		std::perror("ptrace(PTRACE_GETREGSET)");
		std::exit(0);
	}

	switch (iov.iov_len) {
	case sizeof(Context_x86_32):
		return false;
	case sizeof(Context_x86_64):
		return true;
	default:
		std::printf("Unknown iov_len: %zu\n", iov.iov_len);
		std::abort();
	}
}

/**
 * @brief Waits for an event on this thread.
 */
void Thread::wait() {

	assert(state_ == State::Running);

	if (waitpid(tid_, &wstatus_, __WALL) == -1) {
		std::perror("waitpid(Thread::wait)");
		std::exit(0);
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
		std::perror("ptrace(PTRACE_SINGLESTEP)");
		std::exit(0);
	}

	state_ = State::Running;
}

/**
 * @brief Causes the thread to resume execution.
 */
void Thread::resume() {

	assert(state_ == State::Stopped);

	if (ptrace(PTRACE_CONT, tid_, 0L, 0L) == -1) {
		std::perror("ptrace(PTRACE_CONT)");
		std::exit(0);
	}

	state_ = State::Running;
}

/**
 * @brief Causes a running thread the stop execution. This will be
 * eventually followed by a debug event when it actually stops.
 */
void Thread::stop() const {

	assert(state_ == State::Running);

	if (tgkill(pid_, tid_, SIGSTOP) == -1) {
		std::perror("tgkill");
		std::exit(0);
	}
}

/**
 * @brief Terminates this thread.
 */
void Thread::kill() const {
	assert(state_ == State::Running);

	if (tgkill(pid_, tid_, SIGKILL) == -1) {
		std::perror("tgkill");
		std::exit(0);
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
#else
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
	if (ptrace(PTRACE_GETREGS, tid_, 0, &ctx->ctx_64_.regs) == -1) {
		std::perror("ptrace(PTRACE_GETREGS)");
		std::exit(0);
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
		std::perror("ptrace(PTRACE_GETREGSET)");
		std::exit(0);
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
#else
	get_xstate32(ctx);
#endif
}

/**
 * @brief Retrieves the thread xstate (64-bit).
 *
 * @param ctx A pointer to the context object.
 */
void Thread::get_xstate64(Context *ctx) const {

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
#if 0
	// Possible sizes of X86_XSTATE
	static constexpr size_t BNDREGS_SIZE           = 1024;
	static constexpr size_t BNDCFG_SIZE            = 1088;
	static constexpr size_t AVX512_SIZE            = 2688;
	static constexpr size_t MAX_SIZE               = 2688;
#endif

	Context_x86_64_xstate xsave;
	struct iovec iov = {&xsave, sizeof(xsave)};
	if (ptrace(PTRACE_GETREGSET, tid_, NT_X86_XSTATE, &iov) == -1) {
		std::perror("ptrace(PTRACE_GETREGSET)");
		std::exit(0);
	}

	bool x87_present = xsave.xstate_bv & FEATURE_X87;
	bool sse_present = xsave.xstate_bv & FEATURE_SSE;
	bool avx_present = xsave.xstate_bv & FEATURE_AVX;

	// Due to the lazy saving the feature bits may be unset in XSTATE_BV if the app
	// has not touched the corresponding registers yet. But once the registers are
	// touched, they are initialized to zero by the OS (not control/tag ones). To the app
	// it looks as if the registers have always been zero. Thus we should provide the same
	// illusion to the user.
	constexpr size_t FpuRegisterCount = 8;
	constexpr size_t FpuRegisterSize  = 16;
	constexpr size_t SseRegisterCount = 16; // SSE structure has 16 registers (XMM0-XMM15)
	constexpr size_t SseRegisterSize  = 16; // Each XMM register is 128 bits = 16 bytes
	constexpr size_t AvxRegisterSize  = 32; // Each YMM register is 256 bits = 32 bytes

	if (x87_present) {
		ctx->xstate_.x87.status_word = xsave.swd; // should be first for RIndexToSTIndex() to work

		for (size_t n = 0; n < FpuRegisterCount; ++n) {
			std::memcpy(ctx->xstate_.x87.registers[n].data, xsave.st_space + FpuRegisterSize * xsave.st_space[n * FpuRegisterSize], FpuRegisterSize);
		}

		ctx->xstate_.x87.control_word      = xsave.cwd;
		ctx->xstate_.x87.tag_word          = xsave.ftw;
		ctx->xstate_.x87.inst_ptr_offset   = xsave.rip;
		ctx->xstate_.x87.data_ptr_offset   = xsave.rdp;
		ctx->xstate_.x87.inst_ptr_selector = 0;
		ctx->xstate_.x87.data_ptr_selector = 0;
		ctx->xstate_.x87.opcode            = xsave.fop;
		ctx->xstate_.x87.filled            = true;
	} else {

		for (size_t n = 0; n < FpuRegisterCount; ++n) {
			std::memset(ctx->xstate_.x87.registers[n].data, 0, FpuRegisterSize);
		}

		ctx->xstate_.x87              = {};
		ctx->xstate_.x87.control_word = xsave.cwd; // this appears always present
		ctx->xstate_.x87.tag_word     = 0xffff;
		ctx->xstate_.x87.filled       = true;
	}

	if (sse_present) {
		printf("SSE present\n");
		ctx->xstate_.avx_sse.mxcsr      = xsave.mxcsr;
		ctx->xstate_.avx_sse.mxcsr_mask = xsave.mxcr_mask;

		for (size_t n = 0; n < SseRegisterCount; ++n) {
			// Copy the 128-bit XMM register data to the first 16 bytes of the AvxRegister
			std::memcpy(ctx->xstate_.avx_sse.registers[n].data,
						xsave.xmm_space + SseRegisterSize * n,
						SseRegisterSize);
			// Clear the upper 240 bytes since SSE only uses the lower 128 bits
			std::memset(ctx->xstate_.avx_sse.registers[n].data + SseRegisterSize, 0,
						sizeof(ctx->xstate_.avx_sse.registers[n].data) - SseRegisterSize);
		}

		ctx->xstate_.avx_sse.sse_filled = true;
	} else {
		// SSE not present, initialize with zeros
		ctx->xstate_.avx_sse.mxcsr      = 0x1f80; // Default MXCSR value
		ctx->xstate_.avx_sse.mxcsr_mask = 0;

		for (size_t n = 0; n < SseRegisterCount; ++n) {
			std::memset(ctx->xstate_.avx_sse.registers[n].data, 0,
						sizeof(ctx->xstate_.avx_sse.registers[n].data));
		}

		ctx->xstate_.avx_sse.sse_filled = true;
	}

	if (avx_present) {
		// AVX extends the XMM registers to YMM registers (256 bits)
		// The lower 128 bits are already populated by SSE above
		// We need to populate the upper 128 bits from the AVX extended state

		// AVX state starts after the legacy area (576 bytes) in the xsave buffer
		// Each YMM register's upper 128 bits are stored sequentially
		constexpr size_t AvxStateOffset = 576;
		constexpr size_t AvxUpperSize   = AvxRegisterSize - SseRegisterSize;

		auto avx_upper_data = reinterpret_cast<const uint8_t *>(&xsave) + AvxStateOffset;

		for (size_t n = 0; n < SseRegisterCount; ++n) {
			// Copy the upper 128 bits (bytes 16-31) of each YMM register
			std::memcpy(ctx->xstate_.avx_sse.registers[n].data + SseRegisterSize, avx_upper_data + AvxUpperSize * n, AvxUpperSize);
			// Clear the remaining 224 bytes (bytes 32-255)
			std::memset(ctx->xstate_.avx_sse.registers[n].data + SseRegisterSize + AvxUpperSize, 0, sizeof(ctx->xstate_.avx_sse.registers[n].data) - SseRegisterSize - AvxUpperSize);
		}

		ctx->xstate_.avx_sse.avx_filled = true;
	}
}

/**
 * @brief Retrieves the thread xstate (32-bit).
 *
 * @param ctx A pointer to the context object.
 */
void Thread::get_xstate32(Context *ctx) const {
	(void)ctx;

	// TODO(eteran): implement
	// x86-64: 2688 bytes
	// x86-32: ?

	alignas(256) char xstate[4096];
	struct iovec iov = {&xstate, sizeof(xstate)};
	if (ptrace(PTRACE_GETREGSET, tid_, NT_PRFPREG, &iov) == -1) {
		std::perror("ptrace(PTRACE_GETREGSET)");
		std::exit(0);
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
	get_debug_registers32(ctx);
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
	if (ctx->is_64_bit()) {
		ctx->ctx_64_.debug_regs[0] = static_cast<uint64_t>(ptrace(PTRACE_PEEKUSER, tid_, offsetof(struct user, u_debugreg[0]), 0L));
		ctx->ctx_64_.debug_regs[1] = static_cast<uint64_t>(ptrace(PTRACE_PEEKUSER, tid_, offsetof(struct user, u_debugreg[1]), 0L));
		ctx->ctx_64_.debug_regs[2] = static_cast<uint64_t>(ptrace(PTRACE_PEEKUSER, tid_, offsetof(struct user, u_debugreg[2]), 0L));
		ctx->ctx_64_.debug_regs[3] = static_cast<uint64_t>(ptrace(PTRACE_PEEKUSER, tid_, offsetof(struct user, u_debugreg[3]), 0L));
		ctx->ctx_64_.debug_regs[4] = static_cast<uint64_t>(ptrace(PTRACE_PEEKUSER, tid_, offsetof(struct user, u_debugreg[4]), 0L));
		ctx->ctx_64_.debug_regs[5] = static_cast<uint64_t>(ptrace(PTRACE_PEEKUSER, tid_, offsetof(struct user, u_debugreg[5]), 0L));
		ctx->ctx_64_.debug_regs[6] = static_cast<uint64_t>(ptrace(PTRACE_PEEKUSER, tid_, offsetof(struct user, u_debugreg[6]), 0L));
		ctx->ctx_64_.debug_regs[7] = static_cast<uint64_t>(ptrace(PTRACE_PEEKUSER, tid_, offsetof(struct user, u_debugreg[7]), 0L));
	} else {
		ctx->ctx_32_.debug_regs[0] = static_cast<uint32_t>(ptrace(PTRACE_PEEKUSER, tid_, offsetof(struct user, u_debugreg[0]), 0L));
		ctx->ctx_32_.debug_regs[1] = static_cast<uint32_t>(ptrace(PTRACE_PEEKUSER, tid_, offsetof(struct user, u_debugreg[1]), 0L));
		ctx->ctx_32_.debug_regs[2] = static_cast<uint32_t>(ptrace(PTRACE_PEEKUSER, tid_, offsetof(struct user, u_debugreg[2]), 0L));
		ctx->ctx_32_.debug_regs[3] = static_cast<uint32_t>(ptrace(PTRACE_PEEKUSER, tid_, offsetof(struct user, u_debugreg[3]), 0L));
		ctx->ctx_32_.debug_regs[4] = static_cast<uint32_t>(ptrace(PTRACE_PEEKUSER, tid_, offsetof(struct user, u_debugreg[4]), 0L));
		ctx->ctx_32_.debug_regs[5] = static_cast<uint32_t>(ptrace(PTRACE_PEEKUSER, tid_, offsetof(struct user, u_debugreg[5]), 0L));
		ctx->ctx_32_.debug_regs[6] = static_cast<uint32_t>(ptrace(PTRACE_PEEKUSER, tid_, offsetof(struct user, u_debugreg[6]), 0L));
		ctx->ctx_32_.debug_regs[7] = static_cast<uint32_t>(ptrace(PTRACE_PEEKUSER, tid_, offsetof(struct user, u_debugreg[7]), 0L));
	}
}

/**
 * @brief Get a specific segment base for a given segment register.
 *
 * @param ctx A pointer to the context object.
 * @param reg The register to get the segment base for.
 * @return The segment base.
 */
uint32_t Thread::get_segment_base(Context *ctx, RegisterId reg) const {

	// TODO(eteran): this is too arch specific, move to ContextIntel
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
#ifndef __x86_64__
	// TODO(eteran): this is too arch specific, move to ContextIntel
	if (!ctx->is_64_bit()) {
		ctx->ctx_32_.gs_base = get_segment_base(ctx, RegisterId::GS);
		ctx->ctx_32_.fs_base = get_segment_base(ctx, RegisterId::FS);
	}
#endif
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
 * @brief Sets the thread GP registers.
 *
 * @param ctx A pointer to the context object.
 */
void Thread::set_registers(const Context *ctx) const {

#if defined(__x86_64__)
	if (ptrace(PTRACE_SETREGS, tid_, 0, &ctx->ctx_64_.regs) == -1) {
		std::perror("ptrace(PTRACE_SETREGS)");
		std::exit(0);
	}
#else
	struct iovec iov;
	if (is_64_bit_) {
		iov.iov_base = const_cast<Context_x86_64 *>(&ctx->ctx_64_.regs);
		iov.iov_len  = sizeof(ctx->ctx_64_.regs);

	} else {
		iov.iov_base = const_cast<Context_x86_32 *>(&ctx->ctx_32_.regs);
		iov.iov_len  = sizeof(ctx->ctx_32_.regs);
	}

	if (ptrace(PTRACE_SETREGSET, tid_, NT_PRSTATUS, &iov) == -1) {
		std::perror("ptrace(PTRACE_SETREGSET)");
		std::exit(0);
	}
#endif
}

/**
 * @brief Sets the thread hardware debug registers.
 *
 * @param ctx A pointer to the context object.
 */
void Thread::set_debug_registers(const Context *ctx) const {
	// TODO(eteran): implement
	(void)ctx;
}

/**
 * @brief Sets the thread xstate.
 *
 * @param ctx A pointer to the context object.
 */
void Thread::set_xstate(const Context *ctx) const {

	// TODO(eteran): implement
	(void)ctx;
}

/**
 * @brief Set the segment bases
 *
 * @param ctx A pointer to the context object.
 */
void Thread::set_segment_bases(const Context *ctx) const {
	// TODO(eteran): implement
	(void)ctx;
}

/**
 * @brief Set a specific segment base for a given segment register.
 *
 * @param ctx A pointer to the context object.
 * @param reg The register to set the segment base for.
 * @param base The segment base.
 */
void Thread::set_segment_base(const Context *ctx, RegisterId reg, uint32_t base) {
	// TODO(eteran): implement
	(void)ctx;
	(void)reg;
	(void)base;
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
	set_segment_bases(ctx);
}
