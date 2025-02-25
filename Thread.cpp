
#include "Thread.hpp"

#include <cstdio>
#include <cstdlib>

#include <signal.h>
#include <sys/ptrace.h>
#include <sys/wait.h>

/**
 * @brief Construct a new Thread object
 *
 * @param tid
 */
Thread::Thread(pid_t tid)
	: tid_(tid) {
}

/**
 * @brief
 *
 */
void Thread::step() {
	long ret = ::ptrace(PTRACE_SINGLESTEP, tid_, 0L, 0L);
	if (ret == -1) {
		perror("ptrace");
		exit(0);
	}
}

/**
 * @brief
 *
 */
void Thread::resume() {
	long ret = ::ptrace(PTRACE_CONT, tid_, 0L, 0L);
	if (ret == -1) {
		perror("ptrace");
		exit(0);
	}
}

/**
 * @brief
 *
 */
void Thread::stop() {
	long ret = ::kill(tid_, SIGSTOP);
	if (ret == -1) {
		perror("ptrace");
		exit(0);
	}
}

/**
 * @brief
 *
 */
void Thread::kill() {
	long ret = ::ptrace(PTRACE_KILL, tid_, 0L, 0L);
	if (ret == -1) {
		perror("ptrace");
		exit(0);
	}
}

/**
 * @brief
 *
 */
void Thread::detach() {
	long ret = ::ptrace(PTRACE_DETACH, tid_, 0L, 0L);
	if (ret == -1) {
		perror("ptrace");
		exit(0);
	}
}

/**
 * @brief
 *
 */
void Thread::wait() {

	int wstatus = 0;

	long ret = ::waitpid(tid_, &wstatus, __WALL);
	if (ret == -1) {
		perror("waitpid");
		exit(0);
	}

	wstatus_ = wstatus;
}

/**
 * @brief
 *
 * @return true
 * @return false
 */
bool Thread::is_exited() const {
	return WIFEXITED(wstatus_);
}

/**
 * @brief
 *
 * @return true
 * @return false
 */
bool Thread::is_signaled() const {
	return WIFSIGNALED(wstatus_);
}

/**
 * @brief
 *
 * @return true
 * @return false
 */
bool Thread::is_stopped() const {
	return WIFSTOPPED(wstatus_);
}

/**
 * @brief
 *
 * @return true
 * @return false
 */
bool Thread::is_continued() const {
	return WIFCONTINUED(wstatus_);
}

/**
 * @brief
 *
 * @return int
 */
int Thread::exit_status() const {
	return WEXITSTATUS(wstatus_);
}

/**
 * @brief
 *
 * @return int
 */
int Thread::signal_status() const {
	return WTERMSIG(wstatus_);
}

/**
 * @brief
 *
 * @return int
 */
int Thread::stop_status() const {
	return WSTOPSIG(wstatus_);
}
