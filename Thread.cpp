
#include "Thread.hpp"

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
 * @brief Construct a new Thread object
 *
 * @param tid
 */
Thread::Thread(pid_t pid, pid_t tid, Flag f)
	: pid_(pid), tid_(tid) {

	if (f & Thread::Attach) {
		long ret = ::ptrace(PTRACE_ATTACH, tid, 0L, 0L);
		if (ret == -1) {
			::perror("ptrace(PTRACE_ATTACH)");
			::exit(0);
		}
	}

	wait();

	long options = TraceOptions;
	if (f & KillOnTracerExit) {
		options |= PTRACE_O_EXITKILL;
	}

	::ptrace(PTRACE_SETOPTIONS, tid, 0L, options);
}

/**
 * @brief Destroy the Thread object
 *
 */
Thread::~Thread() {
	detach();
}

/**
 * @brief waits for an event on this thread
 *
 */
void Thread::wait() {

	assert(state_ == State::Running);

	long ret = ::waitpid(tid_, &wstatus_, __WALL);
	if (ret == -1) {
		::perror("waitpid(Thread::wait)");
		::exit(0);
	}

	state_ = State::Stopped;
}

/**
 * @brief detaches from the associated thread, if any
 * no-op if already detached
 *
 */
void Thread::detach() {
	if (tid_ != -1) {
		long ret = ::ptrace(PTRACE_DETACH, tid_, 0L, 0L);
		IGNORE(ret);
		tid_ = -1;
	}
}

/**
 * @brief causes the thread to step one instruction. This will be
 * eventually followed by a debug event when it stops again
 *
 */
void Thread::step() {

	assert(state_ == State::Stopped);

	long ret = ::ptrace(PTRACE_SINGLESTEP, tid_, 0L, 0L);
	if (ret == -1) {
		::perror("ptrace(PTRACE_SINGLESTEP)");
		::exit(0);
	}

	state_ = State::Running;
}

/**
 * @brief causes the thread to resume execution
 *
 */
void Thread::resume() {

	assert(state_ == State::Stopped);

	long ret = ::ptrace(PTRACE_CONT, tid_, 0L, 0L);
	if (ret == -1) {
		::perror("ptrace(PTRACE_CONT)");
		::exit(0);
	}

	state_ = State::Running;
}

/**
 * @brief causes a running thread the stop execution. This will be
 * eventually followed by a debug event when it actually stops
 *
 */
void Thread::stop() {

	assert(state_ == State::Running);

	long ret = ::tgkill(pid_, tid_, SIGSTOP);
	if (ret == -1) {
		::perror("tgkill");
		::exit(0);
	}
}

/**
 * @brief terminates this thread
 *
 */
void Thread::kill() {
	assert(state_ == State::Running);

	long ret = ::tgkill(pid_, tid_, SIGKILL);
	if (ret == -1) {
		::perror("tgkill");
		::exit(0);
	}
}

/**
 * @brief
 *
 * @return bool
 */
bool Thread::is_exited() const {
	assert(state_ == State::Stopped);
	return WIFEXITED(wstatus_);
}

/**
 * @brief
 *
 * @return bool
 */
bool Thread::is_signaled() const {
	assert(state_ == State::Stopped);
	return WIFSIGNALED(wstatus_);
}

/**
 * @brief
 *
 * @return bool
 */
bool Thread::is_stopped() const {
	assert(state_ == State::Stopped);
	return WIFSTOPPED(wstatus_);
}

/**
 * @brief
 *
 * @return bool
 */
bool Thread::is_continued() const {
	assert(state_ == State::Stopped);
	return WIFCONTINUED(wstatus_);
}

/**
 * @brief
 *
 * @return int
 */
int Thread::exit_status() const {
	assert(state_ == State::Stopped);
	return WEXITSTATUS(wstatus_);
}

/**
 * @brief
 *
 * @return int
 */
int Thread::signal_status() const {
	assert(state_ == State::Stopped);
	return WTERMSIG(wstatus_);
}

/**
 * @brief
 *
 * @return int
 */
int Thread::stop_status() const {
	assert(state_ == State::Stopped);
	return WSTOPSIG(wstatus_);
}

/**
 * @brief retrieves the thread context
 *
 * @param ctx a pointer to the context object
 */
void Thread::get_context(Context *ctx) const {

	assert(state_ == State::Stopped);

	constexpr size_t MaxAlign = std::max(alignof(Context_x86_64), alignof(Context_x86_32));
	constexpr size_t MaxSize  = std::max(sizeof(Context_x86_64), sizeof(Context_x86_32));
	alignas(MaxAlign) char buffer[MaxSize];

	struct iovec iov = {buffer, sizeof(buffer)};
	long ret         = ::ptrace(PTRACE_GETREGSET, tid_, NT_PRSTATUS, &iov);

	if (ret == -1) {
		::perror("ptrace(PTRACE_GETREGSET)");
		::exit(0);
	}

	fill_context(ctx, buffer, iov.iov_len);

	// TODO(eteran): FPU
	// TODO(eteran): SSE/SSE2/etc...
}

/**
 * @brief sets the thread context
 *
 * @param ctx a pointer to the context object
 */
void Thread::set_context(const Context *ctx) const {

	assert(state_ == State::Stopped);

	constexpr size_t MaxAlign = std::max(alignof(Context_x86_64), alignof(Context_x86_32));
	constexpr size_t MaxSize  = std::max(sizeof(Context_x86_64), sizeof(Context_x86_32));
	alignas(MaxAlign) char buffer[MaxSize];

	store_context(buffer, ctx, sizeof(buffer));

	struct iovec iov = {buffer, ctx->type_};

	if (ptrace(PTRACE_SETREGSET, tid_, NT_PRSTATUS, &iov) == -1) {
		::perror("ptrace(PTRACE_SETREGSET)");
		::exit(0);
	}

	// TODO(eteran): FPU
	// TODO(eteran): SSE/SSE2/etc...
}
