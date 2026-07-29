
#include "Debug/ThreadArm.hpp"
#include "Debug/Breakpoint.hpp"
#include "Debug/Context.hpp"
#include "Debug/Debugger.hpp"
#include "Debug/DebuggerError.hpp"
#include "Debug/Process.hpp"
#include "Debug/Ptrace.hpp"

#include <cassert>
#include <cinttypes>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <elf.h>
#include <sys/procfs.h>
#include <sys/ptrace.h>
#include <sys/syscall.h>
#include <sys/uio.h>
#include <sys/user.h>
#include <sys/wait.h>
#include <unistd.h>

namespace {

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
		// NOTE(eteran): we intentionally DO NOT try to catch or report errors from
		// ptrace detach because we want to make a best effort to detach even if the
		// thread has already exited or is in some other weird state.
		// The destructor should never throw, and we don't want to leak resources
		// just because the thread is in a bad state.
		(void)do_ptrace(PTRACE_DETACH, tid_, 0L, 0L);
		tid_ = -1;
	}
}

/**
 * @brief Loads the signal info for this thread into the `siginfo_` member.
 * This should be called when handling a signal event to get more information
 * about the signal that caused the event.
 *
 * @return true if the signal info was successfully loaded, false otherwise.
 */
bool Thread::load_signal_info() {
	if (auto ret = do_ptrace(PTRACE_GETSIGINFO, tid_, 0L, &siginfo_); ret.is_err()) {
		siginfo_ = {};
		return false;
	}
	return true;
}

/**
 * @brief Causes the thread to step one instruction. This will be
 * eventually followed by a debug event when it stops again.
 */
void Thread::step() {
	step(0);
}

/**
 * @brief Causes the thread to step one instruction. This will be
 * eventually followed by a debug event when it stops again.
 *
 * @param signal The signal to deliver to the thread after stepping. If 0, no signal is delivered.
 */
void Thread::step(int signal) {
	assert(state_ == State::Stopped);
	pending_signal_ = 0;
	pending_step_breakpoint_.reset();

	if (auto bp = process_->find_breakpoint(get_instruction_pointer()); bp && bp->enabled()) {
		bp->disable();
		pending_step_breakpoint_ = bp->address();
	}

	// TODO(eteran): if we are sitting on a pushfd/pushf/pushfq instruction, we need to emulate the instruction
	// So that we can filter out the TF flag. After which we can add a pending event to the process so it looks
	// like a normal single step event. This is because the TF flag is SET by the kernel when we single step,
	// and the pushfd/pushf/pushfq instructions will push the TF flag onto the stack, which is not what we want.
	if (auto ret = do_ptrace(PTRACE_SINGLESTEP, tid_, 0L, signal); ret.is_err()) {
		if (pending_step_breakpoint_) {
			if (auto bp = process_->find_breakpoint(*pending_step_breakpoint_); bp) {
				bp->enable();
			}
			pending_step_breakpoint_.reset();
		}
		throw DebuggerError("Failed to step thread %d: %s", tid_, strerror(ret.error()));
	}

	state_ = State::Running;
}

/**
 * @brief Causes the thread to resume execution.
 *
 * @param signal The signal to deliver to the thread when resuming. If 0, no signal is delivered.
 */
void Thread::resume(int signal) {
	resume_internal(signal, false);
}

/**
 * @brief Causes the thread to resume execution.
 */
void Thread::resume() {
	resume(0);
}

/**
 * @brief Causes the thread to resume execution until it enters or exits a system call.
 */
void Thread::resume_until_syscall() {
	resume_until_syscall(0);
}

/**
 * @brief Causes the thread to resume execution until it enters or exits a system call.
 *
 * @param signal The signal to deliver to the thread when resuming. If 0, no signal is delivered.
 */
void Thread::resume_until_syscall(int signal) {
	resume_internal(signal, true);
}

/**
 * @brief Internal helper function to resume the thread with a signal and optionally until a syscall.
 *
 * @param signal The signal to deliver to the thread when resuming. If 0, no signal is delivered.
 * @param until_syscall If true, the thread will resume until it enters or exits a system call. If false, it will resume normally.
 */
void Thread::resume_internal(int signal, bool until_syscall) {

	assert(state_ == State::Stopped);
	pending_signal_ = 0;
	pending_step_breakpoint_.reset();

	const int request = until_syscall ? PTRACE_SYSCALL : PTRACE_CONT;

	if (auto bp = process_->find_breakpoint(get_instruction_pointer()); bp && bp->enabled()) {
		bp->disable();

		// TODO(eteran): if we are sitting on a pushfd/pushf/pushfq instruction, we need to emulate the instruction
		// So that we can filter out the TF flag. After which we can add a pending event to the process so it looks
		// like a normal single step event. This is because the TF flag is SET by the kernel when we single step,
		// and the pushfd/pushf/pushfq instructions will push the TF flag onto the stack, which is not what we want.
		if (auto ret = do_ptrace(PTRACE_SINGLESTEP, tid_, 0L, signal); ret.is_err()) {
			bp->enable();
			throw DebuggerError("Failed to step thread %d: %s", tid_, strerror(ret.error()));
		}

		state_ = State::Running;

		// NOTE(eteran): We need to check if the wait resulted in a single step event or some other event
		// (e.g. breakpoint hit by another thread) and handle that accordingly instead of assuming it was
		// a single step event. if it wasn't a single step event, we need continue, but pass the signal
		// through so that the thread stops again at with the event during the `next_debug_event` loop.
		wait();

		bp->enable();

		// If the wait did not result in the single-step we expected, forward the
		// event (signal/exit) back to the event loop by continuing with the
		// appropriate signal and returning without re-enabling the breakpoint.
		if (!is_stopped() || stop_status() != SIGTRAP) {
			// If the thread exited or was signaled, nothing to continue here.
			if (is_exited() || is_signaled()) {
				return;
			}

			int cont_sig = signal;
			if (cont_sig == 0 && load_signal_info()) {
				cont_sig = siginfo_.si_signo;
			}

			if (auto ret = do_ptrace(request, tid_, 0L, cont_sig); ret.is_err()) {
				throw DebuggerError("Failed to continue thread %d: %s", tid_, strerror(ret.error()));
			}

			state_ = State::Running;
			return;
		}
	}

	if (auto ret = do_ptrace(request, tid_, 0L, signal); ret.is_err()) {
		throw DebuggerError("Failed to continue thread %d: %s", tid_, strerror(ret.error()));
	}

	state_ = State::Running;
}

/**
 * @brief Causes a running thread the stop execution. This will be
 * eventually followed by a debug event when it actually stops.
 */
void Thread::stop() const {
	assert(state_ == State::Running);

	if (syscall(SYS_tgkill, process_->pid(), tid_, SIGSTOP) == -1) {
		throw DebuggerError("Failed to stop thread %d: %s", tid_, strerror(errno));
	}
}

/**
 * @brief Terminates this thread.
 */
void Thread::kill() const {
	assert(state_ == State::Running);

	if (syscall(SYS_tgkill, process_->pid(), tid_, SIGKILL) == -1) {
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

#if defined(NT_ARM_VFP)
	struct iovec vfp_iov = {&ctx->vfp_regs_, sizeof(ctx->vfp_regs_)};
	if (auto ret = do_ptrace(PTRACE_GETREGSET, tid_, NT_ARM_VFP, &vfp_iov); ret.is_err()) {
		std::memset(&ctx->vfp_regs_, 0, sizeof(ctx->vfp_regs_));
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
