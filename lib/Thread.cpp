
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
		uint32_t gs_base = get_segment_base(ctx, RegisterId::GS);
		uint32_t fs_base = get_segment_base(ctx, RegisterId::FS);

		ctx->ctx_32_.gs_base = gs_base;
		ctx->ctx_32_.fs_base = fs_base;
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
