
#include "Debug/ThreadArm.hpp"
#include "Debug/Context.hpp"
#include "Debug/Debugger.hpp"
#include "Debug/DebuggerError.hpp"
#include "Debug/Ptrace.hpp"

#include <cstddef>

#include <elf.h>
#include <sys/uio.h>
#include <sys/user.h>

/**
 * @brief Construct a new Thread object.
 *
 * @param tid The thread id to attach to.
 * @param f Controls the attach behavior of this constructor.
 */
Thread::Thread(const internal_t &, Process *process, pid_t tid, Flag f)
	: process_(process), tid_(tid) {

	assert(process);

	if (f & Thread::Attach) {
		if (auto ret = do_ptrace(PTRACE_ATTACH, tid, 0L, 0L); ret.is_err()) {
			throw DebuggerError("Failed to attach to thread %d: %s", tid, strerror(ret.error()));
		}
	}

	wait();

	const long options = create_ptrace_options(f);
	if (auto ret = do_ptrace(PTRACE_SETOPTIONS, tid, 0L, options); ret.is_err()) {
		throw DebuggerError("Failed to set ptrace options for thread %d: %s", tid, strerror(ret.error()));
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
	if (auto ret = do_ptrace(PTRACE_GETREGSET, tid_, NT_PRSTATUS, &iov); ret.is_err()) {
		throw DebuggerError("Failed to get register set for thread %d: %s", tid_, strerror(ret.error()));
	}

	switch (iov.iov_len) {
	case sizeof(Context_Arm_32):
		return false;
	case sizeof(Context_Arm_64):
		return true;
	default:
		throw DebuggerError("Unknown iov_len: %zu", iov.iov_len);
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

	struct iovec iov;
	if (is_64_bit_) {
		iov.iov_base = &ctx->ctx_64_.regs;
		iov.iov_len  = sizeof(ctx->ctx_64_.regs);
	} else {
		iov.iov_base = &ctx->ctx_32_.regs;
		iov.iov_len  = sizeof(ctx->ctx_32_.regs);
	}

	if (auto ret = do_ptrace(PTRACE_GETREGSET, tid_, NT_PRSTATUS, &iov); ret.is_err()) {
		throw DebuggerError("Failed to get registers for thread %d: %s", tid_, strerror(ret.error()));
	}

	std::memset(&ctx->vfp_regs_, 0, sizeof(ctx->vfp_regs_));
	ctx->vfp_filled_ = false;

#if defined(NT_ARM_VFP)
	struct iovec vfp_iov = {&ctx->vfp_regs_, sizeof(ctx->vfp_regs_)};
	if (auto ret = do_ptrace(PTRACE_GETREGSET, tid_, NT_ARM_VFP, &vfp_iov); ret.ok() && vfp_iov.iov_len == sizeof(ctx->vfp_regs_)) {
		ctx->vfp_filled_ = true;
	}
#endif
}

/**
 * @brief Sets the thread context.
 *
 * @param ctx A pointer to the context object.
 */
void Thread::set_context(const Context *ctx) const {

	assert(state_ == State::Stopped);

	struct iovec iov;
	if (is_64_bit_) {
		iov.iov_base = const_cast<Context_Arm_64 *>(&ctx->ctx_64_.regs);
		iov.iov_len  = sizeof(ctx->ctx_64_.regs);
	} else {
		iov.iov_base = const_cast<Context_Arm_32 *>(&ctx->ctx_32_.regs);
		iov.iov_len  = sizeof(ctx->ctx_32_.regs);
	}

	if (auto ret = do_ptrace(PTRACE_SETREGSET, tid_, NT_PRSTATUS, &iov); ret.is_err()) {
		throw DebuggerError("Failed to set registers for thread %d: %s", tid_, strerror(ret.error()));
	}

#if defined(NT_ARM_VFP)
	struct iovec vfp_iov = {const_cast<Context_Arm_Vfp *>(&ctx->vfp_regs_), sizeof(ctx->vfp_regs_)};
	if (auto ret = do_ptrace(PTRACE_SETREGSET, tid_, NT_ARM_VFP, &vfp_iov); ret.is_err()) {
		throw DebuggerError("Failed to set VFP registers for thread %d: %s", tid_, strerror(ret.error()));
	}
#endif
}

/**
 * @brief Retrieves the instruction pointer for the thread.
 *
 * @return The instruction pointer value.
 */
uint64_t Thread::get_instruction_pointer() const {
	Context ctx;
	get_context(&ctx);

	if (is_64_bit_) {
		return ctx.get(RegisterId::XPC).as<uint64_t>();
	}

	return ctx.get(RegisterId::PC).as<uint32_t>();
}

/**
 * @brief Sets the instruction pointer for the thread.
 *
 * @param ip The instruction pointer value.
 */
void Thread::set_instruction_pointer(uint64_t ip) const {
	Context ctx;
	get_context(&ctx);

	if (is_64_bit_) {
		ctx.get(RegisterId::XPC) = ip;
	} else {
		ctx.get(RegisterId::PC) = static_cast<uint32_t>(ip);
	}

	set_context(&ctx);
}
