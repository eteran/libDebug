
#include "Thread.hpp"
#include "Context.hpp"

#include <cassert>
#include <csignal>
#include <cstdio>
#include <cstdlib>

#include <elf.h>
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
}

/**
 * @brief Destroy the Thread object.
 */
Thread::~Thread() {
	detach();
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
 * @brief Retrieves the thread context.
 *
 * @param ctx A pointer to the context object.
 */
void Thread::get_context(Context *ctx) const {

	assert(state_ == State::Stopped);

	alignas(Context::BufferAlign) char buffer[Context::BufferAlign];

	struct iovec iov = {buffer, sizeof(buffer)};
	if (ptrace(PTRACE_GETREGSET, tid_, NT_PRSTATUS, &iov) == -1) {
		std::perror("ptrace(PTRACE_GETREGSET)");
		std::exit(0);
	}

	ctx->fill_from(buffer, iov.iov_len);

	// TODO(eteran): FPU
	// TODO(eteran): Debug registers
	// TODO(eteran): SSE/SSE2/etc...
}

/**
 * @brief Sets the thread context.
 *
 * @param ctx A pointer to the context object.
 */
void Thread::set_context(const Context *ctx) const {

	assert(state_ == State::Stopped);

	alignas(Context::BufferAlign) char buffer[Context::BufferSize];

	ctx->store_to(buffer, sizeof(buffer));

	struct iovec iov = {buffer, ctx->type()};
	if (ptrace(PTRACE_SETREGSET, tid_, NT_PRSTATUS, &iov) == -1) {
		std::perror("ptrace(PTRACE_SETREGSET)");
		std::exit(0);
	}

	// TODO(eteran): FPU
	// TODO(eteran): Debug registers
	// TODO(eteran): SSE/SSE2/etc...
}
